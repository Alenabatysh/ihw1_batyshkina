#include "config.h"
#include "display.h"
#include "event_queue.h"
#include "io.h"
#include "rng.h"
#include "simulation.h"
#include "types.h"
#include <signal.h>
#include <stdlib.h>

// Global pointer to simulation for signal handler access
static Simulation *g_sim = NULL;

// SIGINT handler: cleanup and print stats on Ctrl+C
void sigint_handler(int sig) {
  (void)sig;
  display_cleanup();
  if (g_sim != NULL) {
    sim_print_stats(g_sim);
    sim_destroy(g_sim);
  }
  io_close_log();
  exit(0);
}

// Program entry point
int main(int argc, char *argv[]) {
  Config config;
  STATUS status;

  // Load config from file or use defaults
  if (argc > 1) {
    status = config_load(argv[1], &config);
    if (status != STATUS_OK) {
      io_write_console("Failed to load config, using defaults\n");
      config_set_defaults(&config);
    }
  } else {
    config_set_defaults(&config);
  }

  // Validate configuration parameters
  status = config_validate(&config);
  if (status != STATUS_OK) {
    io_write_console("ERROR: Invalid configuration!\n");
    return 1;
  }

  // Initialize random number generator with seed
  rng_init(config.random_seed);

  // Initialize log file for writing
  status = io_init_log("simulation.log");
  if (status != STATUS_OK) {
    io_write_console("Failed to open log file\n");
    return 1;
  }

  // Print configuration to console and log
  config_print(&config);

  // Create and initialize simulation state
  Simulation sim;
  g_sim = &sim;
  signal(SIGINT, sigint_handler);

  status = sim_init(&sim, &config);
  if (status != STATUS_OK) {
    io_write_console("Failed to initialize simulation\n");
    io_close_log();
    return 1;
  }

  // Initialize display (hide cursor, clear screen)
  display_init();

  // Schedule initial events and run simulation
  sim_schedule_initial(&sim);
  sim_run(&sim);

  // Restore cursor and clean up display
  display_cleanup();

  // Print final statistics
  sim_print_stats(&sim);
  sim_destroy(&sim);

  // Verify conservation law: arrived = served + left
  int processed = sim.stats.total_served + sim.stats.total_left_no_service;
  if (processed != sim.stats.total_arrived) {
    io_printf_both("\nERROR: Conservation law violated!\n");
    io_printf_both("Arrived: %d, Served: %d, Left: %d\n",
                   sim.stats.total_arrived, sim.stats.total_served,
                   sim.stats.total_left_no_service);
    io_close_log();
    return 1;
  }

  io_printf_both("\nConservation law verified: %d = %d + %d\n",
                 sim.stats.total_arrived, sim.stats.total_served,
                 sim.stats.total_left_no_service);

  io_close_log();

  return 0;
}