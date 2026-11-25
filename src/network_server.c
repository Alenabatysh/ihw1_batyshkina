
#include "network.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Maximum number of clients that can be connected to the server at once.
#define SERVER_MAX_CLIENTS 64

// Maximum size of an incoming message from a client.
#define SERVER_LINE_SIZE 2048

// Buffer size for outgoing messages waiting to be sent.
#define SERVER_OUT_SIZE 16384

// Timeout used by poll() when checking socket events.
#define SERVER_POLL_TIMEOUT_MS 500

// Defines the possible roles of connected clients.
typedef enum {
  CLIENT_UNIDENTIFIED = 0,
  CLIENT_PUBLISHER,
  CLIENT_OBSERVER
} ClientRole;

// Stores the connection state and communication buffers of one client.
typedef struct {
  int fd;
  ClientRole role;
  char input[SERVER_LINE_SIZE];
  size_t input_length;
  char output[SERVER_OUT_SIZE];
  size_t output_length;
  char label[64];
} ServerClient;

// Indicates whether the server should stop its main event loop.
static volatile sig_atomic_t server_stop = 0;

// Stores the most recently received simulation snapshot.
static char last_snapshot[SERVER_LINE_SIZE];

// Indicates whether at least one snapshot has been received.
static int have_snapshot = 0;

// Tracks the number of currently connected publishers.
static int active_publishers = 0;

// Handles termination signals and requests a graceful server shutdown.
static void server_signal_handler(int signal_number) {
  (void)signal_number;
  server_stop = 1;
}

// Enables non-blocking mode for the specified socket.
static int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Closes a client connection and releases its associated state.
// Also updates the number of active publishers when necessary.
static void close_client(ServerClient *client) {
  if (client->fd >= 0) {
    if (client->role == CLIENT_PUBLISHER && active_publishers > 0)
      active_publishers--;
    close(client->fd);
  }
  memset(client, 0, sizeof(*client));
  client->fd = -1;
}

// Adds data to the client's output buffer without blocking the event loop.
// If a slow observer falls behind, stale snapshots are discarded and only
// the newest state is retained.
static int queue_output(ServerClient *client, const char *data, size_t length) {
  if (length > sizeof(client->output))
    return -1;
  if (client->output_length + length > sizeof(client->output))
    client->output_length = 0;
  memcpy(client->output + client->output_length, data, length);
  client->output_length += length;
  return 0;
}

// Sends a new simulation snapshot to all currently connected observers.
// Publisher connections are excluded from the broadcast.
static void broadcast_snapshot(ServerClient clients[], const char *snapshot,
                               size_t length) {
  for (int i = 0; i < SERVER_MAX_CLIENTS; ++i) {
    if (clients[i].fd >= 0 && clients[i].role == CLIENT_OBSERVER) {
      if (queue_output(&clients[i], snapshot, length) != 0) {
        fprintf(stderr,
                "Server: observer output queue overflow, disconnecting\n");
        close_client(&clients[i]);
      }
    }
  }
}

// Processes one complete message received from a client.
// Identifies the client's role and handles messages according to that role.
static int process_line(ServerClient clients[], ServerClient *client,
                        char *line) {
  // A newly connected client must identify itself as a publisher or observer.
  if (client->role == CLIENT_UNIDENTIFIED) {

    // Register a publisher that sends simulation snapshots to the server.
    if (strncmp(line, "PUBLISHER ", 10) == 0 ||
        strcmp(line, "PUBLISHER") == 0) {

      // Only one publisher is allowed to be connected at a time.
      if (active_publishers > 0) {
        static const char reject[] = "ERROR publisher already connected\n";
        (void)queue_output(client, reject, sizeof(reject) - 1);
        return -1;
      }

      client->role = CLIENT_PUBLISHER;
      active_publishers++;
      snprintf(client->label, sizeof(client->label), "%s",
               (strlen(line) > 10) ? line + 10 : "publisher");
      fprintf(stderr, "Server: publisher connected (%s)\n", client->label);

      // Confirm successful publisher registration.
      static const char welcome[] = "WELCOME PUBLISHER\n";
      (void)queue_output(client, welcome, sizeof(welcome) - 1);
      return 0;
    }

    // Register an observer that receives simulation snapshots.
    if (strncmp(line, "OBSERVER", 8) == 0) {
      client->role = CLIENT_OBSERVER;
      snprintf(client->label, sizeof(client->label), "%s",
               line[8] == ' ' ? line + 9 : "observer");
      fprintf(stderr, "Server: observer connected (%s)\n", client->label);

      // Confirm successful observer registration.
      static const char welcome[] = "WELCOME OBSERVER\n";
      (void)queue_output(client, welcome, sizeof(welcome) - 1);

      // Send the latest available snapshot to a newly connected observer.
      if (have_snapshot)
        (void)queue_output(client, last_snapshot, strlen(last_snapshot));
      return 0;
    }

    // Reject clients that do not send a valid role identification message.
    static const char error[] = "ERROR send PUBLISHER or OBSERVER first\n";
    (void)queue_output(client, error, sizeof(error) - 1);
    return -1;
  }

  // Process messages received from the publisher.
  if (client->role == CLIENT_PUBLISHER) {
    size_t length = strlen(line);

    // Check the snapshot message format and make sure it fits in the buffer.
    if (strncmp(line, "S|", 2) != 0 || length >= sizeof(last_snapshot) - 1) {
      fprintf(stderr, "Server: malformed snapshot from publisher\n");
      return -1;
    }

    // Save the latest snapshot so newly connected observers can receive it.
    memcpy(last_snapshot, line, length);
    last_snapshot[length] = '\n';
    last_snapshot[length + 1] = '\0';
    have_snapshot = 1;

    // Forward the received snapshot to all connected observers.
    broadcast_snapshot(clients, last_snapshot, length + 1);
    return 0;
  }

  // Observers are receive-only and do not send simulation data to the server.
  return 0;
}

