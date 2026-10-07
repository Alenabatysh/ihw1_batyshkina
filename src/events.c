#include "events.h"
#include "display.h"
#include "io.h"
#include "rng.h"
#include "simulation.h"
#include <stdio.h>

// Event dispatcher: log event, add to display, and call specific handler
void handle_event(Simulation *sim, Event event) {
  io_printf_log("[%04d] %-20s entity=%d data=%d\n", event.time,
                event_type_names[event.type], event.entity_id, event.data);

  display_add_event(event);

  switch (event.type) {
  case EVENT_CUSTOMER_ARRIVE:
    handle_customer_arrive(sim, event);
    break;
  case EVENT_FINISH_SHOPPING:
    handle_finish_shopping(sim, event);
    break;
  case EVENT_QUEUE_CHOICE:
    handle_queue_choice(sim, event);
    break;
  case EVENT_QUEUE_SWITCH:
    handle_queue_switch(sim, event);
    break;
  case EVENT_LEAVE_NO_SERVICE:
    handle_leave_no_service(sim, event);
    break;
  case EVENT_CASHIER_WAKE:
    handle_cashier_wake(sim, event);
    break;
  case EVENT_START_CHECKOUT:
    handle_start_checkout(sim, event);
    break;
  case EVENT_FINISH_CHECKOUT:
    handle_finish_checkout(sim, event);
    break;
  case EVENT_END_OF_DAY:
    handle_end_of_day(sim, event);
    break;
  default:
    io_printf_log("ERROR: Unknown event type %d\n", event.type);
  }
}

// Process customer arrival: create entity and schedule next events
void handle_customer_arrive(Simulation *sim, Event event) {
  Customer *customer = &sim->customers[sim->customers_count];
  customer->id = event.entity_id;
  customer->items_count =
      rng_range(sim->config.items_min, sim->config.items_max);
  customer->queue_id = -1;
  customer->total_wait_time = 0;
  customer->arrival_time = event.time;
  customer->shopping_done_time = 0;

  sim->customers_count++;
  sim->stats.total_arrived++;

  io_printf_log("Customer %d arrived (items: %d)\n", customer->id,
                customer->items_count);

  // Schedule shopping completion
  int shopping_time =
      rng_range(sim->config.shopping_time_min, sim->config.shopping_time_max);
  Event finish_shopping = {.type = EVENT_FINISH_SHOPPING,
                           .time = event.time + shopping_time,
                           .entity_id = customer->id,
                           .data = 0};
  sim_add_event(sim, finish_shopping);

  io_printf_log("Will finish shopping at %d (shopping time: %d)\n",
                finish_shopping.time, shopping_time);

  // Schedule next customer arrival if limit not reached and entrance is open
  if (sim->customers_generated < sim->config.total_customers &&
      !sim->entrance_closed) {
    int interval = rng_range(sim->config.arrival_interval_min,
                             sim->config.arrival_interval_max);
    Event next_arrive = {.type = EVENT_CUSTOMER_ARRIVE,
                         .time = event.time + interval,
                         .entity_id = sim->customers_generated,
                         .data = 0};
    sim_add_event(sim, next_arrive);
    sim->customers_generated++;

    io_printf_log("Next customer arrives at %d (interval: %d)\n",
                  next_arrive.time, interval);
  }
}

// Process shopping completion: update time and schedule queue choice
void handle_finish_shopping(Simulation *sim, Event event) {
  Customer *customer = sim_get_customer(sim, event.entity_id);
  if (customer == NULL)
    return;

  customer->shopping_done_time = event.time;
  sim->stats.total_shopping_time +=
      (customer->shopping_done_time - customer->arrival_time);

  io_printf_log("Customer %d finished shopping\n", customer->id);

  Event queue_choice = {.type = EVENT_QUEUE_CHOICE,
                        .time = event.time,
                        .entity_id = customer->id,
                        .data = 0};
  sim_add_event(sim, queue_choice);
}

// Process queue selection: add to queue or schedule leave if full
void handle_queue_choice(Simulation *sim, Event event) {
  Customer *customer = sim_get_customer(sim, event.entity_id);
  if (customer == NULL)
    return;

  int queue_id = sim_choose_queue(sim);
  customer->queue_id = queue_id;

  io_printf_log("Customer %d chooses queue %d (strategy: %d)\n", customer->id,
                queue_id, sim->config.queue_strategy);

  Queue *queue = sim_get_queue(sim, queue_id);
  if (queue->size >= queue->capacity) {
    io_printf_log("Queue %d is full! Customer %d leaves\n", queue_id,
                  customer->id);

    Event leave = {.type = EVENT_LEAVE_NO_SERVICE,
                   .time = event.time,
                   .entity_id = customer->id,
                   .data = 0};
    sim_add_event(sim, leave);
    return;
  }

  STATUS status = sim_add_to_queue(sim, queue_id, customer->id);
  if (status != STATUS_OK) {
    io_printf_log("ERROR: Failed to add customer to queue\n");
    return;
  }

  io_printf_log("Customer %d added to queue %d (size: %d)\n", customer->id,
                queue_id, queue->size);

  // Wake up cashier if sleeping, otherwise check queue state
  Cashier *cashier = sim_get_cashier(sim, queue_id);
  if (cashier->is_sleeping) {
    Event wake = {.type = EVENT_CASHIER_WAKE,
                  .time = event.time,
                  .entity_id = queue_id,
                  .data = 0};
    sim_add_event(sim, wake);
    io_printf_log("Waking up cashier %d\n", queue_id);
  } else {
    Event queue_switch = {.type = EVENT_QUEUE_SWITCH,
                          .time = event.time,
                          .entity_id = queue_id,
                          .data = 0};
    sim_add_event(sim, queue_switch);
  }
}

