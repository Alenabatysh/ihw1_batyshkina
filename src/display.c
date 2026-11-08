#include "display.h"
#include "types.h"
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

void display_init(void) {
  printf("\033[?25l");
  printf("\033[2J");
  printf("\033[H");
  fflush(stdout);
}

void display_cleanup(void) {
  printf("\033[?25h");
  printf("\033[2J\033[H");
  fflush(stdout);
}

static void draw_line(void) {
  printf("|");
  for (int i = 0; i < DISPLAY_WIDTH; i++) {
    printf("=");
  }
  printf("|\n");
}

static void draw_row(const char *content) {
  int len = (int)strlen(content);
  printf("| %s", content);
  int padding = DISPLAY_WIDTH - len - 1;
  if (padding < 0)
    padding = 0;
  for (int i = 0; i < padding; i++)
    printf(" ");
  printf("|\n");
}

void display_render(Simulation *sim) {
  printf("\033[H");
  draw_line();
  draw_row("Supermarket simulation (processes)");

  char buf[128];
  snprintf(buf, sizeof(buf), "Time: %04d / %d", sim->current_time,
           sim->config.work_day_duration);
  draw_row(buf);
  draw_line();

  sem_wait(MUTEX_STATS);
  snprintf(buf, sizeof(buf), "Arrived: %-4d  Served: %-4d  Left: %-4d",
           sim->stats.total_arrived, sim->stats.total_served,
           sim->stats.total_left_no_service);
  sem_post(MUTEX_STATS);
  draw_row(buf);

  draw_line();

  for (int i = 0; i < NUM_CASHIERS; i++) {
    const Cashier *cashier = &sim->cashiers[i];
    const Queue *queue = &sim->queues[i];

    char row[128];
    int pos = 0;
    pos += snprintf(row + pos, sizeof(row) - pos, "Cashier %d: ", i);

    sem_wait(MUTEX_QUEUE(i));

    for (int j = 0; j < queue->size && pos < (int)sizeof(row) - 10; j++) {
      pos += snprintf(row + pos, sizeof(row) - pos, "[C%d]",
                      queue->customer_ids[j]);
    }

    int is_sleeping = cashier->is_sleeping;
    int current_cust = cashier->current_customer_id;

    sem_post(MUTEX_QUEUE(i));

    while (pos < 30)
      row[pos++] = ' ';
    row[pos] = '\0';

    if (is_sleeping) {
      snprintf(row + pos, sizeof(row) - pos, "sleeping");
    } else if (current_cust >= 0) {
      snprintf(row + pos, sizeof(row) - pos, "serving C%d", current_cust);
    } else {
      snprintf(row + pos, sizeof(row) - pos, "idle");
    }
    draw_row(row);
  }

  draw_line();
  draw_row("Recent events: (see simulation.log)");
  for (int i = 0; i < 3; i++)
    draw_row("");
  draw_line();
  printf("\033[J");
  fflush(stdout);
}