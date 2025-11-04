#include "input_engine.h"
#include "config.h"
#include "display.h"
#include "mapper.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#define ACCENTFLOW_MAX_BASE 8

struct InputEngine {
    int input_fd;
    int uinput_fd;
    bool grab;
    const struct AccentConfig *config;
    struct Display *display;

    bool accent_mode;
    bool has_active_key;
    uint16_t active_keycode;
    size_t variant_index;
    const struct AccentMapping *active_mapping;
    char base[ACCENTFLOW_MAX_BASE];
};

static int emit_event(int fd, uint16_t type, uint16_t code, int32_t value)
{
    struct input_event event;
    memset(&event, 0, sizeof(event));
    gettimeofday(&event.time, NULL);
    event.type = type;
    event.code = code;
    event.value = value;
    if (write(fd, &event, sizeof(event)) < 0) {
        return -1;
    }
    return 0;
}

static int emit_syn(int fd)
{
    return emit_event(fd, EV_SYN, SYN_REPORT, 0);
}

static int send_key(int fd, uint16_t code, int value)
{
    if (emit_event(fd, EV_KEY, code, value) < 0) {
        return -1;
    }
    if (emit_syn(fd) < 0) {
        return -1;
    }
    return 0;
}

static uint16_t hex_char_to_keycode(char c)
{
    if (c >= '0' && c <= '9') {
        return KEY_0 + (c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return KEY_A + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return KEY_A + (c - 'A');
    }
    return 0;
}

static bool utf8_to_codepoint(const char *utf8, uint32_t *codepoint)
{
    if (!utf8 || !codepoint) {
        return false;
    }
    const unsigned char *s = (const unsigned char *)utf8;
    if (s[0] < 0x80) {
        *codepoint = s[0];
        return true;
    }
    if ((s[0] & 0xE0) == 0xC0) {
        if ((s[1] & 0xC0) != 0x80) {
            return false;
        }
        *codepoint = ((uint32_t)(s[0] & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
        return true;
    }
    if ((s[0] & 0xF0) == 0xE0) {
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) {
            return false;
        }
        *codepoint = ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (uint32_t)(s[2] & 0x3F);
        return true;
    }
    if ((s[0] & 0xF8) == 0xF0) {
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) {
            return false;
        }
        *codepoint = ((uint32_t)(s[0] & 0x07) << 18) |
                     ((uint32_t)(s[1] & 0x3F) << 12) |
                     ((uint32_t)(s[2] & 0x3F) << 6) |
                     (uint32_t)(s[3] & 0x3F);
        return true;
    }
    return false;
}

static int send_unicode(InputEngine *engine, const char *utf8)
{
    uint32_t codepoint = 0;
    if (!utf8_to_codepoint(utf8, &codepoint)) {
        log_error("Unable to convert '%s' to a Unicode codepoint", utf8);
        return -1;
    }

    char hex[9];
    snprintf(hex, sizeof(hex), "%x", codepoint);

    if (send_key(engine->uinput_fd, KEY_LEFTCTRL, 1) < 0 ||
        send_key(engine->uinput_fd, KEY_LEFTSHIFT, 1) < 0 ||
        send_key(engine->uinput_fd, KEY_U, 1) < 0 ||
        send_key(engine->uinput_fd, KEY_U, 0) < 0) {
        return -1;
    }
    if (send_key(engine->uinput_fd, KEY_LEFTCTRL, 0) < 0 ||
        send_key(engine->uinput_fd, KEY_LEFTSHIFT, 0) < 0) {
        return -1;
    }

    for (size_t i = 0; hex[i] != '\0'; ++i) {
        uint16_t keycode = hex_char_to_keycode(hex[i]);
        if (!keycode) {
            log_error("Unsupported hex digit '%c' in Unicode sequence", hex[i]);
            return -1;
        }
        if (send_key(engine->uinput_fd, keycode, 1) < 0 ||
            send_key(engine->uinput_fd, keycode, 0) < 0) {
            return -1;
        }
    }

    if (send_key(engine->uinput_fd, KEY_ENTER, 1) < 0 ||
        send_key(engine->uinput_fd, KEY_ENTER, 0) < 0) {
        return -1;
    }

    return 0;
}

static int forward_event(InputEngine *engine, const struct input_event *event)
{
    if (write(engine->uinput_fd, event, sizeof(*event)) < 0) {
        return -1;
    }
    return 0;
}

static bool setup_uinput_device(InputEngine *engine)
{
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        log_error("Unable to open /dev/uinput: %s", strerror(errno));
        return false;
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 ||
        ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0) {
        log_error("Failed to configure uinput events: %s", strerror(errno));
        close(fd);
        return false;
    }

    for (int key = KEY_ESC; key <= KEY_MICMUTE; ++key) {
        ioctl(fd, UI_SET_KEYBIT, key);
    }

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    snprintf(setup.name, sizeof(setup.name), "AccentFlow Virtual Keyboard");
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x1fed;
    setup.id.product = 0x0001;
    setup.id.version = 1;

    if (ioctl(fd, UI_DEV_SETUP, &setup) < 0) {
        log_error("Failed to setup uinput device: %s", strerror(errno));
        close(fd);
        return false;
    }
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        log_error("Failed to create uinput device: %s", strerror(errno));
        close(fd);
        return false;
    }

    engine->uinput_fd = fd;
    return true;
}

