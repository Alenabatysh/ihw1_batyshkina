#ifndef TYPES_H
#define TYPES_H

#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define MAX_CUSTOMERS 1000
#define MAX_QUEUE_CAPACITY 50
#define NUM_CASHIERS 2

typedef enum {
  EVENT_CUSTOMER_ARRIVE,
  EVENT_FINISH_SHOPPING,
  EVENT_QUEUE_CHOICE,
  EVENT_LEAVE_NO_SERVICE,
  EVENT_START_CHECKOUT,
  EVENT_FINISH_CHECKOUT,
  EVENT_END_OF_DAY
} EVENT_TYPE;

typedef enum {
  STRATEGY_SHORTEST,
  STRATEGY_FIRST,
  STRATEGY_RANDOM
} QUEUE_STRATEGY;

typedef enum {
  STATUS_OK = 0,
  STATUS_ERROR,
  STATUS_QUEUE_FULL,
  STATUS_QUEUE_EMPTY,
  STATUS_NULL_PTR
} STATUS;

typedef struct {
  int total_customers, queue_capacity, shopping_time_min, shopping_time_max;
  int arrival_interval_min, arrival_interval_max, items_min, items_max;
  int item_service_time, work_day_duration, display_delay_ms;
  unsigned int random_seed;
  int time_scale;
  QUEUE_STRATEGY queue_strategy;
} Config;

// Customer entity
typedef struct {
  int id, items_count, queue_id, total_wait_time, arrival_time;
  int shopping_done_time, service_start_time;
  pid_t pid;
} Customer;

// Cashier entity
typedef struct {
  int id, is_sleeping, current_customer_id, total_served, total_service_time;
  pid_t pid;
} Cashier;

// Queue entity
typedef struct {
  int id, capacity, size, total_served;
  long long total_wait_time;
  int customer_ids[MAX_QUEUE_CAPACITY];
} Queue;

// Statistics
typedef struct {
  int total_arrived, total_served, total_left_no_service;
  long long total_wait_time, total_shopping_time;
  int max_queue_0_size, max_queue_1_size, simulation_end_time;
} Statistics;

// Main structure in shared memory
typedef struct {
  int current_time, customers_generated;
  int entrance_closed, simulation_finished;
  int interrupted;

  Config config;
  Customer customers[MAX_CUSTOMERS];
  Cashier cashiers[NUM_CASHIERS];
  Queue queues[NUM_CASHIERS];
  Statistics stats;

#ifndef USE_NAMED_SEM
  // Unnamed semaphores (must be in shared memory)
  sem_t mutex_stats;
  sem_t mutex_time;
  sem_t mutex_queues[NUM_CASHIERS];
  sem_t customers_ready[NUM_CASHIERS];
  sem_t service_done[MAX_CUSTOMERS];
#endif
} Simulation;

#ifdef USE_NAMED_SEM
// Named semaphores are stored locally in each process
extern sem_t *g_mutex_stats;
extern sem_t *g_mutex_time;
extern sem_t *g_mutex_queues[NUM_CASHIERS];
extern sem_t *g_customers_ready[NUM_CASHIERS];
extern sem_t *g_service_done[MAX_CUSTOMERS];

#define MUTEX_STATS g_mutex_stats
#define MUTEX_TIME g_mutex_time
#define MUTEX_QUEUE(i) g_mutex_queues[i]
#define CUSTOMERS_READY(i) g_customers_ready[i]
#define SERVICE_DONE(i) g_service_done[i]
#else
// Unnamed semaphores are inside the shared struct
#define MUTEX_STATS (&sim->mutex_stats)
#define MUTEX_TIME (&sim->mutex_time)
#define MUTEX_QUEUE(i) (&sim->mutex_queues[i])
#define CUSTOMERS_READY(i) (&sim->customers_ready[i])
#define SERVICE_DONE(i) (&sim->service_done[i])
#endif

#endif