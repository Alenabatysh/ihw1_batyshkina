#include "display.h"
#include "types.h"
#include <stdio.h>
#include <string.h>

// Circular buffer for storing recent events
static Event recent_events[DISPLAY_MAX_EVENTS];
static int recent_count = 0;
static int recent_index = 0;

// Initialize display: hide cursor and clear screen
void display_init(void) {
  printf("\033[?25l");
  printf("\033[2J");
  printf("\033[H");
  fflush(stdout);
}

// Cleanup display: restore cursor visibility
void display_cleanup(void) {
  printf("\033[?25h");
  printf("\n");
  fflush(stdout);
}

// Add event to the circular buffer of recent events
void display_add_event(Event event) {
  recent_events[recent_index] = event;
  recent_index = (recent_index + 1) % DISPLAY_MAX_EVENTS;
  if (recent_count < DISPLAY_MAX_EVENTS) {
    recent_count++;
  }
}

// Draw horizontal border line
static void draw_line(void) {
  printf("|");
  for (int i = 0; i < DISPLAY_WIDTH; i++) {
    printf("=");
  }
  printf("|\n");
}

// Draw a single row with content and right padding
static void draw_row(const char *content) {
  int len = (int)strlen(content);
  printf("| %s", content);
  int padding = DISPLAY_WIDTH - len - 1;
  if (padding < 0)
    padding = 0;
  for (int i = 0; i < padding; i++) {
    printf(" ");
  }
  printf("|\n");
}

// Render the main simulation dashboard
void display_render(const Simulation *sim) {
  // Move cursor to top-left corner to overwrite previous frame
  printf("\033[H");

  // Draw header section
  draw_line();
  draw_row("SUPERMARKET SIMULATION");

  char buf[128];
  snprintf(buf, sizeof(buf), "Time: %04d / %d", sim->current_time,
           sim->config.work_day_duration);
  draw_row(buf);

  // Draw statistics section
  draw_line();
  snprintf(buf, sizeof(buf), "Arrived: %-4d  Served: %-4d  Left: %-4d",
           sim->stats.total_arrived, sim->stats.total_served,
           sim->stats.total_left_no_service);
  draw_row(buf);

  // Draw cashier and queue status section
  draw_line();
  for (int i = 0; i < NUM_CASHIERS; i++) {
    const Cashier *cashier = &sim->cashiers[i];
    const Queue *queue = &sim->queues[i];

    char row[128];
    int pos = 0;
    pos += snprintf(row + pos, sizeof(row) - pos, "Cashier %d: ", i);

    // Add customers currently in queue
    for (int j = 0; j < queue->size && pos < (int)sizeof(row) - 10; j++) {
      pos += snprintf(row + pos, sizeof(row) - pos, "[C%d]",
                      queue->customer_ids[j]);
    }

    // Pad row to fixed width
    while (pos < 30) {
      row[pos++] = ' ';
    }
    row[pos] = '\0';

    // Append cashier status
    if (cashier->is_sleeping) {
      snprintf(row + pos, sizeof(row) - pos, "sleeping");
    } else if (cashier->current_customer_id >= 0) {
      snprintf(row + pos, sizeof(row) - pos, "serving C%d",
               cashier->current_customer_id);
    } else {
      snprintf(row + pos, sizeof(row) - pos, "idle");
    }

    draw_row(row);
  }

  // Draw recent events section
  draw_line();
  draw_row("Recent events:");

  // Calculate start index for circular buffer
  int start = (recent_count < DISPLAY_MAX_EVENTS) ? 0 : recent_index;

  // Print events in reverse chronological order
  for (int i = 0; i < recent_count; i++) {
    int idx = (start + recent_count - 1 - i) % DISPLAY_MAX_EVENTS;
    Event e = recent_events[idx];

    snprintf(buf, sizeof(buf), "[%04d] %-20s entity=%d", e.time,
             event_type_names[e.type], e.entity_id);
    draw_row(buf);
  }

  // Fill empty rows if buffer is not full
  for (int i = recent_count; i < DISPLAY_MAX_EVENTS; i++) {
    draw_row("");
  }

  // Draw bottom border
  draw_line();

  // Clear remaining lines from previous frame
  printf("\033[J");

  fflush(stdout);
}