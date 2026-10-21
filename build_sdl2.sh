#!/bin/bash
# SDL2 for the Amiga build (PLAN.md §8, phase 2): SDL2_68k (~/d3-sdl2, pinned by
# sources.sh) with the ogles2 GLES2 backend (sdl2/, copies of ogles2's
# ports/sdl2), built with GCC 16.2 and libnix (Makefile.libnix), the runtime
# Doom 3's C++ needs. Output: ~/doom3-build/m68k/sdl2/libSDL2.a and inc/.
#   bash build_sdl2.sh
# Uses, read only: ogles2's headers and import library (OGLES2_REPO,
# OGLES2_BUILD) and PiStorm3D's SDK for SDL2's MiniGL backend (PISTORM3D_SDK).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
SDL2_SRC=${SDL2_SRC:-$HOME/d3-sdl2}
TC16=${TC16:-$HOME/amiga-gcc162-install}
OGLES2_REPO=${OGLES2_REPO:-$HERE/../ogles2}	# next to this repository, as tools/a1200.py expects
OGLES2_BUILD=${OGLES2_BUILD:-$HOME/ogles2-build/m68k}
PISTORM3D_SDK=${PISTORM3D_SDK:-$HERE/../pistorm3d/SDK}
O=${O:-$HOME/doom3-build/m68k/sdl2}
export PATH="$TC16/bin:$PATH"	# the Makefile calls m68k-amigaos-gcc and vasmm68k_mot

[ -d "$SDL2_SRC/.git" ] || { echo "no $SDL2_SRC: run sources.sh first"; exit 1; }
[ -x "$TC16/bin/m68k-amigaos-gcc" ] || { echo "no GCC 16.2 in $TC16"; exit 1; }
[ -f "$OGLES2_BUILD/libogles2.a" ] && [ -f "$OGLES2_BUILD/gen/include/proto/ogles2.h" ] ||
    { echo "no ogles2 build in $OGLES2_BUILD (libogles2.a, gen/include)"; exit 1; }
[ -d "$PISTORM3D_SDK/minigl-shared-library/include" ] || { echo "no PiStorm3D SDK at $PISTORM3D_SDK"; exit 1; }

# SDL2_68k clean, then the patches and the backend's sources.
git -C "$SDL2_SRC" reset -q --hard
git -C "$SDL2_SRC" clean -qfdx
for p in "$HERE"/sdl2/patches/*.patch; do
    git -C "$SDL2_SRC" apply "$p" || { echo "sdl2: $(basename "$p") does not apply"; exit 1; }
done
L=$SDL2_SRC/libSDL2-amigaos3-main
cp "$HERE"/sdl2/src/video/amigaos3/SDL_os3opengles2.[ch] "$L/src/video/amigaos3/"

# Headers on paths without spaces (the Makefile splits CFLAGS on them).
rm -rf "$O"
mkdir -p "$O/inc"
cp -r "$OGLES2_REPO"/include/. "$OGLES2_BUILD"/gen/include/. "$O/inc/"
cp -r "$PISTORM3D_SDK" "$O/pistorm3d-sdk"	# whole: its headers include ../../../backend/...
MGLINC=$O/pistorm3d-sdk/minigl-shared-library/include
# The AHI and CyberGraphX SDK headers: the GCC 6.5 toolchain has them (its
# make sdk=...), the GCC 16.2 one does not. Copied, so neither toolchain changes.
TC6INC=${TC6INC:-$HOME/amiga-gcc-install/m68k-amigaos/include}
(cd "$TC6INC" && find . \( -iname '*ahi*' -o -iname '*cybergraph*' \) -type f -print0 | tar --null -cf - -T -) |
    tar -xf - -C "$O/inc"
[ -f "$O/inc/devices/ahi.h" ] && [ -f "$O/inc/proto/cybergraphics.h" ] || { echo "no AHI/CyberGraphX headers in $TC6INC"; exit 1; }

make -C "$L" -f Makefile.libnix -j8 ${MAKEK:-} OGLES2_CFLAGS="-I$O/inc -I$MGLINC" >"$O/make.log" 2>&1 ||
    { grep -E "error|Error" "$O/make.log" | head -30; echo "sdl2: make failed ($O/make.log)"; exit 1; }
cp "$L"/build/libnix/libSDL2*.a "$O/"
cp -r "$L/include" "$O/inc/SDL2"
ls -l "$O"/libSDL2*.a
