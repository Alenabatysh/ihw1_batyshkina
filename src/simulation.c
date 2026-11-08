#include "simulation.h"
#include "io.h"
#include "rng.h"
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef USE_NAMED_SEM
sem_t *g_mutex_stats = NULL;
sem_t *g_mutex_time = NULL;
sem_t *g_mutex_queues[NUM_CASHIERS] = {NULL};
sem_t *g_customers_ready[NUM_CASHIERS] = {NULL};
sem_t *g_service_done[MAX_CUSTOMERS] = {NULL};
#endif

// Initialize simulation state and semaphores
STATUS sim_init(Simulation *sim, const Config *config) {
  if (sim == NULL || config == NULL)
    return STATUS_NULL_PTR;

  sim->config = *config;
  sim->current_time = 0;
  sim->customers_generated = 0;
  sim->entrance_closed = 0;
  sim->simulation_finished = 0;
  sim->interrupted = 0;

  for (int i = 0; i < NUM_CASHIERS; i++) {
    sim->cashiers[i].id = i;
    sim->cashiers[i].current_customer_id = -1;
    sim->cashiers[i].is_sleeping = 1;
    sim->cashiers[i].total_served = 0;
    sim->cashiers[i].total_service_time = 0;

    sim->queues[i].id = i;
    sim->queues[i].capacity = config->queue_capacity;
    sim->queues[i].size = 0;
    sim->queues[i].total_served = 0;
    sim->queues[i].total_wait_time = 0;
  }

  sim->stats.total_arrived = 0;
  sim->stats.total_served = 0;
  sim->stats.total_left_no_service = 0;
  sim->stats.total_wait_time = 0;
  sim->stats.total_shopping_time = 0;
  sim->stats.max_queue_0_size = 0;
  sim->stats.max_queue_1_size = 0;
  sim->stats.simulation_end_time = 0;

#ifdef USE_NAMED_SEM
  sem_unlink("/sm_mutex_stats");
  sem_unlink("/sm_mutex_time");
  g_mutex_stats = sem_open("/sm_mutex_stats", O_CREAT, 0666, 1);
  g_mutex_time = sem_open("/sm_mutex_time", O_CREAT, 0666, 1);

  char name[64];
  for (int i = 0; i < NUM_CASHIERS; i++) {
    snprintf(name, sizeof(name), "/sm_mq_%d", i);
    sem_unlink(name);
    g_mutex_queues[i] = sem_open(name, O_CREAT, 0666, 1);

    snprintf(name, sizeof(name), "/sm_cr_%d", i);
    sem_unlink(name);
    g_customers_ready[i] = sem_open(name, O_CREAT, 0666, 0);
  }
  for (int i = 0; i < MAX_CUSTOMERS; i++) {
    snprintf(name, sizeof(name), "/sm_sd_%d", i);
    sem_unlink(name);
    g_service_done[i] = sem_open(name, O_CREAT, 0666, 0);
  }
#else
  sem_init(&sim->mutex_stats, 1, 1);
  sem_init(&sim->mutex_time, 1, 1);
  for (int i = 0; i < NUM_CASHIERS; i++) {
    sem_init(&sim->mutex_queues[i], 1, 1);
    sem_init(&sim->customers_ready[i], 1, 0);
  }
  for (int i = 0; i < MAX_CUSTOMERS; i++) {
    sem_init(&sim->service_done[i], 1, 0);
  }
#endif
  return STATUS_OK;
}

// Attach to existing named semaphores (used by independent processes)
void sim_attach_named_sems(Simulation *sim) {
  (void)sim;
#ifdef USE_NAMED_SEM
  g_mutex_stats = sem_open("/sm_mutex_stats", 0);
  g_mutex_time = sem_open("/sm_mutex_time", 0);

  char name[64];
  for (int i = 0; i < NUM_CASHIERS; i++) {
    snprintf(name, sizeof(name), "/sm_mq_%d", i);
    g_mutex_queues[i] = sem_open(name, 0);

    snprintf(name, sizeof(name), "/sm_cr_%d", i);
    g_customers_ready[i] = sem_open(name, 0);
  }
  for (int i = 0; i < MAX_CUSTOMERS; i++) {
    snprintf(name, sizeof(name), "/sm_sd_%d", i);
    g_service_done[i] = sem_open(name, 0);
  }
#endif
}

