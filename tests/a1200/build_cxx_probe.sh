#!/bin/bash
# Builds the phase 2 C++ probe (PLAN.md §8) into an A1200 package:
# ~/doom3-build/m68k/pkg/cxx-probe/ (cxx_probe and Run-Probe). Needs
# build_sdl2.sh's output.
#   bash tests/a1200/build_cxx_probe.sh
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
TC16=${TC16:-$HOME/amiga-gcc162-install}
OGLES2_BUILD=${OGLES2_BUILD:-$HOME/ogles2-build/m68k}
SDL=${SDL:-$HOME/doom3-build/m68k/sdl2}
PKG=${PKG:-$HOME/doom3-build/m68k/pkg/cxx-probe}

[ -f "$SDL/libSDL2.a" ] || { echo "no $SDL/libSDL2.a: run build_sdl2.sh first"; exit 1; }
rm -rf "$PKG"
mkdir -p "$PKG"

# D2's flags: -m68020 -m68881 (fintrz for float-to-int), libnix, no strict aliasing.
"$TC16/bin/m68k-amigaos-g++" -std=gnu++17 -O2 -mcpu=68020 -m68881 -noixemul -fno-strict-aliasing \
    -Wall -Wno-format -I"$SDL/inc/SDL2" -I"$SDL/inc" \
    "$HERE/cxx-probe/src/cxx_probe.cpp" -x c "$HERE/../../src/amiga/libnix_shims.c" -x none -o "$PKG/cxx_probe" \
    -Wl,-u,___stkinit \
    -L"$SDL" -lSDL2 -L"$OGLES2_BUILD" -logles2 -L"$SDL/pistorm3d-sdk/lib" -lminigl -ldebug -lamiga -lm
# -u ___stkinit: libnix's swapstack.o (which reads __stack and registers itself in
# the init list) is otherwise not linked, since nothing refers to it.
cp "$HERE/cxx-probe/Run-Probe" "$PKG/"

# The stack swap is in libnix's swapstack.o, linked only when referenced:
# report whether it made it in (the probe checks the size on the Amiga).
if "$TC16/bin/m68k-amigaos-nm" "$PKG/cxx_probe" 2>/dev/null | grep -q "___stkinit"; then
    echo "stack swap linked (___stkinit present)"
else
    echo "WARNING: ___stkinit not linked: __stack will have no effect"
fi
ls -l "$PKG"
