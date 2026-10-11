#!/bin/sh
set -eu
./build/osborne1 --help >/dev/null
./build/osborne1 --menu --script >/dev/null
./build/osborne1 --textui >/dev/null
echo "CLI smoke tests passed."
