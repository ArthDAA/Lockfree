#ifndef ACCENTFLOW_INPUT_ENGINE_H
#define ACCENTFLOW_INPUT_ENGINE_H

#include <stdbool.h>

struct AccentConfig;
struct Display;

typedef struct InputEngine InputEngine;

InputEngine *input_engine_create(const char *device_path, const struct AccentConfig *config, struct Display *display, bool grab_device);
void input_engine_destroy(InputEngine *engine);
int input_engine_run(InputEngine *engine);

#endif /* ACCENTFLOW_INPUT_ENGINE_H */
