CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?=

BUILD   := build
BIN     := $(BUILD)/osborne1

SRC := \
	src/muthur/muthur.c \
	src/login/users.c \
	src/login/session.c \
	src/login/logout.c \
	src/boot/loader.c \
	src/boot/init.c \
	src/screen/screen.c \
	src/menus/menu.c \
	src/modules/module.c \
	src/sound/sound.c \
	src/frontend/main.c

OBJ := $(SRC:src/%.c=$(BUILD)/%.o)

.PHONY: all run test demo printer clean dirs

all: dirs $(BIN)

dirs:
	@mkdir -p $(BUILD)/muthur $(BUILD)/login $(BUILD)/boot $(BUILD)/screen $(BUILD)/menus $(BUILD)/modules $(BUILD)/sound $(BUILD)/frontend

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

$(BUILD)/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	./$(BIN)

# Render the 2600 Hz tone, the Pong effects, and the printer to build/*.wav
demo: all
	./$(BIN) --sound-demo
	./$(BIN) --printer-demo

test: all
	./$(BIN) --list
	./$(BIN) --sound-test

printer: all
	./$(BIN) --printer-demo

clean:
	rm -rf $(BUILD)
