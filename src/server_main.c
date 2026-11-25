#include "network.h"

#include <stdio.h>

// Main function of the server application.
// Starts the TCP server that accepts observer client connections.
int main(int argc, char *argv[]) {
  // By default, the server listens on all network interfaces.
  const char *bind_host = "0.0.0.0";

  // Default server port.
  const char *port = "5555";

  // If one argument is provided, treat it as the server port.
  if (argc == 2) {
    port = argv[1];
  } else if (argc == 3) {
    // If two arguments are provided, use the specified IP address and port.
    bind_host = argv[1];
    port = argv[2];
  } else if (argc != 1) {
    // Check whether the number of command-line arguments is valid.
    fprintf(stderr, "Usage: %s [bind_ip] [port]\n", argv[0]);
    return 1;
  }

  // Start the TCP server with the selected configuration.
  // The function handles incoming connections and communication with clients.
  return network_server_run(bind_host, port);
}