# AetherOS Nexus C source map

All runtime implementation files in this directory are C and Linux kernel headers.

- `aetheros_main.c`: module initialization and cleanup.
- `banner.c`: kernel-log startup and unload banners.
- `identity.c`: formats kernel release, architecture and AetherOS version.
- `proc.c`: read-only `/proc/aetheros_identity` interface.
- `aetheros_internal.h`: shared private declarations and constants.
- `Kbuild`: combines the C translation units into `aetheros_nexus.ko`.
- `Makefile`: developer build, clean and install targets.
- `Kconfig`: kernel configuration help entry.
- `build-module.sh`: invokes the matching kernel build tree.
- `aetheros.conf`: example module autoload configuration.

The module adds an identity endpoint and log branding only. It is not a full Linux kernel fork, boot splash, display manager, hardware POST implementation, or authentication system.
