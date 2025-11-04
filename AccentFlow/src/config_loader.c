#include "config.h"
#include "utils.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *data;
    size_t length;
    size_t pos;
    char error[256];
} JsonParser;

static void skip_whitespace(JsonParser *parser)
{
    while (parser->pos < parser->length) {
        char c = parser->data[parser->pos];
        if (!isspace((unsigned char)c)) {
            break;
        }
        parser->pos++;
    }
}

static bool match_char(JsonParser *parser, char expected)
{
    skip_whitespace(parser);
    if (parser->pos >= parser->length || parser->data[parser->pos] != expected) {
        snprintf(parser->error, sizeof(parser->error), "Expected '%c'", expected);
        return false;
    }
    parser->pos++;
    return true;
}

static int peek_char(JsonParser *parser)
{
    skip_whitespace(parser);
    if (parser->pos >= parser->length) {
        return EOF;
    }
    return parser->data[parser->pos];
}

static int advance_char(JsonParser *parser)
{
    if (parser->pos >= parser->length) {
        return EOF;
    }
    return parser->data[parser->pos++];
}

static char *parse_string(JsonParser *parser)
{
    if (!match_char(parser, '"')) {
        return NULL;
    }

    size_t capacity = 64;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (!buffer) {
        snprintf(parser->error, sizeof(parser->error), "Out of memory");
        return NULL;
    }

    while (parser->pos < parser->length) {
        char c = advance_char(parser);
        if (c == '"') {
            break;
        }
        if (c == '\\') {
            if (parser->pos >= parser->length) {
                snprintf(parser->error, sizeof(parser->error), "Unexpected end of escape sequence");
                free(buffer);
                return NULL;
            }
            char esc = advance_char(parser);
            switch (esc) {
            case '"': c = '"'; break;
            case '\\': c = '\\'; break;
            case '/': c = '/'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'u':
                snprintf(parser->error, sizeof(parser->error), "\\u escapes are not supported in this configuration parser");
                free(buffer);
                return NULL;
            default:
                snprintf(parser->error, sizeof(parser->error), "Unknown escape character '%c'", esc);
                free(buffer);
                return NULL;
            }
        }

        if (length + 1 >= capacity) {
            capacity *= 2;
            char *tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                snprintf(parser->error, sizeof(parser->error), "Out of memory");
                return NULL;
            }
            buffer = tmp;
        }
        buffer[length++] = c;
    }

    if (length + 1 >= capacity) {
        char *tmp = realloc(buffer, length + 1);
        if (!tmp) {
            free(buffer);
            snprintf(parser->error, sizeof(parser->error), "Out of memory");
            return NULL;
        }
        buffer = tmp;
    }

    buffer[length] = '\0';
    return buffer;
}

static bool expect_token(JsonParser *parser, const char *token)
{
    skip_whitespace(parser);
    size_t len = strlen(token);
    if (parser->pos + len > parser->length) {
        snprintf(parser->error, sizeof(parser->error), "Unexpected end of input");
        return false;
    }
    if (strncmp(parser->data + parser->pos, token, len) != 0) {
        snprintf(parser->error, sizeof(parser->error), "Expected token %s", token);
        return false;
    }
    parser->pos += len;
    return true;
}

static bool parse_value_is_null(JsonParser *parser)
{
    return expect_token(parser, "null");
}

static char **parse_string_array(JsonParser *parser, size_t *count)
{
    if (!match_char(parser, '[')) {
        return NULL;
    }

    size_t capacity = 4;
    size_t length = 0;
    char **items = malloc(capacity * sizeof(char *));
    if (!items) {
        snprintf(parser->error, sizeof(parser->error), "Out of memory");
        return NULL;
    }

    if (peek_char(parser) == ']') {
        parser->pos++;
        *count = 0;
        return items;
    }

    while (parser->pos < parser->length) {
        char *value = parse_string(parser);
        if (!value) {
            for (size_t i = 0; i < length; ++i) {
                free(items[i]);
            }
            free(items);
            return NULL;
        }

        if (length >= capacity) {
            capacity *= 2;
            char **tmp = realloc(items, capacity * sizeof(char *));
            if (!tmp) {
                snprintf(parser->error, sizeof(parser->error), "Out of memory");
                free(value);
                for (size_t i = 0; i < length; ++i) {
                    free(items[i]);
                }
                free(items);
                return NULL;
            }
            items = tmp;
        }
        items[length++] = value;

        int next = peek_char(parser);
        if (next == ',') {
            parser->pos++;
            continue;
        }
        if (next == ']') {
            parser->pos++;
            break;
        }
        snprintf(parser->error, sizeof(parser->error), "Expected ',' or ']' in array");
        for (size_t i = 0; i < length; ++i) {
            free(items[i]);
        }
        free(items);
        return NULL;
    }

    *count = length;
    return items;
}

