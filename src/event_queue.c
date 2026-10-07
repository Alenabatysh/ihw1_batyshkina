#include "event_queue.h"
#include <stdlib.h>

// Allocate and initialize a new event queue with the given capacity
EventQueue *eq_create(int capacity) {
  EventQueue *queue = calloc(1, sizeof(EventQueue));
  if (queue == NULL) {
    return NULL;
  }
  queue->events = calloc(capacity, sizeof(Event));
  if (queue->events == NULL) {
    return NULL;
  }
  queue->size = 0;
  queue->capacity = capacity;
  return queue;
}

// Free the memory allocated for the event queue
void eq_destroy(EventQueue *queue) {
  if (queue != NULL) {
    free(queue->events);
    free(queue);
  }
}

// Check if the event queue contains no elements
bool eq_is_empty(const EventQueue *queue) {
  if (queue == NULL) {
    return true;
  }
  return queue->size == 0;
}

// Get the current number of elements in the queue
STATUS eq_size(const EventQueue *queue, int *out_result) {
  if (queue == NULL || out_result == NULL) {
    return STATUS_NULL_PTR;
  }
  *out_result = queue->size;
  return STATUS_OK;
}

// Define priority order for events occurring at the same time
static int event_priority(EVENT_TYPE type) {
  switch (type) {
  case EVENT_FINISH_CHECKOUT:
    return 0;
  case EVENT_CASHIER_WAKE:
    return 1;
  case EVENT_START_CHECKOUT:
    return 2;
  case EVENT_QUEUE_SWITCH:
    return 3;
  case EVENT_LEAVE_NO_SERVICE:
    return 4;
  case EVENT_QUEUE_CHOICE:
    return 5;
  case EVENT_FINISH_SHOPPING:
    return 6;
  case EVENT_CUSTOMER_ARRIVE:
    return 7;
  case EVENT_END_OF_DAY:
    return 8;
  default:
    return 9;
  }
}

// Compare two events by time, then by priority
static bool event_less(const Event *a, const Event *b) {
  if (a == NULL || b == NULL) {
    return false;
  }
  if (a->time != b->time) {
    return a->time < b->time;
  }
  return event_priority(a->type) < event_priority(b->type);
}

// Swap the values of two event pointers
static void swap_events(Event *a, Event *b) {
  if (a == NULL || b == NULL) {
    return;
  }
  Event temp = *a;
  *a = *b;
  *b = temp;
}

// Restore min-heap property by moving an element up
static void shift_up(EventQueue *queue, int idx) {
  if (queue == NULL || idx < 0 || idx > queue->size - 1) {
    return;
  }
  while (idx != 0) {
    int parent_idx = (idx - 1) / 2;
    if (event_less(queue->events + idx, queue->events + parent_idx)) {
      swap_events(queue->events + idx, queue->events + parent_idx);
      idx = parent_idx;
    } else {
      break;
    }
  }
}

// Restore min-heap property by moving an element down
static void shift_down(EventQueue *queue, int idx) {
  if (queue == NULL || idx < 0 || idx > queue->size - 1) {
    return;
  }
  while (true) {
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;
    int smallest = idx;
    if (left < queue->size &&
        event_less(queue->events + left, queue->events + smallest)) {
      smallest = left;
    }
    if (right < queue->size &&
        event_less(queue->events + right, queue->events + smallest)) {
      smallest = right;
    }
    if (smallest == idx) {
      break;
    }
    swap_events(queue->events + idx, queue->events + smallest);
    idx = smallest;
  }
}

// Insert a new event into the min-heap
STATUS eq_push(EventQueue *queue, Event event) {
  if (queue == NULL) {
    return STATUS_NULL_PTR;
  }
  if (queue->size >= queue->capacity) {
    return STATUS_QUEUE_FULL;
  }
  queue->events[queue->size] = event;
  ++queue->size;
  shift_up(queue, queue->size - 1);
  return STATUS_OK;
}

// Retrieve the highest priority event without removing it
STATUS eq_peek(const EventQueue *queue, Event *out_event) {
  if (queue == NULL || out_event == NULL) {
    return STATUS_NULL_PTR;
  }
  if (queue->size == 0) {
    return STATUS_QUEUE_EMPTY;
  }
  *out_event = queue->events[0];
  return STATUS_OK;
}

// Remove and return the highest priority event from the min-heap
STATUS eq_pop(EventQueue *queue, Event *out_event) {
  if (queue == NULL || out_event == NULL) {
    return STATUS_NULL_PTR;
  }
  if (queue->size == 0) {
    return STATUS_QUEUE_EMPTY;
  }
  Event result = queue->events[0];
  *out_event = result;
  queue->events[0] = queue->events[queue->size - 1];
  --queue->size;
  shift_down(queue, 0);
  return STATUS_OK;
}