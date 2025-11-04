#include "utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void vprint_log(const char *prefix, const char *fmt, va_list args)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);

    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    fprintf(stderr, "%s.%03ld [%s] ", buffer, ts.tv_nsec / 1000000L, prefix);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
}

void log_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprint_log("INFO", fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprint_log("ERROR", fmt, args);
    va_end(args);
}

char *read_file_to_buffer(const char *path, size_t *length)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        log_error("Failed to open %s: %s", path, strerror(errno));
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        log_error("Failed to seek %s", path);
        fclose(fp);
        return NULL;
    }
    long size = ftell(fp);
    if (size < 0) {
        log_error("Failed to determine size of %s", path);
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    char *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        log_error("Out of memory while reading %s", path);
        fclose(fp);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, fp);
    if (read != (size_t)size) {
        log_error("Failed to read %s", path);
        free(buffer);
        fclose(fp);
        return NULL;
    }
    buffer[size] = '\0';

    fclose(fp);

    if (length) {
        *length = (size_t)size;
    }
    return buffer;
}

char *duplicate_string(const char *src)
{
    if (!src) {
        return NULL;
    }
    size_t len = strlen(src);
    char *copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, src, len + 1);
    return copy;
}
