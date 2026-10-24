#ifndef TYPES_H
#define TYPES_H

#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_CUSTOMERS 1000
#define MAX_QUEUE_CAPACITY 50
#define NUM_CASHIERS 2
#define EVENT_TYPES_COUNT 9

// Forward declaration for use in structs below
typedef struct Simulation Simulation;

// Event types in the simulation
typedef enum {
  EVENT_CUSTOMER_ARRIVE,
  EVENT_FINISH_SHOPPING,
  EVENT_QUEUE_CHOICE,
  EVENT_LEAVE_NO_SERVICE,
  EVENT_CASHIER_WAKE,
  EVENT_START_CHECKOUT,
  EVENT_FINISH_CHECKOUT,
  EVENT_END_OF_DAY
} EVENT_TYPE;

// Queue selection strategies
typedef enum {
  STRATEGY_SHORTEST,
  STRATEGY_FIRST,
  STRATEGY_RANDOM
} QUEUE_STRATEGY;

// Function return status codes
typedef enum {
  STATUS_OK = 0,
  STATUS_ERROR,
  STATUS_QUEUE_FULL,
  STATUS_QUEUE_EMPTY,
  STATUS_NULL_PTR
} STATUS;

// Simulation configuration parameters
typedef struct {
  int total_customers, queue_capacity, shopping_time_min, shopping_time_max;
  int arrival_interval_min, arrival_interval_max, items_min, items_max;
  int item_service_time, work_day_duration, display_delay_ms;
  unsigned int random_seed;
  int time_scale; // Time multiplier for visualization
  QUEUE_STRATEGY queue_strategy;
} Config;

// Customer entity
typedef struct {
  int id, items_count, queue_id, total_wait_time, arrival_time;
  int shopping_done_time, service_start_time;
  pthread_t thread; // Thread handle for the customer
  Simulation *sim;  // Pointer to simulation context
#ifdef USE_CONDVAR
  pthread_cond_t service_cond;   // Condition variable for service completion
  pthread_mutex_t service_mutex; // Mutex for condition variable
#else
  sem_t service_done; // Semaphore for service completion
#endif
} Customer;

// Cashier entity
typedef struct {
  int id, is_sleeping, current_customer_id, total_served, total_service_time;
  pthread_mutex_t mutex; // Mutex protecting cashier state
  pthread_t thread;      // Thread handle for the cashier
  Simulation *sim;       // Pointer to simulation context
#ifdef USE_CONDVAR
  pthread_cond_t customer_cond; // Condition variable for customer arrival
#else
  sem_t customer_ready; // Semaphore for customer arrival
#endif
} Cashier;

// Queue data structure
typedef struct {
  int id, capacity, size, total_served;
  long long total_wait_time;
  int *customer_ids;
  pthread_mutex_t mutex; // Mutex protecting queue operations
  Simulation *sim;       // Pointer to simulation context
#ifdef USE_CONDVAR
  pthread_cond_t item_available; // Condition variable to signal cashier
#else
  sem_t cashier_signal; // Semaphore to signal cashier
#endif
} Queue;

// Simulation statistics
typedef struct {
  int total_arrived, total_served, total_left_no_service;
  long long total_wait_time, total_shopping_time;
  int max_queue_0_size, max_queue_1_size, simulation_end_time;
  pthread_mutex_t mutex; // Mutex protecting statistics updates
} Statistics;

// Main simulation state
struct Simulation {
  int current_time, customers_count, customers_generated;
  int entrance_closed, simulation_finished;
  Config config;
  Customer customers[MAX_CUSTOMERS];
  Cashier cashiers[NUM_CASHIERS];
  Queue queues[NUM_CASHIERS];
  Statistics stats;
  pthread_mutex_t global_mutex; // Global mutex for shared state
  pthread_t display_thread;     // Thread handle for the display
};

// Event structure
typedef struct {
  EVENT_TYPE type;
  int time, entity_id, data;
} Event;

// Event type names for display
extern const char *event_type_names[];

#endif