#ifndef IO_H
#define IO_H

#include "types.h"

STATUS io_init_log(const char *filename);

void io_close_log(void);

STATUS io_write_console(const char *msg);

STATUS io_write_log(const char *msg);

STATUS io_write_both(const char *msg);

STATUS io_printf_console(const char *fmt, ...);

STATUS io_printf_log(const char *fmt, ...);

STATUS io_printf_both(const char *fmt, ...);

#endif