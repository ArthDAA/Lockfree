#include "display.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Display {
    size_t last_columns;
};

static void clear_previous(Display *display)
{
    if (!display || display->last_columns == 0) {
        return;
    }
    fprintf(stderr, "\r");
    for (size_t i = 0; i < display->last_columns; ++i) {
        fputc(' ', stderr);
    }
    fprintf(stderr, "\r");
    display->last_columns = 0;
}

Display *display_create_tui(void)
{
    Display *display = calloc(1, sizeof(Display));
    return display;
}

void display_destroy(Display *display)
{
    if (!display) {
        return;
    }
    clear_previous(display);
    free(display);
}

void display_show_variants(Display *display, const char *base, const struct AccentMapping *mapping, size_t active_index)
{
    if (!display) {
        return;
    }
    clear_previous(display);
    if (!mapping || mapping->variant_count == 0) {
        return;
    }

    char buffer[512];
    size_t offset = 0;
    offset += (size_t)snprintf(buffer + offset, sizeof(buffer) - offset, "AccentFlow %s: ", base ? base : "");
    for (size_t i = 0; i < mapping->variant_count && offset < sizeof(buffer); ++i) {
        const char *variant = mapping->variants[i];
        if (!variant) {
            continue;
        }
        if (i == active_index) {
            offset += (size_t)snprintf(buffer + offset, sizeof(buffer) - offset, "[%s]", variant);
        } else {
            offset += (size_t)snprintf(buffer + offset, sizeof(buffer) - offset, " %s ", variant);
        }
    }

    fprintf(stderr, "%s", buffer);
    display->last_columns = strlen(buffer);
}

void display_show_committed(Display *display, const char *text)
{
    if (!display) {
        return;
    }
    clear_previous(display);
    if (!text) {
        return;
    }
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "AccentFlow committed: %s", text);
    fprintf(stderr, "%s\n", buffer);
}

void display_clear(Display *display)
{
    if (!display) {
        return;
    }
    clear_previous(display);
}
