#include "config.h"
#include "io.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Set default configuration values
void config_set_defaults(Config *config) {
  config->total_customers = 100;
  config->queue_capacity = 10;
  config->shopping_time_min = 5;
  config->shopping_time_max = 60;
  config->arrival_interval_min = 1;
  config->arrival_interval_max = 60;
  config->items_min = 1;
  config->items_max = 10;
  config->item_service_time = 25;
  config->queue_strategy = STRATEGY_SHORTEST;
  config->work_day_duration = 3600;
  config->random_seed = 0;
  config->display_delay_ms = 25;
}

// Load configuration from file, parse key=value pairs
STATUS config_load(const char *filename, Config *config) {
  if (filename == NULL || config == NULL) {
    return STATUS_NULL_PTR;
  }
  config_set_defaults(config);

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return STATUS_ERROR;
  }

  char buffer[4096];
  ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (bytes <= 0) {
    return STATUS_ERROR;
  }

  // Null-terminate buffer for string operations
  buffer[bytes] = '\0';
  char *line = buffer;
  while (*line != '\0') {
    // Find end of current line
    char *newline = strchr(line, '\n');
    if (newline != NULL) {
      *newline = '\0';
    }

    // Skip leading whitespace
    char *part = line;
    while (isspace((unsigned char)(*part))) {
      ++part;
      continue;
    }

    // Skip empty lines and comments
    if (*part == '\0' || *part == '#') {
      if (newline != NULL) {
        line = newline + 1;
        continue;
      } else {
        break;
      }
    }

    // Find '=' separator
    char *equal = strchr(part, '=');
    if (equal == NULL) {
      if (newline != NULL) {
        line = newline + 1;
        continue;
      } else {
        break;
      }
    }

    // Split into key and value, trim trailing spaces from key
    *equal = '\0';
    char *key = part;
    char *value = equal + 1;
    char *key_end = equal - 1;
    while (key_end >= key && isspace((unsigned char)(*key_end))) {
      *key_end = '\0';
      --key_end;
    }

    // Match known keys and assign values
    if (strcmp(key, "total_customers") == 0) {
      config->total_customers = atoi(value);
    } else if (strcmp(key, "queue_capacity") == 0) {
      config->queue_capacity = atoi(value);
    } else if (strcmp(key, "shopping_time_min") == 0) {
      config->shopping_time_min = atoi(value);
    } else if (strcmp(key, "shopping_time_max") == 0) {
      config->shopping_time_max = atoi(value);
    } else if (strcmp(key, "arrival_interval_min") == 0) {
      config->arrival_interval_min = atoi(value);
    } else if (strcmp(key, "arrival_interval_max") == 0) {
      config->arrival_interval_max = atoi(value);
    } else if (strcmp(key, "items_min") == 0) {
      config->items_min = atoi(value);
    } else if (strcmp(key, "items_max") == 0) {
      config->items_max = atoi(value);
    } else if (strcmp(key, "item_service_time") == 0) {
      config->item_service_time = atoi(value);
    } else if (strcmp(key, "queue_strategy") == 0) {
      config->queue_strategy = (QUEUE_STRATEGY)atoi(value);
    } else if (strcmp(key, "work_day_duration") == 0) {
      config->work_day_duration = atoi(value);
    } else if (strcmp(key, "random_seed") == 0) {
      config->random_seed = (unsigned int)atoi(value);
    } else if (strcmp(key, "display_delay_ms") == 0) {
      config->display_delay_ms = atoi(value);
    }

    // Move to next line
    if (newline != NULL) {
      line = newline + 1;
    } else {
      break;
    }
  }
  return STATUS_OK;
}

// Validate configuration parameters
STATUS config_validate(const Config *config) {
  if (config == NULL) {
    return STATUS_NULL_PTR;
  }

  // Check positive values and valid ranges
  if (config->total_customers <= 0) {
    return STATUS_ERROR;
  }

  if (config->queue_capacity <= 0) {
    return STATUS_ERROR;
  }
  if (config->shopping_time_min <= 0 ||
      config->shopping_time_min > config->shopping_time_max) {
    return STATUS_ERROR;
  }
  if (config->arrival_interval_min <= 0 ||
      config->arrival_interval_min > config->arrival_interval_max) {
    return STATUS_ERROR;
  }
  if (config->items_min <= 0 || config->items_min > config->items_max) {
    return STATUS_ERROR;
  }
  if (config->item_service_time <= 0) {
    return STATUS_ERROR;
  }
  if (config->work_day_duration <= 0) {
    return STATUS_ERROR;
  }
  if (config->display_delay_ms < 0) {
    return STATUS_ERROR;
  }

  // Check strategy enum range
  if (config->queue_strategy < STRATEGY_SHORTEST ||
      config->queue_strategy > STRATEGY_RANDOM) {
    return STATUS_ERROR;
  }

  return STATUS_OK;
}

// Print configuration to console and log
STATUS config_print(const Config *config) {
  if (config == NULL) {
    return STATUS_NULL_PTR;
  }

  const char *strategy_names[] = {"SHORTEST", "FIRST", "RANDOM"};
  const char *current_strategy = strategy_names[config->queue_strategy];

  io_printf_both("******************************\n");
  io_printf_both("    SIMULATION CONFIGURATION    \n");
  io_printf_both("Total customers:      %d\n", config->total_customers);
  io_printf_both("Queue capacity:       %d\n", config->queue_capacity);
  io_printf_both("Shopping time:        [%d, %d] sec\n",
                 config->shopping_time_min, config->shopping_time_max);
  io_printf_both("Arrival interval:     [%d, %d] sec\n",
                 config->arrival_interval_min, config->arrival_interval_max);
  io_printf_both("Items per customer:   [%d, %d]\n", config->items_min,
                 config->items_max);
  io_printf_both("Service time/item:    %d sec\n", config->item_service_time);
  io_printf_both("Queue strategy:       %s\n", current_strategy);
  io_printf_both("Work day duration:    %d sec\n", config->work_day_duration);
  io_printf_both("Random seed:          %u\n", config->random_seed);
  io_printf_both("Display delay:        %d ms\n", config->display_delay_ms);
  io_printf_both("******************************\n");

  return STATUS_OK;
}