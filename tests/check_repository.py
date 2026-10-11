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
