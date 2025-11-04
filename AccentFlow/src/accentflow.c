#include "accentflow.h"
#include "config.h"
#include "display.h"
#include "input_engine.h"
#include "utils.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s [-c config] [-d device] [--no-grab]\n", program);
}

int main(int argc, char **argv)
{
    const char *config_path = "/etc/accentflow/config.json";
    const char *device_path = NULL;
    bool grab_device = true;

    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"device", required_argument, 0, 'd'},
        {"no-grab", no_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:d:nh", long_options, NULL)) != -1) {
        switch (opt) {
        case 'c':
            config_path = optarg;
            break;
        case 'd':
            device_path = optarg;
            break;
        case 'n':
            grab_device = false;
            break;
        case 'h':
        default:
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    char *error_message = NULL;
    AccentConfig *config = config_load(config_path, &error_message);
    if (!config) {
        log_error("Failed to load configuration: %s", error_message ? error_message : "unknown error");
        free(error_message);
        return EXIT_FAILURE;
    }
    free(error_message);

    if (!device_path) {
        device_path = config_get_input_device(config);
    }
    if (!device_path) {
        log_error("No input device specified. Use --device or set input_device in the configuration file.");
        config_free(config);
        return EXIT_FAILURE;
    }

    Display *display = display_create_tui();
    if (!display) {
        log_error("Unable to initialize display module");
        config_free(config);
        return EXIT_FAILURE;
    }

    InputEngine *engine = input_engine_create(device_path, config, display, grab_device);
    if (!engine) {
        display_destroy(display);
        config_free(config);
        return EXIT_FAILURE;
    }

    log_info("AccentFlow daemon started");

    int rc = input_engine_run(engine);
    if (rc != 0) {
        log_error("Input engine terminated with error");
    }

    input_engine_destroy(engine);
    display_destroy(display);
    config_free(config);

    log_info("AccentFlow daemon stopped");
    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
