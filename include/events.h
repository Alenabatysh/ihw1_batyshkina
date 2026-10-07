#ifndef EVENTS_H
#define EVENTS_H

#include "types.h"

void handle_event(Simulation *sim, Event event);

void handle_customer_arrive(Simulation *sim, Event event);

void handle_finish_shopping(Simulation *sim, Event event);

void handle_queue_choice(Simulation *sim, Event event);

void handle_queue_switch(Simulation *sim, Event event);

void handle_leave_no_service(Simulation *sim, Event event);

void handle_cashier_wake(Simulation *sim, Event event);

void handle_start_checkout(Simulation *sim, Event event);

void handle_finish_checkout(Simulation *sim, Event event);

void handle_end_of_day(Simulation *sim, Event event);

#endif