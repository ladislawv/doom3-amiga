#!/bin/bash
# SDL2 for the Amiga build (PLAN.md §8, phase 2): SDL2_68k (~/d3-sdl2, pinned by
# sources.sh) with the ogles2 GLES2 backend (sdl2/, copies of ogles2's
# ports/sdl2), built with GCC 16.2 and libnix (Makefile.libnix), the runtime
# Doom 3's C++ needs. Output: ~/doom3-build/m68k/sdl2/libSDL2.a and inc/.
#   bash build_sdl2.sh
#   OGLES=3 bash build_sdl2.sh   the same on ogles3.library (sdl2/ogles3, copies
#                                of ogles3's ports/sdl2; PLAN.md §14), into
#                                ~/doom3-build/m68k/sdl2-ogles3
# Uses, read only: ogles2's headers and import library (OGLES2_REPO,
# OGLES2_BUILD), or ogles3's release SDK (OGLES3_SDK), and PiStorm3D's SDK for
# SDL2's MiniGL backend (PISTORM3D_SDK).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
SDL2_SRC=${SDL2_SRC:-$HOME/d3-sdl2}
TC16=${TC16:-$HOME/amiga-gcc162-install}
OGLES=${OGLES:-2}
OGLES2_REPO=${OGLES2_REPO:-$HERE/../ogles2}	# next to this repository, as tools/a1200.py expects
OGLES2_BUILD=${OGLES2_BUILD:-$HOME/ogles2-build/m68k}
OGLES3_SDK=${OGLES3_SDK:-$HOME/ogles3-build/release/ogles3/SDK}	# release/build_release.sh in ogles3
PISTORM3D_SDK=${PISTORM3D_SDK:-$HERE/../pistorm3d/SDK}
if [ "$OGLES" = 3 ]; then O=${O:-$HOME/doom3-build/m68k/sdl2-ogles3}; else O=${O:-$HOME/doom3-build/m68k/sdl2}; fi
export PATH="$TC16/bin:$PATH"	# the Makefile calls m68k-amigaos-gcc and vasmm68k_mot

[ -d "$SDL2_SRC/.git" ] || { echo "no $SDL2_SRC: run sources.sh first"; exit 1; }
[ -x "$TC16/bin/m68k-amigaos-gcc" ] || { echo "no GCC 16.2 in $TC16"; exit 1; }
case $OGLES in
2) [ -f "$OGLES2_BUILD/libogles2.a" ] && [ -f "$OGLES2_BUILD/gen/include/proto/ogles2.h" ] ||
       { echo "no ogles2 build in $OGLES2_BUILD (libogles2.a, gen/include)"; exit 1; } ;;
3) [ -f "$OGLES3_SDK/lib/libogles3.a" ] && [ -f "$OGLES3_SDK/include/proto/ogles3.h" ] ||
       { echo "no ogles3 SDK in $OGLES3_SDK (lib/libogles3.a, include/proto/ogles3.h)"; exit 1; } ;;
*) echo "OGLES is 2 or 3"; exit 1 ;;
esac
[ -d "$PISTORM3D_SDK/minigl-shared-library/include" ] || { echo "no PiStorm3D SDK at $PISTORM3D_SDK"; exit 1; }

# SDL2_68k clean, then the patches and the backend's sources. On ogles3, its
# copies of 0001 and 0002 and of the backend replace ogles2's; the rest are the
# same patches, whose context lines naming ogles2's header variable and proto
# file (0100) take ogles3's names.
git -C "$SDL2_SRC" reset -q --hard
git -C "$SDL2_SRC" clean -qfdx
for p in "$HERE"/sdl2/patches/*.patch; do
    if [ "$OGLES" = 3 ]; then
        q=$HERE/sdl2/ogles3/patches/$(basename "$p")
        [ -f "$q" ] && p=$q
        sed 's/OGLES2_CFLAGS/OGLES3_CFLAGS/g; s|proto/ogles2\.h|proto/ogles3.h|g' "$p" | git -C "$SDL2_SRC" apply ||
            { echo "sdl2: $(basename "$p") does not apply"; exit 1; }
    else
        git -C "$SDL2_SRC" apply "$p" || { echo "sdl2: $(basename "$p") does not apply"; exit 1; }
    fi
done
L=$SDL2_SRC/libSDL2-amigaos3-main
B=$HERE/sdl2
[ "$OGLES" = 3 ] && B=$HERE/sdl2/ogles3
cp "$B"/src/video/amigaos3/SDL_os3opengles2.[ch] "$L/src/video/amigaos3/"

# Headers on paths without spaces (the Makefile splits CFLAGS on them).
rm -rf "$O"
mkdir -p "$O/inc"
if [ "$OGLES" = 3 ]; then
    cp -r "$OGLES3_SDK"/include/. "$O/inc/"
else
    cp -r "$OGLES2_REPO"/include/. "$OGLES2_BUILD"/gen/include/. "$O/inc/"
fi
cp -r "$PISTORM3D_SDK" "$O/pistorm3d-sdk"	# whole: its headers include ../../../backend/...
MGLINC=$O/pistorm3d-sdk/minigl-shared-library/include
# The AHI and CyberGraphX SDK headers: the GCC 6.5 toolchain has them (its
# make sdk=...), the GCC 16.2 one does not. Copied, so neither toolchain changes.
TC6INC=${TC6INC:-$HOME/amiga-gcc-install/m68k-amigaos/include}
(cd "$TC6INC" && find . \( -iname '*ahi*' -o -iname '*cybergraph*' \) -type f -print0 | tar --null -cf - -T -) |
    tar -xf - -C "$O/inc"
[ -f "$O/inc/devices/ahi.h" ] && [ -f "$O/inc/proto/cybergraphics.h" ] || { echo "no AHI/CyberGraphX headers in $TC6INC"; exit 1; }

make -C "$L" -f Makefile.libnix -j8 ${MAKEK:-} OGLES${OGLES}_CFLAGS="-I$O/inc -I$MGLINC" >"$O/make.log" 2>&1 ||
    { grep -E "error|Error" "$O/make.log" | head -30; echo "sdl2: make failed ($O/make.log)"; exit 1; }
cp "$L"/build/libnix/libSDL2*.a "$O/"
cp -r "$L/include" "$O/inc/SDL2"
ls -l "$O"/libSDL2*.a
