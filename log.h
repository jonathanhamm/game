#ifndef __sa_log_h__
#define __sa_log_h__

#include <stdio.h>

#define LOG_PATH "./logs"
#define LOG_ERR_BUF_SIZE 256

extern int log_init_name(const char *name);
extern int log_init(FILE *file);
extern void log_puts(const char *format, ...);
extern void log_debug(const char *format, ...);
extern void log_info(const char *format, ...);
extern void log_warn(const char *format, ...);
extern void log_error(const char *format, ...);
extern void log_error_errno(const char *format, ...);
extern void log_error_explicit(int err, const char *format, ...);
extern void log_end(void);

#endif

