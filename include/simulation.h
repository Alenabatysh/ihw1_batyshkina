#ifndef SIMULATION_H
#define SIMULATION_H

#include "types.h"

// Initialize simulation state, mutexes, and semaphores/condvars
STATUS sim_init(Simulation *sim, const Config *config);

// Destroy mutexes, semaphores/condvars, and free allocated memory
void sim_destroy(Simulation *sim);

// Get customer pointer by ID
Customer *sim_get_customer(Simulation *sim, int customer_id);

// Get cashier pointer by ID
Cashier *sim_get_cashier(Simulation *sim, int cashier_id);

// Get queue pointer by ID
Queue *sim_get_queue(Simulation *sim, int queue_id);

// Start cashier, generator, and display threads
void sim_start(Simulation *sim);

// Wait for all customer, cashier, and display threads to finish
void sim_wait_completion(Simulation *sim);

// Print final simulation statistics to console and log
STATUS sim_print_stats(const Simulation *sim);

#endif