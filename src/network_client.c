#include "network.h"

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CLIENT_LINE_SIZE 2048

// Stores one snapshot of the supermarket simulation received from the server.
typedef struct {
  unsigned long sequence;
  int model_time;
  int duration;
  int arrived;
  int served;
  int left;
  int queue_size[NUM_CASHIERS];
  char queue_ids[NUM_CASHIERS][512];
  int cashier_sleeping[NUM_CASHIERS];
  int cashier_customer[NUM_CASHIERS];
  int finished;
} MonitorSnapshot;

// Indicates whether the client should stop its execution.
static volatile sig_atomic_t client_stop = 0;

// Handles termination signals and requests a graceful client shutdown.
static void client_signal_handler(int signal_number) {
  (void)signal_number;
  client_stop = 1;
}

// Sends all data to the specified socket.
// A single send() call may transmit only part of the provided data.
static int send_all(int fd, const char *data, size_t length) {
  size_t sent = 0;
  while (sent < length) {
    ssize_t count = send(fd, data + sent, length - sent, MSG_NOSIGNAL);
    if (count < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (count == 0)
      return -1;
    sent += (size_t)count;
  }
  return 0;
}

// Establishes a TCP connection to the specified server.
// Tries all available addresses returned by getaddrinfo().
static int connect_tcp(const char *host, const char *port) {
  struct addrinfo hints;
  struct addrinfo *result = NULL;
  struct addrinfo *it;
  int fd = -1;

  // Configure address resolution for TCP connections.
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int gai_error = getaddrinfo(host, port, &hints, &result);
  if (gai_error != 0) {
    fprintf(stderr, "Client: getaddrinfo: %s\n", gai_strerror(gai_error));
    return -1;
  }

  // Try to create a socket and connect to each available address.
  for (it = result; it != NULL; it = it->ai_next) {
    fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (fd < 0)
      continue;
    if (connect(fd, it->ai_addr, it->ai_addrlen) == 0)
      break;
    close(fd);
    fd = -1;
  }

  // Release the address information allocated by getaddrinfo().
  freeaddrinfo(result);
  return fd;
}

// Converts a string into an integer.
// Returns -1 if the input is empty, invalid or outside the allowed range.
static int parse_int(const char *text, int *value) {
  char *end = NULL;
  long parsed;
  if (text == NULL || value == NULL || text[0] == '\0')
    return -1;
  errno = 0;
  parsed = strtol(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || parsed < -1000000L ||
      parsed > 1000000L)
    return -1;
  *value = (int)parsed;
  return 0;
}

// Parses a snapshot received from the server.
// The message must contain exactly 16 fields separated by '|'.
static int parse_snapshot(char *line, MonitorSnapshot *snapshot) {
  char *fields[16];
  char *save = NULL;
  char *token = strtok_r(line, "|", &save);
  int count = 0;

  // Split the received message into individual fields.
  while (token != NULL && count < 16) {
    fields[count++] = token;
    token = strtok_r(NULL, "|", &save);
  }

  // Check the number of fields and the message type.
  if (count != 16 || strcmp(fields[0], "S") != 0 || token != NULL)
    return -1;

  // Parse the snapshot sequence number.
  char *end = NULL;
  errno = 0;
  snapshot->sequence = strtoul(fields[1], &end, 10);
  if (errno != 0 || end == fields[1] || *end != '\0')
    return -1;

  // Map message fields to the corresponding numeric snapshot values.
  int *numeric[] = {&snapshot->model_time,
                    &snapshot->duration,
                    &snapshot->arrived,
                    &snapshot->served,
                    &snapshot->left,
                    &snapshot->queue_size[0],
                    NULL,
                    &snapshot->queue_size[1],
                    NULL,
                    &snapshot->cashier_sleeping[0],
                    &snapshot->cashier_customer[0],
                    &snapshot->cashier_sleeping[1],
                    &snapshot->cashier_customer[1],
                    &snapshot->finished};

  // Field indexes correspond to the positions in the received message.
  // NULL entries represent string fields that are processed separately.
  int field_index[] = {2, 3, 4, 5, 6, 7, -1, 9, -1, 11, 12, 13, 14, 15};

  for (size_t i = 0; i < sizeof(field_index) / sizeof(field_index[0]); ++i) {
    if (numeric[i] != NULL &&
        parse_int(fields[field_index[i]], numeric[i]) != 0)
      return -1;
  }

  // Check that queue identifier strings fit into the destination arrays.
  if (strlen(fields[8]) >= sizeof(snapshot->queue_ids[0]) ||
      strlen(fields[10]) >= sizeof(snapshot->queue_ids[1]))
    return -1;

  // Copy queue identifiers into the snapshot structure.
  strcpy(snapshot->queue_ids[0], fields[8]);
  strcpy(snapshot->queue_ids[1], fields[10]);

  // Validate queue sizes and the simulation completion flag.
  if (snapshot->queue_size[0] < 0 ||
      snapshot->queue_size[0] > MAX_QUEUE_CAPACITY ||
      snapshot->queue_size[1] < 0 ||
      snapshot->queue_size[1] > MAX_QUEUE_CAPACITY ||
      (snapshot->finished != 0 && snapshot->finished != 1))
    return -1;
  return 0;
}

// Displays the current simulation snapshot in the terminal.
// The screen is refreshed whenever a new snapshot is received.
static void render_snapshot(const MonitorSnapshot *snapshot) {
  // Clear the terminal and move the cursor to the top-left corner.
  printf("\033[2J\033[H");
  printf(
      "+------------------------------------------------------------------+\n");
  printf(
      "|              SUPERMARKET - REMOTE MONITOR (TCP)                  |\n");
  printf(
      "+------------------------------------------------------------------+\n");

  // Display simulation time and snapshot sequence number.
  printf("| Model time: %6d / %-6d      Snapshot: %-10lu              |\n",
         snapshot->model_time, snapshot->duration, snapshot->sequence);
  printf(
      "+------------------------------------------------------------------+\n");

  // Display the total number of arrived, served and unserved customers.
  printf("| Arrived: %-6d Served: %-6d Left without service: %-6d       |\n",
         snapshot->arrived, snapshot->served, snapshot->left);
  printf(
      "+------------------------------------------------------------------+\n");

  // Display queue contents and the current status of each cashier.
  for (int i = 0; i < NUM_CASHIERS; ++i) {
    printf("| Cashier %d queue (%2d): %-40.40s          |\n", i,
           snapshot->queue_size[i], snapshot->queue_ids[i]);

    if (snapshot->cashier_customer[i] >= 0) {
      printf("| Cashier %d status: SERVING customer C%-4d                      "
             "   |\n",
             i, snapshot->cashier_customer[i]);
    } else if (snapshot->cashier_sleeping[i]) {
      printf("| Cashier %d status: waiting for a customer                      "
             "   |\n",
             i);
    } else {
      printf("| Cashier %d status: idle                                        "
             "    |\n",
             i);
    }
  }

  printf(
      "+------------------------------------------------------------------+\n");

  // Display whether the simulation is still running or has finished.
  if (snapshot->finished) {
    printf("| Simulation complete. The last state was received from server.   "
           "|\n");
  } else {
    printf("| Connected. This observer can be stopped/restarted "
           "independently.|\n");
  }

  printf(
      "+------------------------------------------------------------------+\n");
  fflush(stdout);
}

// Processes one complete line received from the server.
// Handles welcome messages, errors and simulation snapshots.
static int handle_line(char *line, int *finished) {
  // Handle the initial welcome message from the server.
  if (strncmp(line, "WELCOME", 7) == 0) {
    printf("%s\n", line);
    fflush(stdout);
    return 0;
  }

  // Report an error message received from the server.
  if (strncmp(line, "ERROR", 5) == 0) {
    fprintf(stderr, "%s\n", line);
    return -1;
  }

  // Parse and display a new simulation snapshot.
  if (strncmp(line, "S|", 2) == 0) {
    MonitorSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));

    if (parse_snapshot(line, &snapshot) != 0) {
      fprintf(stderr, "Client: received malformed snapshot\n");
      return -1;
    }

    render_snapshot(&snapshot);

    // Update the completion flag based on the received snapshot.
    *finished = snapshot.finished;
    return 0;
  }

  return 0;
}

