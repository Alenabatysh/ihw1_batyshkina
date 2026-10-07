#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"

#define DISPLAY_WIDTH 58
#define DISPLAY_MAX_EVENTS 5

void display_init(void);

void display_cleanup(void);

void display_render(const Simulation *sim);

void display_add_event(Event event);

#endif