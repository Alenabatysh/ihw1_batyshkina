#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"

#define DISPLAY_WIDTH 58
#define DISPLAY_MAX_EVENTS 5

// Initialize display: hide cursor and clear screen
void display_init(void);

// Cleanup display: restore cursor visibility
void display_cleanup(void);

// Render the main simulation dashboard (cashiers, queues, stats)
void display_render(Simulation *sim);

// Add event to the circular buffer of recent events for display
void display_add_event(Event event);

#endif