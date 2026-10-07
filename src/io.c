#include "io.h"
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// File descriptor for the log file (-1 means not opened)
static int log_fd = -1;

// Size of the internal buffer for formatted output
#define IO_BUFFER_SIZE 1024

// Open and initialize the log file for writing
STATUS io_init_log(const char *filename) {
  if (filename == NULL) {
    return STATUS_NULL_PTR;
  }
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0) {
    return STATUS_ERROR;
  }
  // Close previous log file if it was open
  if (log_fd != -1) {
    io_close_log();
  }
  log_fd = fd;
  return STATUS_OK;
}

// Close the log file and reset the file descriptor
void io_close_log(void) {
  if (log_fd != -1) {
    close(log_fd);
    log_fd = -1;
  }
}

// Write a string to the console using the write system call
STATUS io_write_console(const char *msg) {
  if (msg == NULL) {
    return STATUS_NULL_PTR;
  }
  size_t len = strlen(msg);
  ssize_t size = write(STDOUT_FILENO, msg, len);
  if (size < 0 || (size_t)size != len) {
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

// Write a string to the log file using the write system call
STATUS io_write_log(const char *msg) {
  if (msg == NULL) {
    return STATUS_NULL_PTR;
  }
  if (log_fd == -1) {
    return STATUS_ERROR;
  }
  size_t len = strlen(msg);
  ssize_t size = write(log_fd, msg, len);
  if (size < 0 || (size_t)size != len) {
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

// Write a string to both console and log file
STATUS io_write_both(const char *msg) {
  if (msg == NULL) {
    return STATUS_NULL_PTR;
  }

  STATUS res1 = io_write_console(msg);

  // Write to log only if it is open
  if (log_fd != -1) {
    STATUS res2 = io_write_log(msg);
    if (res1 == STATUS_OK && res2 == STATUS_OK) {
      return STATUS_OK;
    }
    return STATUS_ERROR;
  }
  return res1;
}

// Formatted output to the console using vsnprintf
STATUS io_printf_console(const char *fmt, ...) {
  if (fmt == NULL) {
    return STATUS_NULL_PTR;
  }
  va_list args;
  va_start(args, fmt);
  char buffer[IO_BUFFER_SIZE];
  int len = vsnprintf(buffer, IO_BUFFER_SIZE, fmt, args);
  va_end(args);
  if (len < 0) {
    return STATUS_ERROR;
  }
  STATUS res = io_write_console(buffer);
  return res;
}

// Formatted output to the log file using vsnprintf
STATUS io_printf_log(const char *fmt, ...) {
  if (fmt == NULL) {
    return STATUS_NULL_PTR;
  }
  va_list args;
  va_start(args, fmt);
  char buffer[IO_BUFFER_SIZE];
  int len = vsnprintf(buffer, IO_BUFFER_SIZE, fmt, args);
  va_end(args);
  if (len < 0) {
    return STATUS_ERROR;
  }
  STATUS res = io_write_log(buffer);
  return res;
}

// Formatted output to both console and log file
STATUS io_printf_both(const char *fmt, ...) {
  if (fmt == NULL) {
    return STATUS_NULL_PTR;
  }
  va_list args;
  va_start(args, fmt);
  char buffer[IO_BUFFER_SIZE];
  int len = vsnprintf(buffer, IO_BUFFER_SIZE, fmt, args);
  va_end(args);
  if (len < 0) {
    return STATUS_ERROR;
  }
  STATUS res = io_write_both(buffer);
  return res;
}