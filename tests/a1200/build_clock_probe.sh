#!/bin/bash
# Builds the clock probe (PLAN.md section 14) into an A1200 package:
# ~/doom3-build/m68k/pkg/clock-probe/ (clock_probe and Run-Clock).
#   bash tests/a1200/build_clock_probe.sh
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
TC16=${TC16:-$HOME/amiga-gcc162-install}
PKG=${PKG:-$HOME/doom3-build/m68k/pkg/clock-probe}
rm -rf "$PKG"
mkdir -p "$PKG"
"$TC16/bin/m68k-amigaos-gcc" -O2 -mcpu=68020 -m68881 -noixemul -Wall \
    "$HERE/clock-probe/src/clock_probe.c" -o "$PKG/clock_probe"
cp "$HERE/clock-probe/Run-Clock" "$PKG/"
ls -l "$PKG"
