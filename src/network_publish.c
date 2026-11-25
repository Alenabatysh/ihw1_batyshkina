
#include "network.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// Maximum size of one serialized simulation snapshot.
#define SNAPSHOT_LINE_SIZE 2048

// Establish a TCP connection to the monitoring server.
// The function tries all available addresses returned by getaddrinfo.
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
    fprintf(stderr, "Publisher: cannot resolve %s: %s\n", host,
            gai_strerror(gai_error));
    return -1;
  }

  // Try connecting to each available server address.
  for (it = result; it != NULL; it = it->ai_next) {
    fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (fd < 0)
      continue;
    if (connect(fd, it->ai_addr, it->ai_addrlen) == 0)
      break;
    close(fd);
    fd = -1;
  }

  freeaddrinfo(result);

  // Set a timeout for sending data to prevent indefinite blocking.
  if (fd >= 0) {
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  }
  return fd;
}

// Send the entire buffer through the TCP socket.
// send() may transmit only part of the data, so the function repeats the call
// until all bytes have been sent or an error occurs.
static int send_all(int fd, const char *buffer, size_t length) {
  size_t sent = 0;
  while (sent < length) {
    ssize_t n = send(fd, buffer + sent, length - sent, MSG_NOSIGNAL);
    if (n < 0) {
      // Retry the operation if it was interrupted by a signal.
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (n == 0)
      return -1;
    sent += (size_t)n;
  }
  return 0;
}

// Convert a cashier queue into a comma-separated list of customer IDs.
// An empty queue is represented by a dash.
static int queue_to_text(const Queue *queue, char *dst, size_t dst_size) {
  size_t used = 0;
  if (queue->size == 0) {
    if (dst_size < 2)
      return -1;
    strcpy(dst, "-");
    return 0;
  }

  // Add customer IDs to the output buffer.
  for (int i = 0; i < queue->size; ++i) {
    int n = snprintf(dst + used, dst_size - used, "%s%d", (i == 0) ? "" : ",",
                     queue->customer_ids[i]);
    if (n < 0 || (size_t)n >= dst_size - used)
      return -1;
    used += (size_t)n;
  }
  return 0;
}

// Read independently protected parts of shared state and serialize a snapshot.
// Each section is protected by its corresponding semaphore.
// The snapshot uses a line-oriented protocol with fields separated by '|':
// S|seq|time|duration|arrived|served|left|q0size|q0ids|q1size|q1ids|
// c0sleep|c0customer|c1sleep|c1customer|finished
static int make_snapshot(Simulation *sim, unsigned long sequence, char *line,
                         size_t line_size, int *is_finished) {
  int model_time;
  int entrance_closed;
  int arrived, served, left;
  int q_size[NUM_CASHIERS];
  int cashier_sleeping[NUM_CASHIERS];
  int cashier_customer[NUM_CASHIERS];
  char q_text[NUM_CASHIERS][512];

  // Read the current model time and entrance status.
  if (sem_wait(MUTEX_TIME) == -1)
    return -1;
  model_time = sim->current_time;
  entrance_closed = sim->entrance_closed;
  (void)sem_post(MUTEX_TIME);

  // Read the current simulation statistics.
  if (sem_wait(MUTEX_STATS) == -1)
    return -1;
  arrived = sim->stats.total_arrived;
  served = sim->stats.total_served;
  left = sim->stats.total_left_no_service;
  (void)sem_post(MUTEX_STATS);

  // Read each cashier's queue and current working state.
  for (int i = 0; i < NUM_CASHIERS; ++i) {
    if (sem_wait(MUTEX_QUEUE(i)) == -1)
      return -1;
    q_size[i] = sim->queues[i].size;
    cashier_sleeping[i] = sim->cashiers[i].is_sleeping;
    cashier_customer[i] = sim->cashiers[i].current_customer_id;

    // Convert the queue contents into a text representation.
    int result = queue_to_text(&sim->queues[i], q_text[i], sizeof(q_text[i]));
    (void)sem_post(MUTEX_QUEUE(i));
    if (result != 0)
      return -1;
  }

  // Check whether all customers have been processed and both cashiers are idle.
  int processed = served + left;
  *is_finished = entrance_closed && processed >= arrived && q_size[0] == 0 &&
                 q_size[1] == 0 && cashier_customer[0] < 0 &&
                 cashier_customer[1] < 0;

  // Serialize all collected data into a single protocol message.
  int n = snprintf(line, line_size,
                   "S|%lu|%d|%d|%d|%d|%d|%d|%s|%d|%s|%d|%d|%d|%d|%d\n",
                   sequence, model_time, sim->config.work_day_duration, arrived,
                   served, left, q_size[0], q_text[0], q_size[1], q_text[1],
                   cashier_sleeping[0], cashier_customer[0],
                   cashier_sleeping[1], cashier_customer[1], *is_finished);
  if (n < 0 || (size_t)n >= line_size)
    return -1;
  return n;
}

// Pause execution for the specified number of milliseconds.
// nanosleep is repeated if the waiting period is interrupted by a signal.
static void sleep_ms(int milliseconds) {
  struct timespec request;
  request.tv_sec = milliseconds / 1000;
  request.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
  while (nanosleep(&request, &request) == -1 && errno == EINTR) {
  }
}

// Publish simulation snapshots to the TCP monitoring server.
// The publisher reconnects automatically if the server becomes unavailable.
int network_publish_simulation(Simulation *sim, const char *host,
                               const char *port) {
  // Check whether all required arguments are valid.
  if (sim == NULL || host == NULL || port == NULL)
    return 1;

  unsigned long sequence = 0;
  int fd = -1;
  int finished = 0;
  char line[SNAPSHOT_LINE_SIZE];

  // Identification message sent when the publisher connects to the server.
  char hello[] = "PUBLISHER supermarket-simulation\n";

  // Determine how often new snapshots should be sent.
  // The minimum interval is 50 milliseconds.
  int interval_ms = sim->config.display_delay_ms;
  if (interval_ms < 50)
    interval_ms = 50;

  fprintf(stderr, "Publisher: sending snapshots to %s:%s\n", host, port);

  // Continue publishing until the simulation finishes or receives an interrupt.
  while (!finished && !sim->interrupted) {
    // Establish a connection if the publisher is currently disconnected.
    if (fd < 0) {
      fd = connect_tcp(host, port);
      if (fd < 0) {
        fprintf(stderr, "Publisher: server unavailable; retrying...\n");
        sleep_ms(500);
        continue;
      }

      // Identify this connection as a simulation data publisher.
      if (send_all(fd, hello, strlen(hello)) != 0) {
        close(fd);
        fd = -1;
        sleep_ms(500);
        continue;
      }
      fprintf(stderr, "Publisher: connected to monitoring server\n");
    }

    // Create a new snapshot and send it to the monitoring server.
    int length = make_snapshot(sim, sequence++, line, sizeof(line), &finished);
    if (length < 0 || send_all(fd, line, (size_t)length) != 0) {
      // Close a broken connection and retry the connection later.
      fprintf(stderr, "Publisher: connection lost; reconnecting\n");
      close(fd);
      fd = -1;
      sleep_ms(250);
      continue;
    }

    // Wait before publishing the next snapshot if the simulation is still
    // running.
    if (!finished)
      sleep_ms(interval_ms);
  }

  // Release the TCP socket when publishing is finished.
  if (fd >= 0) {
    close(fd);
  }

  fprintf(stderr, "Publisher: stopped%s\n",
          finished ? " (simulation complete)" : "");
  return 0;
}
