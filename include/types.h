#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <stdint.h>

#define MAX_CUSTOMERS 1000
#define MAX_QUEUE_CAPACITY 50
#define NUM_CASHIERS 2
#define EVENT_TYPES_COUNT 9

// Simulation event types
typedef enum {
  EVENT_CUSTOMER_ARRIVE,
  EVENT_FINISH_SHOPPING,
  EVENT_QUEUE_CHOICE,
  EVENT_QUEUE_SWITCH,
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
  int total_customers;
  int queue_capacity;
  int shopping_time_min;
  int shopping_time_max;
  int arrival_interval_min;
  int arrival_interval_max;
  int items_min;
  int items_max;
  int item_service_time;
  QUEUE_STRATEGY queue_strategy;
  int work_day_duration;
  unsigned int random_seed;
  int display_delay_ms;
} Config;

// Customer entity
typedef struct {
  int id;
  int items_count;
  int queue_id;
  int total_wait_time;
  int arrival_time;
  int shopping_done_time;
} Customer;

// Cashier entity
typedef struct {
  int id;
  int is_sleeping;
  int current_customer_id;
  int total_served;
  int total_service_time;
} Cashier;

// Queue data structure
typedef struct {
  int id;
  int capacity;
  int size;
  int *customer_ids;
  int total_served;
  long long total_wait_time;
} Queue;

// Event structure
typedef struct {
  EVENT_TYPE type;
  int time;
  int entity_id;
  int data;
} Event;

// Min-heap event queue
typedef struct {
  Event *events;
  int size;
  int capacity;
} EventQueue;

// Simulation statistics
typedef struct {
  int total_arrived;
  int total_served;
  int total_left_no_service;
  long long total_wait_time;
  long long total_shopping_time;
  int max_queue_0_size;
  int max_queue_1_size;
  int simulation_end_time;
} Statistics;

// Main simulation state
typedef struct {
  int current_time;
  int customers_count;
  int customers_generated;
  int entrance_closed;
  Config config;
  Customer customers[MAX_CUSTOMERS];
  Cashier cashiers[NUM_CASHIERS];
  Queue queues[NUM_CASHIERS];
  Statistics stats;
  EventQueue *event_queue;
} Simulation;

// Event type names for display
extern const char *event_type_names[];

#endif