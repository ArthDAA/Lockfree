PREFIX ?= /opt/accentflow
BINDIR := $(PREFIX)
CONFIGDIR := /etc/accentflow

CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -D_GNU_SOURCE
LDFLAGS :=

SRC_DIR := src
INC_DIR := include

SOURCES := \
    $(SRC_DIR)/accentflow.c \
    $(SRC_DIR)/config_loader.c \
    $(SRC_DIR)/display_tui.c \
    $(SRC_DIR)/input_engine.c \
    $(SRC_DIR)/mapper.c \
    $(SRC_DIR)/utils.c

OBJECTS := $(SOURCES:.c=.o)
TARGET := accentflowd

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -d $(DESTDIR)$(CONFIGDIR)
	install -m 0644 config/config.json $(DESTDIR)$(CONFIGDIR)/config.json

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all install clean
