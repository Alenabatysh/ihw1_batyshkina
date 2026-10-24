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

// SIGINT handler: signals threads to stop gracefully
void sigint_handler(int sig) {
  (void)sig;
  g_interrupted = 1;
  if (g_sim != NULL) {
    // Set flags to stop generator and display threads
    g_sim->entrance_closed = 1;
    g_sim->simulation_finished = 1;

#ifdef USE_CONDVAR
    // Broadcast to wake up all cashiers waiting on queues
    for (int i = 0; i < NUM_CASHIERS; i++) {
      pthread_mutex_lock(&g_sim->queues[i].mutex);
      pthread_cond_broadcast(&g_sim->queues[i].item_available);
      pthread_mutex_unlock(&g_sim->queues[i].mutex);
    }

    for (int i = 0; i < g_sim->customers_generated; i++) {
      pthread_mutex_lock(&g_sim->customers[i].service_mutex);
      pthread_cond_broadcast(&g_sim->customers[i].service_cond);
      pthread_mutex_unlock(&g_sim->customers[i].service_mutex);
    }
#else
    // Wake up sleeping cashiers using semaphores
    sem_post(&g_sim->cashiers[0].customer_ready);
    sem_post(&g_sim->cashiers[1].customer_ready);
#endif

    // Async-signal-safe output
    const char msg[] =
        "\n[INFO] Ctrl+C received. Shutting down gracefully...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
  }
}

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
    io_write_console("Error: Invalid configuration!\n");
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

  // Initialize the real-time display interface
  display_init();
  io_printf_both("\nStarting multithreaded simulation...\n");

  // Start threads and wait for completion
  sim_start(&sim);
  sim_wait_completion(&sim);

  // Restore terminal cursor and state
  display_cleanup();

  if (g_interrupted) {
    io_printf_both("\nSimulation was interrupted by user.\n");
  } else {
    io_printf_both("\nSimulation complete.\n");
  }

  // Print final statistics
  sim_print_stats(&sim);
  sim_destroy(&sim);

  // Verify conservation law: arrived = served + left
  int processed = sim.stats.total_served + sim.stats.total_left_no_service;
  if (processed != sim.stats.total_arrived) {
    io_printf_both("\nError: Conservation law violated!\n");
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
  g_sim = NULL;

  return 0;
}