# Osborne Desktop

A lightweight desktop environment that runs on the Linux kernel and hosts the
Osborne 1 emulator as an application window.

## Structure

```
os/
  kernel/     Linux kernel build config, cmdline, initramfs hooks
  boot/       GRUB + init scripts, systemd units
  desktop/    window manager, panel, launcher, theme
    menus/    menu + submenu definitions (JSON) and renderer
  server/     network + storage services for the "server" mode
  seed/       seed files: users, groups, app registry, desktop entries
  etc/        system config
```

## Modes

- **Desktop mode** — `os-desk` starts the panel, launcher and app grid.
- **Server mode** — `os-served` starts services via the init system, exposes
  the emulator over VNC and a REST API.

Start either with:

```
os-desk                      # desktop session
os-served --port 5900        # headless server session
```
