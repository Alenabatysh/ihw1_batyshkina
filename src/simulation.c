#include "simulation.h"
#include "io.h"
#include "rng.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>

// Forward declarations of thread functions
extern void *cashier_thread_func(void *arg);
extern void *generator_thread_func(void *arg);
extern void *display_thread_func(void *arg);

// Initialize simulation state, mutexes, and semaphores/condvars
STATUS sim_init(Simulation *sim, const Config *config) {
  if (sim == NULL || config == NULL)
    return STATUS_NULL_PTR;

  // Copy configuration and reset counters
  sim->config = *config;
  sim->current_time = 0;
  sim->customers_count = config->total_customers;
  sim->customers_generated = 0;
  sim->entrance_closed = 0;
  sim->simulation_finished = 0;

  // Initialize global and statistics mutexes
  pthread_mutex_init(&sim->global_mutex, NULL);
  pthread_mutex_init(&sim->stats.mutex, NULL);

  // Initialize cashiers and queues
  for (int i = 0; i < NUM_CASHIERS; i++) {
    sim->cashiers[i].id = i;
    sim->cashiers[i].sim = sim;
    sim->cashiers[i].current_customer_id = -1;
    sim->cashiers[i].is_sleeping = 1;
    sim->cashiers[i].total_served = 0;
    sim->cashiers[i].total_service_time = 0;
    pthread_mutex_init(&sim->cashiers[i].mutex, NULL);

#ifdef USE_CONDVAR
    pthread_cond_init(&sim->cashiers[i].customer_cond, NULL);
#else
    sem_init(&sim->cashiers[i].customer_ready, 0, 0);
#endif

    sim->queues[i].id = i;
    sim->queues[i].sim = sim;
    sim->queues[i].capacity = config->queue_capacity;
    sim->queues[i].size = 0;
    sim->queues[i].total_served = 0;
    sim->queues[i].total_wait_time = 0;
    sim->queues[i].customer_ids = calloc(config->queue_capacity, sizeof(int));
    pthread_mutex_init(&sim->queues[i].mutex, NULL);

#ifdef USE_CONDVAR
    pthread_cond_init(&sim->queues[i].item_available, NULL);
#else
    sem_init(&sim->queues[i].cashier_signal, 0, 0);
#endif
  }

  // Reset statistics
  sim->stats.total_arrived = 0;
  sim->stats.total_served = 0;
  sim->stats.total_left_no_service = 0;
  sim->stats.total_wait_time = 0;
  sim->stats.total_shopping_time = 0;
  sim->stats.max_queue_0_size = 0;
  sim->stats.max_queue_1_size = 0;
  sim->stats.simulation_end_time = 0;

  return STATUS_OK;
}

// Destroy mutexes, semaphores/condvars, and free allocated memory
void sim_destroy(Simulation *sim) {
  if (sim == NULL)
    return;

  pthread_mutex_destroy(&sim->global_mutex);
  pthread_mutex_destroy(&sim->stats.mutex);

  for (int i = 0; i < NUM_CASHIERS; i++) {
    pthread_mutex_destroy(&sim->cashiers[i].mutex);
#ifdef USE_CONDVAR
    pthread_cond_destroy(&sim->cashiers[i].customer_cond);
#else
    sem_destroy(&sim->cashiers[i].customer_ready);
#endif
    pthread_mutex_destroy(&sim->queues[i].mutex);
#ifdef USE_CONDVAR
    pthread_cond_destroy(&sim->queues[i].item_available);
#else
    sem_destroy(&sim->queues[i].cashier_signal);
#endif
    free(sim->queues[i].customer_ids);
  }
}

// Start cashier, generator, and display threads
void sim_start(Simulation *sim) {
  // Create display thread first
  pthread_create(&sim->display_thread, NULL, display_thread_func, sim);

  // Create cashier threads
  for (int i = 0; i < NUM_CASHIERS; i++) {
    pthread_create(&sim->cashiers[i].thread, NULL, cashier_thread_func,
                   &sim->cashiers[i]);
  }

  // Create generator thread
  pthread_t generator_thread;
  pthread_create(&generator_thread, NULL, generator_thread_func, sim);

  // Wait for generator to finish spawning all customers
  pthread_join(generator_thread, NULL);
}

// Wait for all customers, cashiers, and display thread to finish
void sim_wait_completion(Simulation *sim) {
  // Wait for all customer threads to finish being served
  for (int i = 0; i < sim->customers_count; i++) {
    pthread_join(sim->customers[i].thread, NULL);
  }

  // Wake up all cashiers so they can check the exit condition and terminate
  for (int i = 0; i < NUM_CASHIERS; i++) {
#ifdef USE_CONDVAR
    pthread_mutex_lock(&sim->queues[i].mutex);
    pthread_cond_broadcast(&sim->queues[i].item_available);
    pthread_mutex_unlock(&sim->queues[i].mutex);
#else
    sem_post(&sim->cashiers[i].customer_ready);
#endif
  }

  // Wait for cashier threads to exit
  for (int i = 0; i < NUM_CASHIERS; i++) {
    pthread_join(sim->cashiers[i].thread, NULL);
  }

  // Signal display thread to stop and wait for it
  sim->simulation_finished = 1;
  pthread_join(sim->display_thread, NULL);
  sim->stats.simulation_end_time = sim->current_time;
}

// Get customer pointer by ID
Customer *sim_get_customer(Simulation *sim, int id) {
  if (!sim || id < 0 || id >= sim->customers_count)
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

  if (sim->stats.total_served > 0) {
    io_printf_both("Average wait time:     %.2f sec\n",
                   (double)sim->stats.total_wait_time /
                       sim->stats.total_served);
  } else {
    io_printf_both("Average wait time:     N/A\n");
  }

  io_printf_both("Max queue 0 size:      %d\n", sim->stats.max_queue_0_size);
  io_printf_both("Max queue 1 size:      %d\n", sim->stats.max_queue_1_size);
  io_printf_both("Cashier 0 served:      %d\n", sim->cashiers[0].total_served);
  io_printf_both("Cashier 1 served:      %d\n", sim->cashiers[1].total_served);
  io_printf_both("******************************\n");

  return STATUS_OK;
}