#include "mapper.h"
#include "config.h"

#include <ctype.h>
#include <linux/input-event-codes.h>
#include <stddef.h>
#include <string.h>

static const char *keycode_to_base(uint16_t keycode)
{
    switch (keycode) {
    case KEY_A: return "a";
    case KEY_B: return "b";
    case KEY_C: return "c";
    case KEY_D: return "d";
    case KEY_E: return "e";
    case KEY_F: return "f";
    case KEY_G: return "g";
    case KEY_H: return "h";
    case KEY_I: return "i";
    case KEY_J: return "j";
    case KEY_K: return "k";
    case KEY_L: return "l";
    case KEY_M: return "m";
    case KEY_N: return "n";
    case KEY_O: return "o";
    case KEY_P: return "p";
    case KEY_Q: return "q";
    case KEY_R: return "r";
    case KEY_S: return "s";
    case KEY_T: return "t";
    case KEY_U: return "u";
    case KEY_V: return "v";
    case KEY_W: return "w";
    case KEY_X: return "x";
    case KEY_Y: return "y";
    case KEY_Z: return "z";
    case KEY_APOSTROPHE: return "'";
    case KEY_SEMICOLON: return ";";
    case KEY_GRAVE: return "`";
    case KEY_LEFTBRACE: return "[";
    case KEY_RIGHTBRACE: return "]";
    case KEY_MINUS: return "-";
    case KEY_EQUAL: return "=";
    default:
        return NULL;
    }
}

const struct AccentMapping *mapper_from_keycode(const struct AccentConfig *config, uint16_t keycode, char *out_base)
{
    const char *base = keycode_to_base(keycode);
    if (!base) {
        return NULL;
    }
    if (out_base) {
        strcpy(out_base, base);
    }
    return config_find_mapping(config, base);
}

const char *mapper_select_variant(const struct AccentMapping *mapping, size_t index)
{
    if (!mapping || mapping->variant_count == 0) {
        return NULL;
    }
    size_t normalized = index % mapping->variant_count;
    return mapping->variants[normalized];
}