static void reset_state(InputEngine *engine)
{
    engine->has_active_key = false;
    engine->active_keycode = 0;
    engine->variant_index = 0;
    engine->active_mapping = NULL;
    engine->base[0] = '\0';
    if (engine->display) {
        display_clear(engine->display);
    }
}

static void handle_accent_key(InputEngine *engine, const struct input_event *event)
{
    if (event->value == 1) {
        engine->accent_mode = true;
        reset_state(engine);
        log_info("Accent mode engaged");
    } else if (event->value == 0 && engine->accent_mode) {
        engine->accent_mode = false;
        if (engine->active_mapping) {
            const char *variant = mapper_select_variant(engine->active_mapping, engine->variant_index);
            if (variant) {
                if (send_unicode(engine, variant) == 0) {
                    if (engine->display) {
                        display_show_committed(engine->display, variant);
                    }
                    log_info("Committed variant '%s'", variant);
                }
            }
        }
        reset_state(engine);
        log_info("Accent mode released");
    }
}

static void update_preview(InputEngine *engine)
{
    if (!engine->display || !engine->active_mapping) {
        return;
    }
    size_t index = engine->active_mapping->variant_count > 0 ? engine->variant_index % engine->active_mapping->variant_count : 0;
    display_show_variants(engine->display, engine->base, engine->active_mapping, index);
}

static bool handle_accentable_key(InputEngine *engine, const struct input_event *event)
{
    if (!engine->accent_mode) {
        return false;
    }
    if (event->value != 1 && event->value != 2) {
        if (engine->has_active_key && event->code == engine->active_keycode) {
            return true;
        }
        return false;
    }

    char base[ACCENTFLOW_MAX_BASE] = {0};
    const struct AccentMapping *mapping = mapper_from_keycode(engine->config, event->code, base);
    if (!mapping) {
        return false;
    }

    if (!engine->has_active_key || engine->active_keycode != event->code) {
        engine->has_active_key = true;
        engine->active_keycode = event->code;
        engine->active_mapping = mapping;
        snprintf(engine->base, sizeof(engine->base), "%s", base);
        engine->variant_index = 0;
    } else {
        engine->variant_index++;
    }

    update_preview(engine);
    return true;
}

InputEngine *input_engine_create(const char *device_path, const struct AccentConfig *config, struct Display *display, bool grab_device)
{
    if (!device_path) {
        log_error("No input device specified");
        return NULL;
    }

    InputEngine *engine = calloc(1, sizeof(InputEngine));
    if (!engine) {
        return NULL;
    }

    engine->input_fd = -1;
    engine->uinput_fd = -1;
    engine->config = config;
    engine->display = display;
    engine->grab = grab_device;

    engine->input_fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (engine->input_fd < 0) {
        log_error("Unable to open %s: %s", device_path, strerror(errno));
        free(engine);
        return NULL;
    }

    if (grab_device) {
        if (ioctl(engine->input_fd, EVIOCGRAB, 1) < 0) {
            log_error("Failed to grab input device %s: %s", device_path, strerror(errno));
            close(engine->input_fd);
            free(engine);
            return NULL;
        }
    }

    if (!setup_uinput_device(engine)) {
        if (grab_device) {
            ioctl(engine->input_fd, EVIOCGRAB, 0);
        }
        close(engine->input_fd);
        free(engine);
        return NULL;
    }

    log_info("AccentFlow listening on %s", device_path);
    return engine;
}

void input_engine_destroy(InputEngine *engine)
{
    if (!engine) {
        return;
    }
    if (engine->grab && engine->input_fd >= 0) {
        ioctl(engine->input_fd, EVIOCGRAB, 0);
    }
    if (engine->input_fd >= 0) {
        close(engine->input_fd);
    }
    if (engine->uinput_fd >= 0) {
        ioctl(engine->uinput_fd, UI_DEV_DESTROY);
        close(engine->uinput_fd);
    }
    free(engine);
}

static int process_event(InputEngine *engine, const struct input_event *event)
{
    if (event->type == EV_KEY && event->code == KEY_RIGHTALT) {
        handle_accent_key(engine, event);
        return 0;
    }

    if (event->type == EV_KEY) {
        if (handle_accentable_key(engine, event)) {
            return 0;
        }
    }

    if (!engine->accent_mode || !engine->has_active_key || event->code != engine->active_keycode) {
        if (forward_event(engine, event) < 0) {
            return -1;
        }
    }

    return 0;
}

int input_engine_run(InputEngine *engine)
{
    if (!engine) {
        return -1;
    }

    while (1) {
        struct input_event event;
        ssize_t bytes = read(engine->input_fd, &event, sizeof(event));
        if (bytes < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                usleep(1000);
                continue;
            }
            log_error("Read error: %s", strerror(errno));
            return -1;
        }
        if (bytes != sizeof(event)) {
            continue;
        }

        if (process_event(engine, &event) < 0) {
            return -1;
        }
    }

    return 0;
}
