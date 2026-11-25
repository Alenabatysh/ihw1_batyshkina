#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

// Set default configuration values
void config_set_defaults(Config *config);

// Load configuration from file, parse key=value pairs
STATUS config_load(const char *filename, Config *config);

// Validate configuration parameters (check ranges and constraints)
STATUS config_validate(const Config *config);

// Print configuration to console and log
STATUS config_print(const Config *config);

#endif