// Reads incoming data from a client socket.
// Splits the received byte stream into complete lines before processing them.
static int read_client(ServerClient clients[], ServerClient *client) {
  char chunk[1024];

  // Continue reading while data is available on the non-blocking socket.
  for (;;) {
    ssize_t count = recv(client->fd, chunk, sizeof(chunk), 0);
    if (count == 0)
      return -1;

    if (count < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return 0;
      return -1;
    }

    // Process each received character and detect message boundaries.
    for (ssize_t i = 0; i < count; ++i) {
      char ch = chunk[i];

      if (ch == '\n') {
        // Terminate the received line before passing it to the message handler.
        client->input[client->input_length] = '\0';

        // Remove a trailing carriage return if the message uses CRLF.
        if (client->input_length > 0 &&
            client->input[client->input_length - 1] == '\r')
          client->input[client->input_length - 1] = '\0';

        int status = process_line(clients, client, client->input);
        client->input_length = 0;

        if (status != 0)
          return -1;
      } else {
        // Reject messages that exceed the input buffer capacity.
        if (client->input_length + 1 >= sizeof(client->input))
          return -1;
        client->input[client->input_length++] = ch;
      }
    }
  }
}

// Sends all pending data from the client's output buffer.
// Handles partial writes and temporarily unavailable sockets.
static int flush_client(ServerClient *client) {
  while (client->output_length > 0) {
    ssize_t count =
        send(client->fd, client->output, client->output_length, MSG_NOSIGNAL);

    if (count < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return 0;
      return -1;
    }

    if (count == 0)
      return -1;

    // Remove the bytes that have already been sent from the output buffer.
    client->output_length -= (size_t)count;
    if (client->output_length > 0)
      memmove(client->output, client->output + count, client->output_length);
  }

  return 0;
}

// Creates and configures the TCP listening socket.
// The socket is bound to the specified address and port.
static int create_listener(const char *bind_host, const char *port) {
  struct addrinfo hints;
  struct addrinfo *result = NULL;
  struct addrinfo *it;
  int listener = -1;

  // Configure address resolution for a TCP server socket.
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int gai_error = getaddrinfo(bind_host, port, &hints, &result);
  if (gai_error != 0) {
    fprintf(stderr, "Server: getaddrinfo: %s\n", gai_strerror(gai_error));
    return -1;
  }

  // Try available addresses until a listening socket is created successfully.
  for (it = result; it != NULL; it = it->ai_next) {
    int reuse = 1;
    listener = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (listener < 0)
      continue;

    // Allow the server address to be reused after a previous shutdown.
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Bind the socket, start listening and enable non-blocking mode.
    if (bind(listener, it->ai_addr, it->ai_addrlen) == 0 &&
        listen(listener, SERVER_MAX_CLIENTS) == 0 &&
        set_nonblocking(listener) == 0)
      break;

    close(listener);
    listener = -1;
  }

  // Release the address information allocated by getaddrinfo().
  freeaddrinfo(result);
  return listener;
}

