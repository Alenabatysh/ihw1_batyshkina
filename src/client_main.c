#include "network.h"

#include <stdio.h>

// Main function of the client application.
// Receives the server IP address and port from command-line arguments.
int main(int argc, char *argv[]) {
  // The client requires two arguments: server IP address and port.
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
    return 1;
  }

  // Start the network client using the specified server address and port.
  // The function handles the connection and receives simulation data.
  return network_client_run(argv[1], argv[2]);
}