#include "simulation.h"
#include "display.h"
#include "event_queue.h"
#include "events.h"
#include "io.h"
#include "rng.h"
#include <stdlib.h>
#include <unistd.h>

// Initialize simulation state: cashiers, queues, statistics, and event queue
STATUS sim_init(Simulation *sim, const Config *config) {
  if (sim == NULL || config == NULL) {
    return STATUS_NULL_PTR;
  }

  // Copy configuration
  sim->config = *config;

  // Reset counters
  sim->customers_count = 0;
  sim->current_time = 0;
  sim->customers_generated = 0;
  sim->entrance_closed = 0;

  // Initialize cashiers: all sleeping, no current customer
  for (int i = 0; i < NUM_CASHIERS; i++) {
    sim->cashiers[i].id = i;
    sim->cashiers[i].is_sleeping = 1;
    sim->cashiers[i].current_customer_id = -1;
    sim->cashiers[i].total_served = 0;
    sim->cashiers[i].total_service_time = 0;
  }

  // Create queues with dynamic arrays for customer IDs
  for (int i = 0; i < NUM_CASHIERS; i++) {
    sim->queues[i].id = i;
    sim->queues[i].capacity = config->queue_capacity;
    sim->queues[i].size = 0;
    sim->queues[i].customer_ids = calloc(config->queue_capacity, sizeof(int));
    if (sim->queues[i].customer_ids == NULL) {
      // Rollback already allocated memory
      for (int j = 0; j < i; j++) {
        free(sim->queues[j].customer_ids);
      }
      return STATUS_ERROR;
    }
    sim->queues[i].total_served = 0;
    sim->queues[i].total_wait_time = 0;
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

  // Create event queue with estimated capacity
  int eq_capacity = config->total_customers * 5 + 100;
  sim->event_queue = eq_create(eq_capacity);
  if (sim->event_queue == NULL) {
    // Rollback queue memory
    for (int i = 0; i < NUM_CASHIERS; i++) {
      free(sim->queues[i].customer_ids);
    }
    return STATUS_ERROR;
  }

  return STATUS_OK;
}

// Free dynamically allocated memory for queues and event queue
void sim_destroy(Simulation *sim) {
  if (sim == NULL) {
    return;
  }

  // Free customer ID arrays in each queue
  for (int i = 0; i < NUM_CASHIERS; i++) {
    if (sim->queues[i].customer_ids != NULL) {
      free(sim->queues[i].customer_ids);
      sim->queues[i].customer_ids = NULL;
    }
  }

  // Destroy event queue
  if (sim->event_queue != NULL) {
    eq_destroy(sim->event_queue);
    sim->event_queue = NULL;
  }
}

// Add event to the event queue
STATUS sim_add_event(Simulation *sim, Event event) {
  if (sim == NULL || sim->event_queue == NULL) {
    return STATUS_NULL_PTR;
  }
  return eq_push(sim->event_queue, event);
}

// Get customer pointer by ID
Customer *sim_get_customer(Simulation *sim, int customer_id) {
  if (sim == NULL || customer_id < 0 || customer_id >= sim->customers_count) {
    return NULL;
  }
  return &sim->customers[customer_id];
}

// Get cashier pointer by ID
Cashier *sim_get_cashier(Simulation *sim, int cashier_id) {
  if (sim == NULL || cashier_id < 0 || cashier_id >= NUM_CASHIERS) {
    return NULL;
  }
  return &sim->cashiers[cashier_id];
}

// Get queue pointer by ID
Queue *sim_get_queue(Simulation *sim, int queue_id) {
  if (sim == NULL || queue_id < 0 || queue_id >= NUM_CASHIERS) {
    return NULL;
  }
  return &sim->queues[queue_id];
}

// Add customer to the end of queue and update max size statistics
STATUS sim_add_to_queue(Simulation *sim, int queue_id, int customer_id) {
  Queue *queue = sim_get_queue(sim, queue_id);
  if (queue == NULL) {
    return STATUS_ERROR;
  }
  if (queue->size >= queue->capacity) {
    return STATUS_QUEUE_FULL;
  }
  queue->customer_ids[queue->size] = customer_id;
  queue->size++;

  // Update maximum queue size statistics
  if (queue_id == 0 && queue->size > sim->stats.max_queue_0_size) {
    sim->stats.max_queue_0_size = queue->size;
  }
  if (queue_id == 1 && queue->size > sim->stats.max_queue_1_size) {
    sim->stats.max_queue_1_size = queue->size;
  }

  return STATUS_OK;
}

// Remove customer from queue by shifting remaining elements
STATUS sim_remove_from_queue(Simulation *sim, int queue_id, int customer_id) {
  Queue *queue = sim_get_queue(sim, queue_id);
  if (queue == NULL) {
    return STATUS_ERROR;
  }

  // Find customer index in queue
  int found_index = -1;
  for (int i = 0; i < queue->size; i++) {
    if (queue->customer_ids[i] == customer_id) {
      found_index = i;
      break;
    }
  }

  if (found_index == -1) {
    return STATUS_ERROR;
  }

  // Shift elements left to fill the gap
  for (int i = found_index; i < queue->size - 1; i++) {
    queue->customer_ids[i] = queue->customer_ids[i + 1];
  }
  queue->size--;

  return STATUS_OK;
}

// Choose queue according to the configured strategy
int sim_choose_queue(Simulation *sim) {
  switch (sim->config.queue_strategy) {
  case STRATEGY_SHORTEST:
    // Select queue with minimum size
    if (sim->queues[0].size <= sim->queues[1].size) {
      return 0;
    }
    return 1;

  case STRATEGY_FIRST:
    // Always select first queue
    return 0;

  case STRATEGY_RANDOM:
    // Select random queue
    return rng_range(0, NUM_CASHIERS - 1);

  default:
    return 0;
  }
}

// Schedule initial events: first customer arrival and end of day
void sim_schedule_initial(Simulation *sim) {
  // First customer arrives at time 0
  Event first_arrive = {
      .type = EVENT_CUSTOMER_ARRIVE, .time = 0, .entity_id = 0, .data = 0};
  sim_add_event(sim, first_arrive);
  sim->customers_generated = 1;

  // End of day event
  Event end_of_day = {.type = EVENT_END_OF_DAY,
                      .time = sim->config.work_day_duration,
                      .entity_id = 0,
                      .data = 0};
  sim_add_event(sim, end_of_day);
}

// Main simulation loop: extract and process events until queue is empty
void sim_run(Simulation *sim) {
  if (sim == NULL) {
    return;
  }

  Event event;
  while (!eq_is_empty(sim->event_queue)) {
    STATUS status = eq_pop(sim->event_queue, &event);
    if (status != STATUS_OK) {
      break;
    }

    sim->current_time = event.time;

    // Process event and update display
    handle_event(sim, event);
    display_render(sim);

    // Break if entrance is closed and no more events
    if (sim->entrance_closed && eq_is_empty(sim->event_queue)) {
      break;
    }

    // Delay for visualization if configured
    if (sim->config.display_delay_ms > 0) {
      usleep(sim->config.display_delay_ms * 1000);
    }
  }

  sim->stats.simulation_end_time = sim->current_time;
}

// Print final simulation statistics to console and log
STATUS sim_print_stats(const Simulation *sim) {
  if (sim == NULL) {
    return STATUS_NULL_PTR;
  }

  io_printf_both("\n");
  io_printf_both("******************************\n");
  io_printf_both("    SIMULATION STATISTICS    \n");
  io_printf_both("Simulation duration:   %d sec\n",
                 sim->stats.simulation_end_time);
  io_printf_both("Total arrived:         %d\n", sim->stats.total_arrived);
  io_printf_both("Total served:          %d\n", sim->stats.total_served);
  io_printf_both("Total left (no serv.): %d\n",
                 sim->stats.total_left_no_service);

  // Calculate and print average wait time
  if (sim->stats.total_served > 0) {
    double avg_wait =
        (double)sim->stats.total_wait_time / sim->stats.total_served;
    io_printf_both("Average wait time:     %.2f sec\n", avg_wait);
  } else {
    io_printf_both("Average wait time:     N/A\n");
  }

  // Calculate and print average shopping time
  if (sim->stats.total_arrived > 0) {
    double avg_shop =
        (double)sim->stats.total_shopping_time / sim->stats.total_arrived;
    io_printf_both("Average shopping time: %.2f sec\n", avg_shop);
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