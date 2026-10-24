#include "display.h"
#include "io.h"
#include "rng.h"
#include "types.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>

// Cashier thread: waits for customers and serves them
void *cashier_thread_func(void *arg) {
  Cashier *cashier = (Cashier *)arg;
  Simulation *sim = cashier->sim;

  while (1) {
#ifdef USE_CONDVAR
    // Wait for customer using condition variable
    pthread_mutex_lock(&sim->queues[cashier->id].mutex);
    while (sim->queues[cashier->id].size == 0 && !sim->entrance_closed &&
           !sim->simulation_finished) {
      pthread_cond_wait(&sim->queues[cashier->id].item_available,
                        &sim->queues[cashier->id].mutex);
    }
    if (sim->simulation_finished ||
        (sim->entrance_closed && sim->queues[cashier->id].size == 0)) {
      pthread_mutex_unlock(&sim->queues[cashier->id].mutex);
      break;
    }
    int cust_id = sim->queues[cashier->id].customer_ids[0];
    for (int i = 0; i < sim->queues[cashier->id].size - 1; i++) {
      sim->queues[cashier->id].customer_ids[i] =
          sim->queues[cashier->id].customer_ids[i + 1];
    }
    sim->queues[cashier->id].size--;
    pthread_mutex_unlock(&sim->queues[cashier->id].mutex);
#else
    // Wait for customer using semaphore
    sem_wait(&cashier->customer_ready);
    if (sim->entrance_closed) {
      pthread_mutex_lock(&sim->queues[cashier->id].mutex);
      int is_empty = (sim->queues[cashier->id].size == 0);
      pthread_mutex_unlock(&sim->queues[cashier->id].mutex);
      if (is_empty)
        break;
    }
    pthread_mutex_lock(&sim->queues[cashier->id].mutex);
    if (sim->queues[cashier->id].size == 0) {
      pthread_mutex_unlock(&sim->queues[cashier->id].mutex);
      continue;
    }
    int cust_id = sim->queues[cashier->id].customer_ids[0];
    for (int i = 0; i < sim->queues[cashier->id].size - 1; i++) {
      sim->queues[cashier->id].customer_ids[i] =
          sim->queues[cashier->id].customer_ids[i + 1];
    }
    sim->queues[cashier->id].size--;
    pthread_mutex_unlock(&sim->queues[cashier->id].mutex);
#endif

    // Serve the customer
    Customer *cust = &sim->customers[cust_id];
    cashier->current_customer_id = cust_id;
    cust->service_start_time = sim->current_time;
    int service_time = cust->items_count * sim->config.item_service_time;
    io_printf_log(
        "Cashier %d starts serving customer %d (items: %d, time: %d sec)\n",
        cashier->id, cust_id, cust->items_count, service_time);
    usleep(service_time * 1000 * sim->config.time_scale);

    // Update statistics safely
    pthread_mutex_lock(&sim->stats.mutex);
    cashier->total_served++;
    cashier->total_service_time += service_time;
    sim->stats.total_served++;
    cust->total_wait_time = cust->service_start_time - cust->shopping_done_time;
    sim->stats.total_wait_time += cust->total_wait_time;
    pthread_mutex_unlock(&sim->stats.mutex);

    io_printf_log("Cashier %d finished serving customer %d\n", cashier->id,
                  cust_id);
    cashier->current_customer_id = -1;

#ifdef USE_CONDVAR
    // Signal customer that service is done using condition variable
    pthread_mutex_lock(&cust->service_mutex);
    pthread_cond_signal(&cust->service_cond);
    pthread_mutex_unlock(&cust->service_mutex);
#else
    // Signal customer that service is done using semaphore
    sem_post(&cust->service_done);
#endif
  }
  return NULL;
}

