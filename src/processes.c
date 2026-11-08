#include "io.h"
#include "rng.h"
#include "simulation.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Cashier process routine
void cashier_process(Simulation *sim, int cashier_id) {
  while (1) {
    sem_wait(MUTEX_STATS);
    int interrupted = sim->interrupted || sim->simulation_finished;
    int entrance_closed = sim->entrance_closed;
    int all_processed =
        (sim->stats.total_served + sim->stats.total_left_no_service >=
         sim->stats.total_arrived);
    sem_post(MUTEX_STATS);

    if (interrupted)
      break;

    if (entrance_closed) {
      sem_wait(MUTEX_QUEUE(cashier_id));
      int q_size = sim->queues[cashier_id].size;
      sem_post(MUTEX_QUEUE(cashier_id));

      if (q_size == 0 && all_processed) {
        break;
      }
    }

    sem_wait(CUSTOMERS_READY(cashier_id));

    sem_wait(MUTEX_QUEUE(cashier_id));
    if (sim->queues[cashier_id].size == 0) {
      sem_post(MUTEX_QUEUE(cashier_id));
      continue;
    }

    int cust_id = sim->queues[cashier_id].customer_ids[0];
    for (int i = 0; i < sim->queues[cashier_id].size - 1; i++) {
      sim->queues[cashier_id].customer_ids[i] =
          sim->queues[cashier_id].customer_ids[i + 1];
    }
    sim->queues[cashier_id].size--;
    sem_post(MUTEX_QUEUE(cashier_id));

    sem_wait(MUTEX_QUEUE(cashier_id));
    sim->cashiers[cashier_id].is_sleeping = 0;
    sim->cashiers[cashier_id].current_customer_id = cust_id;
    sem_post(MUTEX_QUEUE(cashier_id));

    Customer *cust = &sim->customers[cust_id];

    sem_wait(MUTEX_TIME);
    cust->service_start_time = sim->current_time;
    sem_post(MUTEX_TIME);

    int service_time = cust->items_count * sim->config.item_service_time;

    io_printf_log(
        "Cashier %d starts serving customer %d (items: %d, time: %d sec)\n",
        cashier_id, cust_id, cust->items_count, service_time);

    usleep(service_time * 1000 * sim->config.time_scale);

    sem_wait(MUTEX_STATS);
    sim->cashiers[cashier_id].total_served++;
    sim->cashiers[cashier_id].total_service_time += service_time;
    sim->stats.total_served++;
    cust->total_wait_time = cust->service_start_time - cust->shopping_done_time;
    sim->stats.total_wait_time += cust->total_wait_time;
    sem_post(MUTEX_STATS);

    io_printf_log("Cashier %d finished serving customer %d\n", cashier_id,
                  cust_id);

    sem_wait(MUTEX_QUEUE(cashier_id));
    sim->cashiers[cashier_id].current_customer_id = -1;
    sim->cashiers[cashier_id].is_sleeping = 1;
    sem_post(MUTEX_QUEUE(cashier_id));

    sem_post(SERVICE_DONE(cust_id));
  }
}

// Customer process routine
void customer_process(Simulation *sim, int customer_id) {
  Customer *cust = &sim->customers[customer_id];

  int shop_time =
      rng_range(sim->config.shopping_time_min, sim->config.shopping_time_max);
  usleep(shop_time * 1000 * sim->config.time_scale);

  if (sim->interrupted) {
    sem_wait(MUTEX_STATS);
    sim->stats.total_left_no_service++;
    sem_post(MUTEX_STATS);
    return;
  }

  sem_wait(MUTEX_TIME);
  cust->shopping_done_time = sim->current_time;
  sem_post(MUTEX_TIME);

  sem_wait(MUTEX_STATS);
  sim->stats.total_shopping_time +=
      (cust->shopping_done_time - cust->arrival_time);
  sem_post(MUTEX_STATS);

  io_printf_log("Customer %d finished shopping\n", cust->id);

  sem_wait(MUTEX_QUEUE(0));
  sem_wait(MUTEX_QUEUE(1));

  int q_id = 0;
  if (sim->config.queue_strategy == STRATEGY_SHORTEST) {
    q_id = (sim->queues[0].size <= sim->queues[1].size) ? 0 : 1;
  } else if (sim->config.queue_strategy == STRATEGY_FIRST) {
    q_id = 0;
  } else {
    q_id = rng_range(0, NUM_CASHIERS - 1);
  }

  sem_post(MUTEX_QUEUE(1));
  sem_post(MUTEX_QUEUE(0));

  cust->queue_id = q_id;
  io_printf_log("Customer %d chooses queue %d\n", cust->id, q_id);

  if (sim->interrupted) {
    sem_wait(MUTEX_STATS);
    sim->stats.total_left_no_service++;
    sem_post(MUTEX_STATS);
    return;
  }

  int final_q = q_id;
  sem_wait(MUTEX_QUEUE(final_q));

  if (sim->queues[final_q].size >= sim->queues[final_q].capacity) {
    sem_post(MUTEX_QUEUE(final_q));

    int alt_id = (final_q == 0) ? 1 : 0;
    sem_wait(MUTEX_QUEUE(alt_id));

    if (sim->queues[alt_id].size >= sim->queues[alt_id].capacity) {
      sem_post(MUTEX_QUEUE(alt_id));
      sem_wait(MUTEX_STATS);
      sim->stats.total_left_no_service++;
      sem_post(MUTEX_STATS);
      io_printf_log("Both queues full! Customer %d leaves without service\n",
                    cust->id);
      return;
    }

    io_printf_log("Queue %d full, customer %d switches to queue %d\n", final_q,
                  cust->id, alt_id);
    final_q = alt_id;
  }

  sim->queues[final_q].customer_ids[sim->queues[final_q].size++] = cust->id;
  cust->queue_id = final_q;

  sem_wait(MUTEX_STATS);
  if (final_q == 0 && sim->queues[final_q].size > sim->stats.max_queue_0_size)
    sim->stats.max_queue_0_size = sim->queues[final_q].size;
  if (final_q == 1 && sim->queues[final_q].size > sim->stats.max_queue_1_size)
    sim->stats.max_queue_1_size = sim->queues[final_q].size;
  sem_post(MUTEX_STATS);

  sem_post(MUTEX_QUEUE(final_q));

  sem_post(CUSTOMERS_READY(final_q));

  sem_wait(SERVICE_DONE(cust->id));
}