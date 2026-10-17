# Security Policy

## Scope

This repository is an experimental systems-programming prototype. It is not a hardened operating system, a production authentication platform, or a replacement for upstream Linux.

## Reporting a vulnerability

Do not publish credentials, private personal data, or exploit details in public issues. Use GitHub's private vulnerability reporting feature for this repository if enabled. Otherwise, contact the repository maintainer using a private channel listed on the repository profile.

Include the affected component, impact, platform/kernel/compiler versions, and safe reproduction steps. Only test systems you own or are authorized to assess.

## Security-sensitive components

- `src/login/`: prototype login/session code; do not use it as a production authentication boundary.
- `sql/`: experimental schemas; review privileges, sensitive-data handling, and rollback behavior before use.
- `kernel/aetheros/`: kernel code executes with high privilege; build against matching headers and test in a disposable VM.
- `os/desktop/menus/`: validate schema and bounds if untrusted configuration is ever supported.

Keep toolchains updated, avoid committing secrets, review kernel-facing changes carefully, and run project tests before accepting changes.