// Destroy/close and unlink all semaphores
void sim_destroy(Simulation *sim) {
  if (sim == NULL)
    return;

#ifdef USE_NAMED_SEM
  if (g_mutex_stats)
    sem_close(g_mutex_stats);
  sem_unlink("/sm_mutex_stats");

  if (g_mutex_time)
    sem_close(g_mutex_time);
  sem_unlink("/sm_mutex_time");

  char name[64];
  for (int i = 0; i < NUM_CASHIERS; i++) {
    if (g_mutex_queues[i])
      sem_close(g_mutex_queues[i]);
    snprintf(name, sizeof(name), "/sm_mq_%d", i);
    sem_unlink(name);

    if (g_customers_ready[i])
      sem_close(g_customers_ready[i]);
    snprintf(name, sizeof(name), "/sm_cr_%d", i);
    sem_unlink(name);
  }
  for (int i = 0; i < MAX_CUSTOMERS; i++) {
    if (g_service_done[i])
      sem_close(g_service_done[i]);
    snprintf(name, sizeof(name), "/sm_sd_%d", i);
    sem_unlink(name);
  }
#else
  sem_destroy(&sim->mutex_stats);
  sem_destroy(&sim->mutex_time);
  for (int i = 0; i < NUM_CASHIERS; i++) {
    sem_destroy(&sim->mutex_queues[i]);
    sem_destroy(&sim->customers_ready[i]);
  }
  for (int i = 0; i < MAX_CUSTOMERS; i++) {
    sem_destroy(&sim->service_done[i]);
  }
#endif
}

// Get customer pointer by ID
Customer *sim_get_customer(Simulation *sim, int id) {
  if (!sim || id < 0 || id >= sim->customers_generated)
    return NULL;
  return &sim->customers[id];
}

// Get cashier pointer by ID
Cashier *sim_get_cashier(Simulation *sim, int id) {
  if (!sim || id < 0 || id >= NUM_CASHIERS)
    return NULL;
  return &sim->cashiers[id];
}

// Get queue pointer by ID
Queue *sim_get_queue(Simulation *sim, int id) {
  if (!sim || id < 0 || id >= NUM_CASHIERS)
    return NULL;
  return &sim->queues[id];
}

// Print final simulation statistics to console and log
STATUS sim_print_stats(const Simulation *sim) {
  if (!sim)
    return STATUS_NULL_PTR;

  io_printf_both("\n******************************\n");
  io_printf_both("SIMULATION STATISTICS\n");
  io_printf_both("******************************\n");
  io_printf_both("Simulation duration:   %d sec\n",
                 sim->stats.simulation_end_time);
  io_printf_both("Total arrived:         %d\n", sim->stats.total_arrived);
  io_printf_both("Total served:          %d\n", sim->stats.total_served);
  io_printf_both("Total left (no serv.): %d\n",
                 sim->stats.total_left_no_service);

  if (sim->stats.total_arrived > 0) {
    io_printf_both("Average shopping time: %.2f sec\n",
                   (double)sim->stats.total_shopping_time /
                       sim->stats.total_arrived);
  } else {
    io_printf_both("Average shopping time: N/A\n");
  }

  io_printf_both("Max queue 0 size:      %d\n", sim->stats.max_queue_0_size);
  io_printf_both("Max queue 1 size:      %d\n", sim->stats.max_queue_1_size);
  io_printf_both("Cashier 0 served:      %d\n", sim->cashiers[0].total_served);
  io_printf_both("Cashier 1 served:      %d\n", sim->cashiers[1].total_served);
  io_printf_both("******************************\n");

  return STATUS_OK;
}