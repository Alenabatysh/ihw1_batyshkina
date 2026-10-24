#include "config.h"
#include "display.h"
#include "io.h"
#include "rng.h"
#include "simulation.h"
#include "types.h"
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

// Global pointer to simulation for signal handler access
static Simulation *g_sim = NULL;

// Atomic flag: 1 if Ctrl+C was pressed
volatile sig_atomic_t g_interrupted = 0;

// SIGINT handler
void sigint_handler(int sig) {
  (void)sig;
  g_interrupted = 1;

#ifndef USE_CONDVAR
  if (g_sim != NULL) {
    sem_post(&g_sim->cashiers[0].customer_ready);
    sem_post(&g_sim->cashiers[1].customer_ready);
  }
#endif
  const char msg[] = "\n[INFO] Ctrl+C received. Shutting down...\n";
  write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

int main(int argc, char *argv[]) {
  Config config;
  STATUS status;

  if (argc > 1) {
    status = config_load(argv[1], &config);
    if (status != STATUS_OK) {
      io_write_console("Failed to load config, using defaults\n");
      config_set_defaults(&config);
    }
  } else {
    config_set_defaults(&config);
  }

  status = config_validate(&config);
  if (status != STATUS_OK) {
    io_write_console("Error: Invalid configuration!\n");
    return 1;
  }

  rng_init(config.random_seed);

  status = io_init_log("simulation.log");
  if (status != STATUS_OK) {
    io_write_console("Failed to open log file\n");
    return 1;
  }

  config_print(&config);

  Simulation sim;
  g_sim = &sim;

  struct sigaction sa;
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);

  status = sim_init(&sim, &config);
  if (status != STATUS_OK) {
    io_write_console("Failed to initialize simulation\n");
    io_close_log();
    g_sim = NULL;
    return 1;
  }

  display_init();
  io_printf_both("\nStarting multithreaded simulation...\n");

  sim_start(&sim);
  sim_wait_completion(&sim);

  display_cleanup();

  if (g_interrupted) {
    io_printf_both("\nSimulation was interrupted by user.\n");
  } else {
    io_printf_both("\nSimulation complete.\n");
  }

  sim_print_stats(&sim);
  sim_destroy(&sim);

  int processed = sim.stats.total_served + sim.stats.total_left_no_service;
  if (processed != sim.stats.total_arrived) {
    io_printf_both("\nError: Conservation law violated!\n");
    io_printf_both("Arrived: %d, Served: %d, Left: %d\n",
                   sim.stats.total_arrived, sim.stats.total_served,
                   sim.stats.total_left_no_service);
    io_close_log();
    g_sim = NULL;
    return 1;
  }

  io_printf_both("\nConservation law verified: %d = %d + %d\n",
                 sim.stats.total_arrived, sim.stats.total_served,
                 sim.stats.total_left_no_service);
  io_close_log();
  g_sim = NULL;
  return 0;
}