# Project Status and Readiness

Last reviewed: 2026-10-17

## Current repository shape

The repository contains a C11 user-space command-line program, public C headers, JSON desktop menu configuration, SQL schema/query files, repository checks, and an optional out-of-tree Linux kernel module in `kernel/aetheros/`.

The current Makefile's tracked source list does not include the Z80 CPU, machine-bus, disk-image, ROM, or CP/M subsystem directories described in older documentation. Until these components and boot tests exist, describe the code as an OS-style prototype/workbench rather than a completed Z80/CP/M emulator.

## Capability boundaries

| Component | Current purpose | Not established by this code |
| --- | --- | --- |
| User-space CLI | Terminal demonstrations, menus/config loading, boot-flow simulation, sound sample/WAV generation | Real firmware POST, kernel loading, or CP/M boot |
| Login/session | Prototype user/session flow | Secure authentication or hardened authorization |
| SQL assets | Database schema/query experiments | Automatic migration, runtime integration, or production safety |
| AetherOS terminal screen | Native C rendering to a text UI | Full desktop GUI, compositor, or device driver |
| AetherOS kernel module | Kernel log banner and read-only `/proc/aetheros_identity` endpoint | Replacement kernel, hardware diagnostics, or bootloader |
| CI workflow | Linux user-space build and scripted checks | macOS/Windows parity or all-kernel compatibility |

## Engineering priorities

1. **Reproducible baseline:** run repository checks, clean build, and smoke tests on Linux; preserve logs when a job fails.
2. **Remove interface drift:** keep README examples, CLI help, workflow commands, test scripts, and Makefile targets synchronized.
3. **Choose a clear product scope:** either implement a real Z80/CP/M path with CPU tests and disk fixtures, or keep this repository explicitly scoped as an OS-style interface prototype.
4. **Validate CLI input:** validate numeric options such as runlevel; report malformed or missing values and test error cases.
5. **Review authentication claims:** do not promote the login/session code as secure authentication without password hashing, credential policy, authorization checks, and independent security tests.
6. **Review SQL deliberately:** use disposable database instances, migration/rollback scripts, constraints, and least-privilege accounts before runtime integration.
7. **Test kernel integration separately:** use a throwaway VM with matching kernel headers; verify load/unload cleanup and proc output. Do not require privileged module loading in ordinary CI.

## Validation commands

```sh
python3 tests/check_repository.py
make clean all
make test
sh tests/smoke.sh
```

Kernel-module validation is platform- and kernel-specific. Follow `kernel/aetheros/README.md` and load only reviewed code.
