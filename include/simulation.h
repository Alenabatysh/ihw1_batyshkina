#ifndef SIMULATION_H
#define SIMULATION_H

#include "types.h"

// Initialize simulation state, mutexes, and semaphores
STATUS sim_init(Simulation *sim, const Config *config);

// Destroy mutexes, semaphores, and free allocated memory
void sim_destroy(Simulation *sim);

// Get customer pointer by ID
Customer *sim_get_customer(Simulation *sim, int customer_id);

// Get cashier pointer by ID
Cashier *sim_get_cashier(Simulation *sim, int cashier_id);

// Get queue pointer by ID
Queue *sim_get_queue(Simulation *sim, int queue_id);

// Print final simulation statistics to console and log
STATUS sim_print_stats(const Simulation *sim);

// Cashier process routine: waits for customers, serves them, updates stats
void cashier_process(Simulation *sim, int cashier_id);

// Customer process routine: shops, chooses queue, waits for service
void customer_process(Simulation *sim, int customer_id);

// Attach to existing named semaphores (for independent processes)
void sim_attach_named_sems(Simulation *sim);

#endif