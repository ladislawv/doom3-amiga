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

DEPS=${DEPS_OUT:-$HOME/doom3-build/m68k/deps}
[ -f "$SDL/libSDL2.a" ] || { echo "no $SDL/libSDL2.a: run build_sdl2.sh first"; exit 1; }
[ -f "$DEPS/lib/libd3amiga.a" ] || { echo "no $DEPS/lib/libd3amiga.a: run build_deps.sh first"; exit 1; }
rm -rf "$PKG"
mkdir -p "$PKG"

# D2's flags: -m68020 -m68881 (fintrz for float-to-int), libnix, no strict aliasing.
# No libnix stack swap: linked in (-Wl,-u,___stkinit), it gave main() its 8 MB
# stack on the A1200 but hung in its exit code (run 2, 2026-10-20). The Shell's
# Stack command gives the stack instead (Run-Probe). SWAP=1 builds the swap
# variant too, as cxx_probe_swap, to look at that hang again.
VARIANTS="cxx_probe:"
[ -n "${SWAP:-}" ] && VARIANTS="$VARIANTS cxx_probe_swap:-Wl,-u,___stkinit"
for v in $VARIANTS; do
    "$TC16/bin/m68k-amigaos-g++" -std=gnu++17 -O2 -mcpu=68020 -m68881 -noixemul -fno-strict-aliasing \
        -Wall -Wno-format -I"$SDL/inc/SDL2" -I"$SDL/inc" \
        "$HERE/cxx-probe/src/cxx_probe.cpp" -o "$PKG/${v%%:*}" \
        ${v#*:} \
        -Wl,-u,___xpg_strerror_r -L"$DEPS/lib" -ld3amiga \
        -L"$SDL" -lSDL2 -L"$OGLES2_BUILD" -logles2 -L"$SDL/pistorm3d-sdk/lib" -lminigl -ldebug -lm
done
cp "$HERE/cxx-probe/Run-Probe" "$PKG/"
# ogles2.library in libs/, which OGLES2Open finds when LIBS: has none (an
# installed one in LIBS: wins; Run-Probe records which).
mkdir -p "$PKG/libs"
cp "$OGLES2_BUILD/ogles2.library" "$PKG/libs/"

ls -l "$PKG"
