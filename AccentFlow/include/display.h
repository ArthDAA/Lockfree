#ifndef ACCENTFLOW_DISPLAY_H
#define ACCENTFLOW_DISPLAY_H

#include <stddef.h>

struct AccentMapping;

typedef struct Display Display;

Display *display_create_tui(void);
void display_destroy(Display *display);
void display_show_variants(Display *display, const char *base, const struct AccentMapping *mapping, size_t active_index);
void display_show_committed(Display *display, const char *text);
void display_clear(Display *display);

#endif /* ACCENTFLOW_DISPLAY_H */
