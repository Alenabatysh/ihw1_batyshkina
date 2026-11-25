#ifndef NETWORK_H
#define NETWORK_H

#include "types.h"

// TCP publisher: sends current simulation snapshots to the monitoring server.
int network_publish_simulation(Simulation *sim, const char *host,
                               const char *port);

// TCP server: receives snapshots and broadcasts them to observer clients.
int network_server_run(const char *bind_host, const char *port);

// TCP observer: connects to the server and renders incoming snapshots.
int network_client_run(const char *host, const char *port);

#endif
