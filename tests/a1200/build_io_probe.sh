#!/bin/bash
# Builds the pk4 I/O probe (PLAN.md section 14) into an A1200 package:
# ~/doom3-build/m68k/pkg/io-probe/ (io_probe and Run-IO).
#   bash tests/a1200/build_io_probe.sh
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
TC16=${TC16:-$HOME/amiga-gcc162-install}
PKG=${PKG:-$HOME/doom3-build/m68k/pkg/io-probe}
rm -rf "$PKG"
mkdir -p "$PKG"
"$TC16/bin/m68k-amigaos-gcc" -O2 -mcpu=68020 -m68881 -noixemul -Wall \
    "$HERE/io-probe/src/io_probe.c" -o "$PKG/io_probe"
cp "$HERE/io-probe/Run-IO" "$PKG/"
ls -l "$PKG"
