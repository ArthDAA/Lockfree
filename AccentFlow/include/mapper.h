#ifndef ACCENTFLOW_MAPPER_H
#define ACCENTFLOW_MAPPER_H

#include <stddef.h>
#include <stdint.h>

struct AccentConfig;
struct AccentMapping;

const struct AccentMapping *mapper_from_keycode(const struct AccentConfig *config, uint16_t keycode, char *out_base);
const char *mapper_select_variant(const struct AccentMapping *mapping, size_t index);

#endif /* ACCENTFLOW_MAPPER_H */
