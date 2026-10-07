#ifndef SIMULATION_H
#define SIMULATION_H

#include "types.h"

STATUS sim_init(Simulation *sim, const Config *config);

void sim_destroy(Simulation *sim);

STATUS sim_add_event(Simulation *sim, Event event);

Customer *sim_get_customer(Simulation *sim, int customer_id);

Cashier *sim_get_cashier(Simulation *sim, int cashier_id);

Queue *sim_get_queue(Simulation *sim, int queue_id);

STATUS sim_add_to_queue(Simulation *sim, int queue_id, int customer_id);

STATUS sim_remove_from_queue(Simulation *sim, int queue_id, int customer_id);

int sim_choose_queue(Simulation *sim);

void sim_schedule_initial(Simulation *sim);

void sim_run(Simulation *sim);

STATUS sim_print_stats(const Simulation *sim);

#endif