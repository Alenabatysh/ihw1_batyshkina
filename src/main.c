#include "config.h"
#include "display.h"
#include "io.h"
#include "rng.h"
#include "simulation.h"
#include "types.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHM_NAME "/supermarket_shm"
static Simulation *g_sim = NULL; // Global pointer for SIGINT handler

// SIGINT handler: set interrupted flag and wake up all sleeping processes
void sigint_handler(int sig) {
  (void)sig;
  if (g_sim != NULL) {
    g_sim->interrupted = 1;
    Simulation *sim = g_sim;
    (void)sim;

    // Wake up cashiers waiting on customers_ready
    for (int i = 0; i < NUM_CASHIERS; i++)
      sem_post(CUSTOMERS_READY(i));

    // Wake up customers waiting for service_done
    for (int i = 0; i < g_sim->customers_generated; i++)
      sem_post(SERVICE_DONE(i));
  }
}

// Entry point: dispatch by role (init / display / cashier / generator /
// cleanup)
int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <role> [args]\n", argv[0]);
    printf("Roles: init, display, cashier <id>, generator, cleanup\n");
    return 1;
  }

  const char *role = argv[1];
  int shm_fd;
  Simulation *sim;

  // init (create shared memory + semaphores)
  if (strcmp(role, "init") == 0) {
    Config config;
    config_set_defaults(&config);

    if (argc > 2) {
      if (config_load(argv[2], &config) != STATUS_OK) {
        printf("ERROR: Failed to load config file: %s\n", argv[2]);
        return 1;
      }
    }

    if (config_validate(&config) != STATUS_OK) {
      printf("ERROR: Invalid configuration!\n");
      return 1;
    }

    rng_init(config.random_seed);
    io_init_log("simulation.log");
    config_print(&config);

    // Create and map shared memory
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(Simulation));
    sim = mmap(0, sizeof(Simulation), PROT_READ | PROT_WRITE, MAP_SHARED,
               shm_fd, 0);
    sim_init(sim, &config); // Init state and semaphores

    munmap(sim, sizeof(Simulation));
    close(shm_fd);
    printf("Shared memory and semaphores initialized.\n");
    return 0;
  }
  // cleanup (print stats + free resources)
  else if (strcmp(role, "cleanup") == 0) {
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1)
      return 1;
    sim = mmap(0, sizeof(Simulation), PROT_READ | PROT_WRITE, MAP_SHARED,
               shm_fd, 0);

    // Attach to named semaphores so we can close/unlink them properly
    sim_attach_named_sems(sim);

    sim->stats.simulation_end_time = sim->current_time;
    sim_print_stats(sim);

    // Verify invariant: arrived = served + left
    int processed = sim->stats.total_served + sim->stats.total_left_no_service;
    if (processed != sim->stats.total_arrived) {
      printf("\nError: Conservation law violated!\n");
      printf("Arrived: %d, Served: %d, Left: %d\n", sim->stats.total_arrived,
             sim->stats.total_served, sim->stats.total_left_no_service);
      sim_destroy(sim);
      munmap(sim, sizeof(Simulation));
      close(shm_fd);
      shm_unlink(SHM_NAME);
      io_close_log();
      return 1;
    }

    printf("\nConservation law verified: %d = %d + %d\n",
           sim->stats.total_arrived, sim->stats.total_served,
           sim->stats.total_left_no_service);

    // Free all resources
    sim_destroy(sim);
    munmap(sim, sizeof(Simulation));
    close(shm_fd);
    shm_unlink(SHM_NAME);
    io_close_log();
    printf("Cleaned up resources.\n");
    return 0;
  }

  // attach to already existing shared memory
  shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open (did you run 'init'?)");
    return 1;
  }
  sim = mmap(0, sizeof(Simulation), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd,
             0);
  g_sim = sim;

  // Open named semaphores by name in this process
  sim_attach_named_sems(sim);

  // Install SIGINT handler
  struct sigaction sa;
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);

  // display
  if (strcmp(role, "display") == 0) {
    display_init();
    while (!sim->simulation_finished && !sim->interrupted) {
      display_render(sim);
      usleep(50000); // 50ms refresh rate
    }
    display_render(sim);
    display_cleanup();
  }
  // cashier
  else if (strcmp(role, "cashier") == 0 && argc > 2) {
    int id = atoi(argv[2]);
    cashier_process(sim, id);
  }
  // generator (spawns customers via fork)
  else if (strcmp(role, "generator") == 0) {
    rng_init(sim->config.random_seed);

    for (int i = 0; i < sim->config.total_customers; i++) {
      if (sim->interrupted ||
          sim->current_time >= sim->config.work_day_duration)
        break;

      // Sleep for random interval between arrivals
      int interval = rng_range(sim->config.arrival_interval_min,
                               sim->config.arrival_interval_max);
      usleep(interval * 1000 * sim->config.time_scale);

      if (sim->interrupted)
        break;

      // Update shared current_time and arrival counter under lock
      sem_wait(MUTEX_TIME);
      sim->current_time += interval;
      sim->stats.total_arrived++;
      sem_post(MUTEX_TIME);

      sim->customers[i].id = i;
      sim->customers[i].arrival_time = sim->current_time;
      sim->customers[i].items_count =
          rng_range(sim->config.items_min, sim->config.items_max);

      // Spawn a new process for this customer
      pid_t c_pid = fork();
      if (c_pid == 0) {
        signal(SIGINT, SIG_IGN);    // Child doesn't handle SIGINT
        sim_attach_named_sems(sim); // Re-open semaphores in child
        customer_process(sim, i);
        exit(0);
      } else if (c_pid > 0) {
        sim->customers[i].pid = c_pid;
        sim->customers_generated++;
      } else {
        perror("fork customer failed");
      }
    }

    // Wait for all spawned customers to finish
    for (int i = 0; i < sim->customers_generated; i++) {
      waitpid(sim->customers[i].pid, NULL, 0);
    }

    // Close the entrance and wake up cashiers to let them terminate
    sem_wait(MUTEX_TIME);
    sim->entrance_closed = 1;
    sem_post(MUTEX_TIME);

    for (int i = 0; i < NUM_CASHIERS; i++) {
      sem_post(CUSTOMERS_READY(i));
    }
  }

  munmap(sim, sizeof(Simulation));
  close(shm_fd);
  return 0;
}