// Main network client routine.
// Connects to the server, registers as an observer and receives snapshots.
int network_client_run(const char *host, const char *port) {
  if (host == NULL || port == NULL)
    return 1;

  // Configure handlers for Ctrl+C and termination signals.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = client_signal_handler;
  sigemptyset(&action.sa_mask);
  (void)sigaction(SIGINT, &action, NULL);
  (void)sigaction(SIGTERM, &action, NULL);

  // Establish a TCP connection to the monitoring server.
  int fd = connect_tcp(host, port);
  if (fd < 0) {
    fprintf(stderr, "Client: cannot connect to %s:%s\n", host, port);
    return 1;
  }

  // Register this connection as an observer.
  static const char hello[] = "OBSERVER observer\n";
  if (send_all(fd, hello, sizeof(hello) - 1) != 0) {
    perror("Client: send");
    close(fd);
    return 1;
  }

  printf("Connected to monitoring server %s:%s. Press Ctrl+C to disconnect.\n",
         host, port);
  fflush(stdout);

  char input[CLIENT_LINE_SIZE];
  size_t used = 0;
  int finished = 0;

  // Receive server messages until the client is stopped or the simulation ends.
  while (!client_stop && !finished) {
    char chunk[1024];
    ssize_t count = recv(fd, chunk, sizeof(chunk), 0);

    // A zero return value means that the server closed the connection.
    if (count == 0) {
      fprintf(stderr, "Client: server closed the connection\n");
      break;
    }

    // Handle receive errors and interrupted system calls.
    if (count < 0) {
      if (errno == EINTR)
        continue;
      perror("Client: recv");
      close(fd);
      return 1;
    }

    // Process every received byte and split the stream into complete lines.
    for (ssize_t i = 0; i < count; ++i) {
      if (chunk[i] == '\n') {
        input[used] = '\0';

        // Remove a trailing carriage return if the message uses CRLF.
        if (used > 0 && input[used - 1] == '\r')
          input[used - 1] = '\0';

        // Process the complete message received from the server.
        if (handle_line(input, &finished) != 0) {
          close(fd);
          return 1;
        }

        used = 0;

        // Stop processing when the final simulation snapshot is received.
        if (finished)
          break;
      } else {
        // Prevent writing beyond the input buffer boundaries.
        if (used + 1 >= sizeof(input)) {
          fprintf(stderr, "Client: input line too long\n");
          close(fd);
          return 1;
        }
        input[used++] = chunk[i];
      }
    }
  }

  // Close the socket when the client disconnects or the simulation finishes.
  close(fd);

  if (client_stop)
    printf(
        "Observer disconnected. The simulation and server continue running.\n");

  return 0;
}