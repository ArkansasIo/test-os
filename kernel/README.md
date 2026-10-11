# Kernel integration

The `aetheros/` directory contains the first real Linux kernel integration component: a C kernel module that publishes OS identity information and emits a branded kernel-log banner.

This is distinct from the emulator's simulated POST screen. The module runs in Linux kernel context after it is loaded, but it does not replace or rebuild the Linux kernel itself. See [aetheros/README.md](aetheros/README.md) for build and safe test instructions.
