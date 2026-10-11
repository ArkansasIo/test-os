#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
KDIR=${KDIR:-"/lib/modules/$(uname -r)/build"}

if [ ! -d "$KDIR" ]; then
    echo "Kernel build directory not found: $KDIR" >&2
    echo "Install matching Linux kernel headers or set KDIR=/path/to/kernel/build." >&2
    exit 2
fi

make -C "$KDIR" M="$ROOT" modules
printf '\nBuilt AetherOS module. Review the kernel version and module signing policy before loading.\n'
