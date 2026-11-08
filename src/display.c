#include "display.h"
#include "types.h"
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

// Initialize display: hide cursor and clear screen
void display_init(void) {
  printf("\033[?25l"); // Hide cursor
  printf("\033[2J");   // Clear screen
  printf("\033[H");    // Move cursor to top-left
  fflush(stdout);
}

// Cleanup display: restore cursor and clear screen after simulation ends
void display_cleanup(void) {
  printf("\033[?25h");     // Show cursor
  printf("\033[2J\033[H"); // Clear screen and reset cursor
  fflush(stdout);
}

// Draw horizontal separator line
static void draw_line(void) {
  printf("|");
  for (int i = 0; i < DISPLAY_WIDTH; i++) {
    printf("=");
  }
  printf("|\n");
}

// Draw a single row with content and right padding to fixed width
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

// Render main dashboard: time, stats, cashiers, queues
void display_render(Simulation *sim) {
  printf("\033[H"); // Move cursor to top-left to overwrite previous frame

  draw_line();
  draw_row("Supermarket simulation (processes)");

  char buf[128];
  snprintf(buf, sizeof(buf), "Time: %04d / %d", sim->current_time,
           sim->config.work_day_duration);
  draw_row(buf);
  draw_line();

  // Read statistics under lock to avoid reading partially-updated data
  sem_wait(MUTEX_STATS);
  snprintf(buf, sizeof(buf), "Arrived: %-4d  Served: %-4d  Left: %-4d",
           sim->stats.total_arrived, sim->stats.total_served,
           sim->stats.total_left_no_service);
  sem_post(MUTEX_STATS);
  draw_row(buf);

  draw_line();

  // Render each cashier's queue and status
  for (int i = 0; i < NUM_CASHIERS; i++) {
    const Cashier *cashier = &sim->cashiers[i];
    const Queue *queue = &sim->queues[i];

    char row[128];
    int pos = 0;
    pos += snprintf(row + pos, sizeof(row) - pos, "Cashier %d: ", i);

    // Lock queue to read its contents safely
    sem_wait(MUTEX_QUEUE(i));

    for (int j = 0; j < queue->size && pos < (int)sizeof(row) - 10; j++) {
      pos += snprintf(row + pos, sizeof(row) - pos, "[C%d]",
                      queue->customer_ids[j]);
    }

    int is_sleeping = cashier->is_sleeping;
    int current_cust = cashier->current_customer_id;

    sem_post(MUTEX_QUEUE(i));

    // Pad row to align status column
    while (pos < 30)
      row[pos++] = ' ';
    row[pos] = '\0';

    // Append cashier status
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

  printf("\033[J"); // Clear remaining lines from previous frame
  fflush(stdout);
}