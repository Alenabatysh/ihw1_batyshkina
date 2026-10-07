#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include "types.h"
#include <stdbool.h>

EventQueue *eq_create(int capacity);

void eq_destroy(EventQueue *queue);

bool eq_is_empty(const EventQueue *queue);

STATUS eq_size(const EventQueue *queue, int *out_result);

STATUS eq_push(EventQueue *queue, Event event);

STATUS eq_peek(const EventQueue *queue, Event *out_event);

STATUS eq_pop(EventQueue *queue, Event *out_event);

#endif