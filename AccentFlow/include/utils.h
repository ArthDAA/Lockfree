#ifndef ACCENTFLOW_UTILS_H
#define ACCENTFLOW_UTILS_H

#include <stdarg.h>
#include <stddef.h>

void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);
char *read_file_to_buffer(const char *path, size_t *length);
char *duplicate_string(const char *src);

#endif /* ACCENTFLOW_UTILS_H */
