#!/usr/bin/env python3
"""Fast repository invariants that do not require a MySQL server."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
errors = []

def fail(message):
    errors.append(message)

makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
source_block = re.search(r"(?ms)^SRC\s*:=\s*\\\n(.*?)(?=^\s*\nOBJ\s*:)", makefile)
if not source_block:
    fail("Could not parse SRC list from Makefile")
else:
    sources = re.findall(r"src/[^\\\s]+\.c", source_block.group(1))
    if not sources:
        fail("Makefile SRC list is empty")
    for source in sources:
        if not (ROOT / source).is_file():
            fail(f"Makefile references missing source: {source}")

# Every quoted project header used by C sources must be tracked under include/.
for source in (ROOT / "src").rglob("*.c"):
    text = source.read_text(encoding="utf-8", errors="replace")
    for header in re.findall(r'^\s*#\s*include\s*"([^"]+)"', text, re.M):
        local = source.parent / header
        shared = ROOT / "include" / header
        if not local.is_file() and not shared.is_file():
            fail(f"{source.relative_to(ROOT)} includes missing header {header}")

# Boot UI surfaces must be wired into the public header and CLI.
textui_header = (ROOT / "include/textui.h").read_text(encoding="utf-8")
textui_source = (ROOT / "src/boot/textui.c").read_text(encoding="utf-8")
frontend = (ROOT / "src/frontend/main.c").read_text(encoding="utf-8")
for label, condition in (
    ("POST declaration", "void textui_post_screen" in textui_header),
    ("POST implementation", "void textui_post_screen" in textui_source),
    ("boot-sequence option", '"--boot-sequence"' in frontend),
    ("boot-sequence dispatcher", "run_boot_sequence(void)" in frontend),
):
    if not condition:
        fail(f"Missing boot UI component: {label}")


# AetherOS must remain a native C terminal implementation.
aether_header = ROOT / "include/aetheros.h"
aether_source = ROOT / "src/boot/aetheros.c"
if not aether_header.is_file():
    fail("Missing native AetherOS header")
if not aether_source.is_file():
    fail("Missing native AetherOS implementation")
else:
    aether_text = aether_source.read_text(encoding="utf-8")
    if "void aetheros_render" not in aether_text:
        fail("AetherOS renderer function is missing")
    if "N E X U S" not in aether_text:
        fail("AetherOS Nexus branding is missing")
if "src/boot/aetheros.c" not in makefile:
    fail("AetherOS C source is not included in Makefile")
if '"--aetheros-screen"' not in frontend or "aetheros_render(s)" not in frontend:
    fail("AetherOS screen is not wired to the native C CLI")


# The kernel integration must be real, buildable C module source, not UI-only.
kernel_module = ROOT / "kernel/aetheros/aetheros_main.c"
kernel_makefile = ROOT / "kernel/aetheros/Makefile"
kernel_docs = ROOT / "kernel/aetheros/README.md"
if not kernel_module.is_file():
    fail("Missing AetherOS Linux kernel module source")
else:
    kernel_text = kernel_module.read_text(encoding="utf-8")
    for token in ("module_init(", "module_exit(", "MODULE_LICENSE("):
        if token not in kernel_text:
            fail(f"AetherOS kernel module missing required kernel integration: {token}")
if not kernel_makefile.is_file():
    fail("Missing AetherOS kernel module Makefile")
if not kernel_docs.is_file():
    fail("Missing AetherOS kernel build and test instructions")

# Catch accidental editor/chat debris before SQL's first executable statement.
allowed_starts = ("--", "/*", "CREATE ", "USE ", "INSERT ", "SET ")
for sql in sorted((ROOT / "sql").rglob("*.sql")):
    text = sql.read_text(encoding="utf-8", errors="replace").lstrip("\ufeff \t\r\n")
    if not text:
        fail(f"{sql.relative_to(ROOT)} is empty")
        continue
    if not text.upper().startswith(tuple(x.upper() for x in allowed_starts)):
        fail(f"{sql.relative_to(ROOT)} starts with unexpected text: {text.splitlines()[0][:80]!r}")

if errors:
    print("Repository checks FAILED:")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)
print("Repository checks passed: Makefile sources, local headers, and SQL file starts.")
