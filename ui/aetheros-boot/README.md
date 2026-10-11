# AetherOS Nexus — native C terminal screen

The AetherOS Nexus boot/login concept is implemented in C using the repository's existing 80×24 terminal cell buffer and ANSI-capable output.

## Source files

- `include/aetheros.h` — public renderer declaration.
- `src/boot/aetheros.c` — logo, branding, system POST panel, diagnostic rows, and login mockup.
- `src/frontend/main.c` — command-line option and dispatcher.
- `Makefile` — compiles the renderer into the existing executable.
- `tests/check_repository.py` — validates that the C renderer, public header, build integration, and CLI option exist.

## Build and run

```sh
make clean all
./build/osborne1 --aetheros-screen
```

Headless smoke run:

```sh
./build/osborne1 --aetheros-screen </dev/null
python3 tests/check_repository.py
```

## Scope and safety

This is a terminal mockup, not a graphical display manager. Hardware POST results are illustrative. It does not collect passwords, authenticate accounts, power off the host, or load a real Linux kernel.