// Customer thread: shops, chooses queue, waits for service
void *customer_thread_func(void *arg) {
  Customer *cust = (Customer *)arg;
  Simulation *sim = cust->sim;

  // Simulate shopping time
  int shop_time =
      rng_range(sim->config.shopping_time_min, sim->config.shopping_time_max);
  usleep(shop_time * 1000 * sim->config.time_scale);

  pthread_mutex_lock(&sim->global_mutex);
  cust->shopping_done_time = sim->current_time;
  pthread_mutex_unlock(&sim->global_mutex);
  io_printf_log("Customer %d finished shopping\n", cust->id);

  // Choose queue safely
  pthread_mutex_lock(&sim->queues[0].mutex);
  pthread_mutex_lock(&sim->queues[1].mutex);
  int q_id = 0;
  if (sim->config.queue_strategy == STRATEGY_SHORTEST) {
    q_id = (sim->queues[0].size <= sim->queues[1].size) ? 0 : 1;
  } else if (sim->config.queue_strategy == STRATEGY_FIRST) {
    q_id = 0;
  } else {
    q_id = rng_range(0, NUM_CASHIERS - 1);
  }
  pthread_mutex_unlock(&sim->queues[1].mutex);
  pthread_mutex_unlock(&sim->queues[0].mutex);

  cust->queue_id = q_id;
  io_printf_log("Customer %d chooses queue %d\n", cust->id, q_id);

  // Try to join the chosen queue
  pthread_mutex_lock(&sim->queues[q_id].mutex);
  if (sim->queues[q_id].size >= sim->queues[q_id].capacity) {
    pthread_mutex_unlock(&sim->queues[q_id].mutex);
    pthread_mutex_lock(&sim->stats.mutex);
    sim->stats.total_left_no_service++;
    pthread_mutex_unlock(&sim->stats.mutex);
    io_printf_log("Queue %d is full! Customer %d leaves without service\n",
                  q_id, cust->id);
    return NULL;
  }

  sim->queues[q_id].customer_ids[sim->queues[q_id].size++] = cust->id;

  // Update max queue size statistics
  pthread_mutex_lock(&sim->stats.mutex);
  if (q_id == 0 && sim->queues[q_id].size > sim->stats.max_queue_0_size)
    sim->stats.max_queue_0_size = sim->queues[q_id].size;
  if (q_id == 1 && sim->queues[q_id].size > sim->stats.max_queue_1_size)
    sim->stats.max_queue_1_size = sim->queues[q_id].size;
  pthread_mutex_unlock(&sim->stats.mutex);

#ifdef USE_CONDVAR
  // Wake up cashier using condition variable
  pthread_cond_signal(&sim->queues[q_id].item_available);
#else
  // Wake up cashier using semaphore
  sem_post(&sim->cashiers[q_id].customer_ready);
#endif
  pthread_mutex_unlock(&sim->queues[q_id].mutex);

#ifdef USE_CONDVAR
  // Wait for service to complete using condition variable
  pthread_mutex_lock(&cust->service_mutex);
  pthread_cond_wait(&cust->service_cond, &cust->service_mutex);
  pthread_mutex_unlock(&cust->service_mutex);
#else
  // Wait for service to complete using semaphore
  sem_wait(&cust->service_done);
#endif

  return NULL;
}

// Generator thread: creates customer threads over time
void *generator_thread_func(void *arg) {
  Simulation *sim = (Simulation *)arg;
  for (int i = 0; i < sim->config.total_customers; i++) {
    int interval = rng_range(sim->config.arrival_interval_min,
                             sim->config.arrival_interval_max);
    usleep(interval * 1000 * sim->config.time_scale);

    // Check for interruption
    if (sim->entrance_closed) {
      io_printf_log("Generator thread: interrupted, stopping new customers.\n");
      break;
    }

    pthread_mutex_lock(&sim->global_mutex);
    sim->current_time += interval;
    sim->stats.total_arrived++;
    pthread_mutex_unlock(&sim->global_mutex);

    Customer *cust = &sim->customers[i];
    cust->id = i;
    cust->items_count = rng_range(sim->config.items_min, sim->config.items_max);
    cust->sim = sim;

#ifdef USE_CONDVAR
    pthread_cond_init(&cust->service_cond, NULL);
    pthread_mutex_init(&cust->service_mutex, NULL);
#else
    sem_init(&cust->service_done, 0, 0);
#endif

    io_printf_log("Customer %d arrived (items: %d)\n", i, cust->items_count);
    pthread_create(&cust->thread, NULL, customer_thread_func, cust);
    sim->customers_generated++;
  }
  sim->entrance_closed = 1;
  io_printf_log(
      "End of day! Entrance closed. Waiting for remaining customers...\n");
  return NULL;
}

// Display thread: renders the UI in real-time without blocking simulation
void *display_thread_func(void *arg) {
  Simulation *sim = (Simulation *)arg;
  while (!sim->simulation_finished) {
    display_render(sim);
    usleep(50000); // Sleep 50ms to update UI smoothly
  }
  display_render(sim); // Final render
  return NULL;
}