// Process customer leaving without service: update statistics
void handle_leave_no_service(Simulation *sim, Event event) {
  Customer *customer = sim_get_customer(sim, event.entity_id);
  if (customer == NULL)
    return;

  sim->stats.total_left_no_service++;

  io_printf_log("Customer %d left without service (waited: %d sec)\n",
                customer->id, event.time - customer->arrival_time);
}

// Process cashier wake up: change state and start checkout for the first customer
void handle_cashier_wake(Simulation *sim, Event event) {
  int queue_id = event.entity_id;
  Cashier *cashier = sim_get_cashier(sim, queue_id);
  if (cashier == NULL)
    return;

  cashier->is_sleeping = 0;

  io_printf_log("Cashier %d woke up\n", queue_id);

  Queue *queue = sim_get_queue(sim, queue_id);
  if (queue->size > 0) {
    int first_customer_id = queue->customer_ids[0];
    Event start_checkout = {.type = EVENT_START_CHECKOUT,
                            .time = event.time,
                            .entity_id = queue_id,
                            .data = first_customer_id};
    sim_add_event(sim, start_checkout);
  }
}

// Process checkout start: assign customer to cashier and schedule finish
void handle_start_checkout(Simulation *sim, Event event) {
  int queue_id = event.entity_id;
  int customer_id = event.data;

  Cashier *cashier = sim_get_cashier(sim, queue_id);
  Customer *customer = sim_get_customer(sim, customer_id);
  if (cashier == NULL || customer == NULL)
    return;

  cashier->current_customer_id = customer_id;

  int service_time = customer->items_count * sim->config.item_service_time;

  io_printf_log("Cashier %d starts serving customer %d (items: %d, time: %d)\n",
                queue_id, customer_id, customer->items_count, service_time);

  customer->total_wait_time = event.time - customer->shopping_done_time;
  sim->stats.total_wait_time += customer->total_wait_time;

  Event finish_checkout = {.type = EVENT_FINISH_CHECKOUT,
                           .time = event.time + service_time,
                           .entity_id = queue_id,
                           .data = customer_id};
  sim_add_event(sim, finish_checkout);
}

// Process checkout finish: update stats, remove from queue, and serve next or sleep
void handle_finish_checkout(Simulation *sim, Event event) {
  int queue_id = event.entity_id;
  int customer_id = event.data;

  Cashier *cashier = sim_get_cashier(sim, queue_id);
  Customer *customer = sim_get_customer(sim, customer_id);
  Queue *queue = sim_get_queue(sim, queue_id);
  if (cashier == NULL || customer == NULL || queue == NULL)
    return;

  cashier->total_served++;
  cashier->total_service_time +=
      (customer->items_count * sim->config.item_service_time);
  queue->total_served++;
  queue->total_wait_time += customer->total_wait_time;
  sim->stats.total_served++;

  io_printf_log("Cashier %d finished serving customer %d (total served: %d)\n",
                queue_id, customer_id, cashier->total_served);

  sim_remove_from_queue(sim, queue_id, customer_id);

  cashier->current_customer_id = -1;

  // Serve next customer or put cashier to sleep
  if (queue->size > 0) {
    int next_customer_id = queue->customer_ids[0];
    Event start_checkout = {.type = EVENT_START_CHECKOUT,
                            .time = event.time,
                            .entity_id = queue_id,
                            .data = next_customer_id};
    sim_add_event(sim, start_checkout);
    io_printf_log("Next customer in queue: %d\n", next_customer_id);
  } else {
    cashier->is_sleeping = 1;
    io_printf_log("Queue %d is empty, cashier %d goes to sleep\n", queue_id,
                  queue_id);
  }
}

// Process queue switch check: start checkout if cashier is idle and queue is not empty
void handle_queue_switch(Simulation *sim, Event event) {
  int queue_id = event.entity_id;
  Cashier *cashier = sim_get_cashier(sim, queue_id);
  Queue *queue = sim_get_queue(sim, queue_id);
  if (cashier == NULL || queue == NULL)
    return;

  if (cashier->current_customer_id == -1 && queue->size > 0 &&
      !cashier->is_sleeping) {
    int first_customer_id = queue->customer_ids[0];
    Event start_checkout = {.type = EVENT_START_CHECKOUT,
                            .time = event.time,
                            .entity_id = queue_id,
                            .data = first_customer_id};
    sim_add_event(sim, start_checkout);
    io_printf_log("QUEUE_SWITCH: Starting checkout for customer %d\n",
                  first_customer_id);
  }
}

// Process end of day: close entrance for new customers
void handle_end_of_day(Simulation *sim, Event event) {
  sim->entrance_closed = 1;

  io_printf_log("END OF DAY at time %d! Entrance closed.\n", event.time);
  io_printf_log("Waiting for all customers to be served...\n");
}