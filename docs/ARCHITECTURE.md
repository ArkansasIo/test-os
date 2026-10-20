# AetherOS Nexus — project architecture

## Project layers

- **Native application:** C11 code compiled by the root `Makefile`. Its current command-line interface and source tree are authoritative for the native program.
- **Kernel experiment:** `kernel/aetheros/` holds a Linux loadable-module scaffold. It is not a complete independent OS kernel and should be tested only against a matching Linux kernel build tree.
- **Data layer:** `sql/` holds schema and query drafts. A database server is not required for the static website.
- **Web documentation:** `docs/` is a static GitHub Pages site. Its console uses a fixed command allowlist in `app.js`; it does not run shell commands or read server state.

## Source-of-truth rules

1. The C build is not replaced by the browser console.
2. UI status panels are labelled simulated unless they are backed by real runtime telemetry.
3. Do not expose credentials, host files, privileged actions, or arbitrary command execution through the public demo.
4. Keep the OS simulator's historical Osborne/CP/M context clearly distinguished from AetherOS product concepts.
5. Treat all kernel module code as experimental until compiled and tested against the specific target kernel.

## Local validation

```sh
python3 tests/check_repository.py
make clean all
make test
```

## GitHub Pages

The Pages workflow validates the static assets and deploys the `docs/` directory using GitHub Actions. In repository Settings → Pages, select **GitHub Actions** as the build and deployment source. First-time deployment may require that repository setting be enabled by an administrator.