// Main TCP server routine.
// Accepts client connections and coordinates communication with publishers
// and observers using a non-blocking event loop.
int network_server_run(const char *bind_host, const char *port) {
  if (port == NULL)
    return 1;

  // Configure graceful shutdown when Ctrl+C or SIGTERM is received.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = server_signal_handler;
  sigemptyset(&action.sa_mask);
  (void)sigaction(SIGINT, &action, NULL);
  (void)sigaction(SIGTERM, &action, NULL);

  // Ignore SIGPIPE to prevent termination when sending to a disconnected
  // client.
  signal(SIGPIPE, SIG_IGN);

  // Create the listening socket for incoming TCP connections.
  int listener = create_listener(bind_host, port);
  if (listener < 0) {
    perror("Server: cannot create listening socket");
    return 1;
  }

  // Initialize the client table and mark all connection slots as free.
  ServerClient clients[SERVER_MAX_CLIENTS];
  for (int i = 0; i < SERVER_MAX_CLIENTS; ++i) {
    memset(&clients[i], 0, sizeof(clients[i]));
    clients[i].fd = -1;
  }

  // Reset publisher and snapshot state before starting the server.
  have_snapshot = 0;
  active_publishers = 0;

  printf("TCP monitoring server listening on %s:%s\n",
         (bind_host != NULL && bind_host[0] != '\0') ? bind_host : "*", port);
  printf("Maximum simultaneous clients: %d. Press Ctrl+C to stop.\n",
         SERVER_MAX_CLIENTS);
  fflush(stdout);

  // Main event loop: monitor the listening socket and all active clients.
  while (!server_stop) {
    struct pollfd poll_fds[SERVER_MAX_CLIENTS + 1];
    int slot_map[SERVER_MAX_CLIENTS + 1];
    nfds_t count = 1;

    // Add the listening socket to the poll() set.
    poll_fds[0].fd = listener;
    poll_fds[0].events = POLLIN;
    poll_fds[0].revents = 0;
    slot_map[0] = -1;

    // Add active client sockets and register the events they need.
    for (int i = 0; i < SERVER_MAX_CLIENTS; ++i) {
      if (clients[i].fd < 0)
        continue;

      poll_fds[count].fd = clients[i].fd;
      poll_fds[count].events = POLLIN;

      // Monitor writable events only when there is data waiting to be sent.
      if (clients[i].output_length > 0)
        poll_fds[count].events |= POLLOUT;

      poll_fds[count].revents = 0;
      slot_map[count] = i;
      count++;
    }

    // Wait for activity on the registered sockets.
    int ready = poll(poll_fds, count, SERVER_POLL_TIMEOUT_MS);
    if (ready < 0) {
      if (errno == EINTR)
        continue;
      perror("Server: poll");
      break;
    }

    if (ready == 0)
      continue;

    // Accept new connections when the listening socket becomes readable.
    if (poll_fds[0].revents & POLLIN) {
      for (;;) {
        int fd = accept(listener, NULL, NULL);

        if (fd < 0) {
          if (errno == EINTR)
            continue;
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
          perror("Server: accept");
          break;
        }

        // Find an available slot in the client table.
        int slot = -1;
        for (int i = 0; i < SERVER_MAX_CLIENTS; ++i) {
          if (clients[i].fd < 0) {
            slot = i;
            break;
          }
        }

        // Reject the connection if the server is full or setup fails.
        if (slot < 0 || set_nonblocking(fd) != 0) {
          static const char busy[] = "ERROR server is full\n";
          (void)send(fd, busy, sizeof(busy) - 1, MSG_NOSIGNAL);
          close(fd);
          continue;
        }

        // Initialize the new client connection.
        clients[slot].fd = fd;
        clients[slot].role = CLIENT_UNIDENTIFIED;
        clients[slot].input_length = 0;
        clients[slot].output_length = 0;

        fprintf(stderr, "Server: accepted connection (slot %d)\n", slot);
      }
    }

    // Process events reported for each active client socket.
    for (nfds_t j = 1; j < count; ++j) {
      int slot = slot_map[j];

      // Skip slots that are no longer associated with an active connection.
      if (slot < 0 || clients[slot].fd < 0)
        continue;

      short events = poll_fds[j].revents;
      int failed = 0;

      // Read incoming messages when the socket is ready for reading.
      if (events & POLLIN)
        failed = (read_client(clients, &clients[slot]) != 0);

      // Send pending data when the socket is ready for writing.
      if (!failed && (events & POLLOUT))
        failed = (flush_client(&clients[slot]) != 0);

      // Treat socket errors and hang-up events as connection failures.
      if (events & (POLLERR | POLLHUP | POLLNVAL))
        failed = 1;

      // Log and release connections that can no longer be used.
      if (failed) {
        fprintf(stderr, "Server: client disconnected (%s)\n",
                clients[slot].label[0] ? clients[slot].label : "unidentified");
        close_client(&clients[slot]);
      }
    }
  }

  // Close all active client connections when the server is stopping.
  for (int i = 0; i < SERVER_MAX_CLIENTS; ++i)
    close_client(&clients[i]);

  // Close the listening socket and report successful shutdown.
  close(listener);
  printf("TCP monitoring server stopped.\n");
  return 0;
}
