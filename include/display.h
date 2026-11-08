#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"

#define DISPLAY_WIDTH 58

// Initialize display: hide cursor and clear screen
void display_init(void);

// Cleanup display: restore cursor visibility
void display_cleanup(void);

// Render the main simulation dashboard (cashiers, queues, stats)
void display_render(Simulation *sim);

#endif