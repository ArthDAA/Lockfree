#ifndef ACCENTFLOW_CONFIG_H
#define ACCENTFLOW_CONFIG_H

#include <stddef.h>

typedef struct AccentMapping {
    char *base;
    char **variants;
    size_t variant_count;
} AccentMapping;

typedef struct AccentConfig {
    AccentMapping *mappings;
    size_t mapping_count;
    char *input_device;
    char *display_mode;
} AccentConfig;

AccentConfig *config_load(const char *path, char **error_message);
void config_free(AccentConfig *config);
const AccentMapping *config_find_mapping(const AccentConfig *config, const char *base);
const char *config_get_input_device(const AccentConfig *config);
const char *config_get_display_mode(const AccentConfig *config);

#endif /* ACCENTFLOW_CONFIG_H */
