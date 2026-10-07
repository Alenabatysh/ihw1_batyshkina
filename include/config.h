#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

void config_set_defaults(Config *config);

STATUS config_load(const char *filename, Config *config);

STATUS config_validate(const Config *config);

STATUS config_print(const Config *config);

#endif