# Contributing

Thanks for contributing to Test OS / AetherOS Nexus.

## Before a pull request

Run the checks supported by your environment:

```sh
python3 tests/check_repository.py
make clean all
make test
sh tests/smoke.sh
```

Use Linux for the canonical CI-compatible test run. Windows builds use MinGW-w64 and `mingw32-make`; run shell-specific commands separately.

## Engineering practices

- Keep C code compatible with C11 and compile with `-Wall -Wextra`.
- Clearly label simulated boot steps; do not present them as actual kernel or firmware execution.
- Keep kernel-module code, documentation, and testing separate from the user-space terminal UI.
- Validate CLI inputs and cover error paths.
- Never commit passwords, API keys, private disk images, local build output, or personal machine data.
- Treat SQL schemas as experimental until tested against a disposable database with a documented migration plan.
- Kernel modules run with kernel privilege. Review carefully and test against matching target-kernel headers before loading.
- Update project documentation whenever behavior, supported platforms, or test commands change.

## Pull request checklist

- [ ] Scope and expected behavior are described.
- [ ] The change builds without new compiler warnings.
- [ ] Relevant tests pass or failures are explained.
- [ ] Documentation matches actual behavior.
- [ ] Platform-specific limitations are identified.
- [ ] No credentials or generated artifacts are included.
