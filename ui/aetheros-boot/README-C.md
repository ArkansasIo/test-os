# AetherOS Nexus — C implementation

This repository's native C implementation renders an 80×24 terminal version of the AetherOS Nexus boot/login concept.

## Build and run

```sh
make clean all
./build/osborne1 --aetheros-screen
```

The C renderer is in `src/boot/aetheros.c`; its public declaration is in `include/aetheros.h`. It uses the project's existing text UI cell buffer and ANSI-capable terminal renderer. No HTML, CSS, JavaScript, external library, or network resource is required for this version.

## Scope

This is a terminal mockup of the visual concept. POST lines are illustrative. The login, shutdown, restart, hardware diagnostics, and kernel handoff are not connected to real host services.
