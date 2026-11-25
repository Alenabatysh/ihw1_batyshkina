#ifndef IO_H
#define IO_H

#include "types.h"

// Open and initialize the log file for writing
STATUS io_init_log(const char *filename);

// Close the log file and release the file descriptor
void io_close_log(void);

// Write a string to the console using the write system call
STATUS io_write_console(const char *msg);

// Write a string to the log file using the write system call
STATUS io_write_log(const char *msg);

// Write a string to both console and log file
STATUS io_write_both(const char *msg);

// Formatted output to the console using vsnprintf
STATUS io_printf_console(const char *fmt, ...);

// Formatted output to the log file using vsnprintf
STATUS io_printf_log(const char *fmt, ...);

// Formatted output to both console and log file
STATUS io_printf_both(const char *fmt, ...);

#endif