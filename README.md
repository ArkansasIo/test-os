# Test OS — Osborne 1 / CP/M Emulator and AetherOS Nexus Prototype

This repository contains two related but distinct experimental projects:

1. **Osborne 1 / CP/M emulator scaffold** — a portable C command-line program and supporting modules inspired by the Osborne 1.
2. **AetherOS Nexus prototype** — a terminal-rendered OS-style interface and an optional out-of-tree Linux kernel module that exposes build-host identity information.

> **Project status:** experimental prototype. Do not treat this repository as a production operating system, a full Osborne hardware emulator, or a secure login platform. Validate each component and its limitations before relying on it.

## Project components

### Osborne 1 / CP/M workbench

The C command-line application currently contains boot-flow demos, menu/session scaffolding, MU/TH/UR-themed console code, terminal screens, and sound-effect generation. These interfaces model a retro-computing experience; the presence of a boot screen does not mean that a real Linux kernel or CP/M disk image is being booted.

### AetherOS Nexus

- Native C terminal-rendered screen: `src/boot/aetheros.c`
- Public interface: `include/aetheros.h`
- Optional Linux kernel module: `kernel/aetheros/`
- Read-only identity endpoint when the module is loaded: `/proc/aetheros_identity`

The kernel module reports the host kernel release and architecture and writes branding to the kernel log. It **does not** perform hardware POST, replace Linux, boot a kernel, provide a graphical display server, or authenticate users. Kernel modules execute with high privilege: only load a module you have reviewed and built for the exact target kernel.

## Repository map

| Path | Purpose |
| --- | --- |
| `src/` | Native C prototype modules: boot, frontend, login/session, menus, GUI API, sound, and screens |
| `include/` | Public C interfaces and shared type definitions |
| `os/desktop/menus/` | JSON menu configuration |
| `sql/` | Experimental SQL schemas and sample queries; not automatically applied by the C application |
| `kernel/aetheros/` | Optional Linux kernel identity/branding module |
| `tests/` | Static repository checks and command-line smoke tests |
| `.github/workflows/ci.yml` | Linux build and CI checks |
| `Makefile` | Build, test, demo, and cleanup targets |

Some historical documentation describes emulator directories such as `src/cpu/`, `src/machine/`, `src/cpm/`, `disks/`, and `roms/` that are not present in the current source tree. The currently tracked C target should therefore be understood as an OS-style prototype/workbench rather than a completed Z80/CP/M emulator.

## Requirements

- A C compiler supporting C11 (GCC or Clang)
- GNU Make
- Python 3 for repository validation
- Linux kernel headers and a matching kernel build toolchain only if building the optional kernel module

On Debian/Ubuntu, the user-space tools can be installed with:

```sh
sudo apt-get update
sudo apt-get install -y build-essential python3
```

## Build and test the user-space program

```sh
make clean all
make test
make demo
```

Useful CLI checks:

```sh
./build/osborne1 --help
./build/osborne1 --list
./build/osborne1 --menu --script
./build/osborne1 --textui
./build/osborne1 --boot-sequence </dev/null
./build/osborne1 --aetheros-screen </dev/null
```

On Windows, use a MinGW-w64 toolchain and `mingw32-make`. Commands that contain shell redirection should be run in a compatible shell; the Makefile has a dedicated `cmd.exe` path for its standard build recipes.

## Build the optional Linux kernel module

Run on a Linux machine or disposable test VM with development headers that match the running kernel:

```sh
sh kernel/aetheros/build-module.sh
```

The equivalent out-of-tree build is:

```sh
make -C "/lib/modules/$(uname -r)/build" M="$PWD/kernel/aetheros" modules
```

Inspect and load only after reviewing the source. Loading and unloading require privileges:

```sh
sudo insmod kernel/aetheros/aetheros.ko
cat /proc/aetheros_identity
dmesg | tail -n 20
sudo rmmod aetheros
```

If Secure Boot enforces module signing, follow the distribution's trusted signing workflow. Do not disable platform security simply to test the module. Module compatibility has to be verified against the exact target kernel; this repository does not claim a successful build for every kernel version.

## CI

GitHub Actions runs repository structure checks, compiles the C target with warnings enabled, and executes available CLI tests on pushes and pull requests to `main`. CI results validate the commands in the workflow only; they do not establish that the project is a complete OS or that the kernel module was tested against a production kernel.

## Security and data

- Do not use the current login/session scaffolding as an authentication boundary for real users.
- Do not put credentials, access tokens, secrets, or private disk images into source control.
- Treat SQL files as design/prototype assets; review them against a disposable database before applying them.
- Review kernel code and compile it against matching headers before loading it.
- Keep generated build output and local disk artifacts out of commits.

## Contributing

1. Create a focused branch.
2. Keep each change small and document expected behavior.
3. Run `python3 tests/check_repository.py`, `make clean all`, and `make test` where the local platform supports them.
4. Include a brief test summary in the pull request and clearly identify untested platform-specific behavior.

## License

No repository-wide license is asserted here unless and until an explicit license file is added. The kernel module files include SPDX GPL-2.0 identifiers; review their licensing and dependencies independently.
