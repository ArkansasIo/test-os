CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?=

BUILD   := build

# A literal newline, used to join `foreach` output into separate recipe lines.
# `define` is the one portable way to get one into a make variable.
define NEWLINE


endef

# mingw32-make on Windows runs recipes through cmd.exe, so the commands below
# have to be ones cmd understands. cmd's mkdir and rmdir accept forward slashes
# (mkdir build/a/b/c creates every level), so the recipes stay host-portable
# without depending on a POSIX shell being installed.
ifeq ($(OS),Windows_NT)
  # Force cmd.exe as the recipe shell. If MSYS/Git's sh.exe is on PATH, make
  # picks it automatically, and the cmd built-ins below (mkdir, rmdir,
  # `if not exist`) then fail with "syntax of the command is incorrect".
  SHELL  := cmd.exe
  .SHELLFLAGS := /c
  EXE := .exe
  # cmd's mkdir fails if the directory already exists, which aborts the whole
  # recipe. The `dirs` target guards each call so it is idempotent on Windows.
  MKDIRS := mkdir
  RMDIR  := rmdir /s /q
  RM     := rmdir /s /q
else
  EXE :=
  MKDIRS := mkdir -p
  RMDIR  := rm -rf
  RM     := rm -f
endif

BIN     := $(BUILD)/osborne1$(EXE)

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

DIRS := muthur login boot screen menus modules sound frontend

ifeq ($(OS),Windows_NT)
# cmd's mkdir takes one path and errors if it already exists, so every call is
# guarded. Each directory gets its own recipe line: a single line containing
# several commands confuses cmd.exe when make hands it the whole recipe.
dirs:
	@if not exist $(BUILD) mkdir $(BUILD)
	@if not exist $(BUILD)/muthur mkdir $(BUILD)/muthur
	@if not exist $(BUILD)/login mkdir $(BUILD)/login
	@if not exist $(BUILD)/boot mkdir $(BUILD)/boot
	@if not exist $(BUILD)/screen mkdir $(BUILD)/screen
	@if not exist $(BUILD)/menus mkdir $(BUILD)/menus
	@if not exist $(BUILD)/modules mkdir $(BUILD)/modules
	@if not exist $(BUILD)/sound mkdir $(BUILD)/sound
	@if not exist $(BUILD)/frontend mkdir $(BUILD)/frontend
else
dirs:
	@$(MKDIRS) $(BUILD)
	@$(foreach d,$(DIRS),$(MKDIRS) $(BUILD)/$(d)$(NEWLINE))
endif
# The output directories must exist before any .o is written, so `dirs` is an
# order-only prerequisite: it is built first but never forces a relink.
$(BIN): $(OBJ) | dirs
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

$(BUILD)/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	$(BIN)

# Render the 2600 Hz tone, the Pong effects, and the printer to build/*.wav
demo: all
	$(BIN) --sound-demo
	$(BIN) --printer-demo

test: all
	$(BIN) --list
	$(BIN) --sound-test

printer: all
	$(BIN) --printer-demo

clean:
	-$(RMDIR) $(BUILD)
