# AetherOS Nexus — actual Linux kernel integration

This directory is an out-of-tree Linux kernel module written in C. Unlike the terminal mockup under `src/boot/aetheros.c`, this code is built against real Linux kernel headers and executes inside a running Linux kernel when loaded.

## What it does

- Writes the AetherOS Nexus identity banner to the kernel log.
- Creates read-only `/proc/aetheros_identity` with OS branding, running kernel release, and architecture.
- Uses kernel-managed `proc_ops` and `seq_file` APIs.
- Removes its proc entry when unloaded.

## Build

On a Linux host, install the development headers matching the running kernel and a C build toolchain, then run:

```sh
./kernel/aetheros/build-module.sh
```

Equivalent command:

```sh
make -C /lib/modules/$(uname -r)/build M="$PWD/kernel/aetheros" modules
```

For a separate kernel build tree, set `KDIR=/path/to/kernel/build`.

## Load and inspect (optional, privileged)

Only load a module you have built and reviewed on a test machine. Kernel modules execute with kernel privilege.

```sh
sudo insmod kernel/aetheros/aetheros.ko
dmesg | tail -n 20
cat /proc/aetheros_identity
sudo rmmod aetheros
```

If Secure Boot module-signing enforcement is enabled, sign the module with a locally trusted key or test in a VM configured for your workflow. Do not disable platform security just to test the module.

## What this is not

This module does not replace the upstream Linux kernel, alter kernel internals, implement firmware POST, draw a graphical boot splash, authenticate desktop users, or boot a kernel image. It is a real kernel-loadable branding component and a foundation for a larger OS integration. A graphical boot screen belongs in the bootloader, DRM/framebuffer early console, or userspace splash/display manager, not in a generic branding module.

The module has not been compiled against your target kernel in this environment. Build and test it against the exact kernel headers and version you intend to deploy.