static bool append_mapping(AccentConfig *config, AccentMapping mapping)
{
    AccentMapping *tmp = realloc(config->mappings, (config->mapping_count + 1) * sizeof(AccentMapping));
    if (!tmp) {
        return false;
    }
    config->mappings = tmp;
    config->mappings[config->mapping_count++] = mapping;
    return true;
}

static bool parse_object(JsonParser *parser, AccentConfig *config)
{
    if (!match_char(parser, '{')) {
        return false;
    }

    if (peek_char(parser) == '}') {
        parser->pos++;
        return true;
    }

    while (parser->pos < parser->length) {
        char *key = parse_string(parser);
        if (!key) {
            return false;
        }
        if (!match_char(parser, ':')) {
            free(key);
            return false;
        }

        int next = peek_char(parser);
        if (next == '[') {
            size_t count = 0;
            char **variants = parse_string_array(parser, &count);
            if (!variants) {
                free(key);
                return false;
            }

            AccentMapping mapping = {0};
            mapping.base = key;
            mapping.variants = variants;
            mapping.variant_count = count;

            if (!append_mapping(config, mapping)) {
                snprintf(parser->error, sizeof(parser->error), "Out of memory");
                free(key);
                for (size_t i = 0; i < count; ++i) {
                    free(variants[i]);
                }
                free(variants);
                return false;
            }
        } else if (next == '"') {
            char *value = parse_string(parser);
            if (!value) {
                free(key);
                return false;
            }
            if (strcmp(key, "input_device") == 0) {
                free(config->input_device);
                config->input_device = value;
                free(key);
            } else if (strcmp(key, "display_mode") == 0) {
                free(config->display_mode);
                config->display_mode = value;
                free(key);
            } else {
                log_error("Ignoring unexpected string property '%s' in configuration", key);
                free(key);
                free(value);
            }
        } else if (next == 'n') {
            if (!parse_value_is_null(parser)) {
                free(key);
                return false;
            }
            if (strcmp(key, "input_device") == 0) {
                free(config->input_device);
                config->input_device = NULL;
            } else if (strcmp(key, "display_mode") == 0) {
                free(config->display_mode);
                config->display_mode = NULL;
            }
            free(key);
        } else {
            snprintf(parser->error, sizeof(parser->error), "Unsupported value type for key '%s'", key);
            free(key);
            return false;
        }

        int delimiter = peek_char(parser);
        if (delimiter == ',') {
            parser->pos++;
            continue;
        }
        if (delimiter == '}') {
            parser->pos++;
            break;
        }
        snprintf(parser->error, sizeof(parser->error), "Expected ',' or '}' in object");
        return false;
    }

    return true;
}

AccentConfig *config_load(const char *path, char **error_message)
{
    size_t length = 0;
    char *buffer = read_file_to_buffer(path, &length);
    if (!buffer) {
        if (error_message) {
            *error_message = duplicate_string("Unable to read configuration file");
        }
        return NULL;
    }

    AccentConfig *config = calloc(1, sizeof(AccentConfig));
    if (!config) {
        free(buffer);
        if (error_message) {
            *error_message = duplicate_string("Out of memory");
        }
        return NULL;
    }

    JsonParser parser = {0};
    parser.data = buffer;
    parser.length = length;

    if (!parse_object(&parser, config)) {
        if (error_message) {
            *error_message = duplicate_string(parser.error[0] ? parser.error : "Failed to parse configuration");
        }
        config_free(config);
        free(buffer);
        return NULL;
    }

    free(buffer);
    return config;
}

void config_free(AccentConfig *config)
{
    if (!config) {
        return;
    }
    for (size_t i = 0; i < config->mapping_count; ++i) {
        AccentMapping *mapping = &config->mappings[i];
        free(mapping->base);
        for (size_t j = 0; j < mapping->variant_count; ++j) {
            free(mapping->variants[j]);
        }
        free(mapping->variants);
    }
    free(config->mappings);
    free(config->input_device);
    free(config->display_mode);
    free(config);
}

const AccentMapping *config_find_mapping(const AccentConfig *config, const char *base)
{
    if (!config || !base) {
        return NULL;
    }
    for (size_t i = 0; i < config->mapping_count; ++i) {
        if (strcmp(config->mappings[i].base, base) == 0) {
            return &config->mappings[i];
        }
    }
    return NULL;
}

const char *config_get_input_device(const AccentConfig *config)
{
    return config ? config->input_device : NULL;
}

const char *config_get_display_mode(const AccentConfig *config)
{
    return config ? config->display_mode : NULL;
}
