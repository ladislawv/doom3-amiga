#!/bin/bash
# The Amiga build (PLAN.md §8, phase 3): d3es for AmigaOS 3 (68k) with GCC 16.2
# and libnix, the port's patches and its system layer (src/sys/amigaos3).
#   bash build.sh            reset ~/d3es, apply patches/, build
#   AMIGA_OPT="-O3 ..."      other optimisation flags in place of d3es's Release
#                            ones (-O2 -fno-math-errno -fno-trapping-math -fomit-frame-pointer)
#   OUT_NAME=game-o3         build into ~/doom3-build/m68k/<OUT_NAME> (default game)
#   OGLES=3                  on ogles3.library: SDL from OGLES=3 build_sdl2.sh,
#                            libogles3.a from OGLES3_SDK (default OUT_NAME game-ogles3)
# Needs build_sdl2.sh's and build_deps.sh's output. Output:
# ~/doom3-build/m68k/game/dhewm3 (the base game, with the game code linked in).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
D3ES_SRC=${D3ES_SRC:-$HOME/d3es}
TC16=${TC16:-$HOME/amiga-gcc162-install}
OGLES=${OGLES:-2}
OGLES2_BUILD=${OGLES2_BUILD:-$HOME/ogles2-build/m68k}
OGLES3_SDK=${OGLES3_SDK:-$HOME/ogles3-build/release/ogles3/SDK}
M=${M:-$HOME/doom3-build/m68k}
if [ "$OGLES" = 3 ]; then
    OUT=$M/${OUT_NAME:-game-ogles3}
    SDL=$M/sdl2-ogles3
    GLLIB=$OGLES3_SDK/lib/libogles3.a
else
    OUT=$M/${OUT_NAME:-game}
    SDL=$M/sdl2
    GLLIB=$OGLES2_BUILD/libogles2.a
fi
DEPS=$M/deps
export TC16
# include directories for every file (cmake/m68k-amigaos.cmake): GLES2/, KHR/
# and the ogles2 headers, as build_sdl2.sh collected them
export AMIGA_INCLUDES="-I$SDL/inc"

[ -d "$D3ES_SRC/.git" ] || { echo "no $D3ES_SRC: run sources.sh first"; exit 1; }
[ -f "$SDL/libSDL2.a" ] || { echo "no $SDL/libSDL2.a: run build_sdl2.sh first (with OGLES=$OGLES)"; exit 1; }
[ -f "$GLLIB" ] || { echo "no $GLLIB"; exit 1; }
[ -f "$DEPS/lib/libz.a" ] && [ -f "$DEPS/lib/libd3amiga.a" ] || { echo "no $DEPS: run build_deps.sh first"; exit 1; }
[ -f /usr/include/AL/al.h ] || { echo "no OpenAL headers: sudo apt install libopenal-dev"; exit 1; }

# The clone: pinned commit, the port's patches (staged), the system layer.
git -C "$D3ES_SRC" reset -q --hard
git -C "$D3ES_SRC" clean -qfdx
for p in "$HERE"/patches/*.patch; do
    git -C "$D3ES_SRC" apply "$p" || { echo "$(basename "$p") does not apply"; exit 1; }
done
git -C "$D3ES_SRC" add -A
mkdir -p "$D3ES_SRC/neo/sys/amigaos3"
cp "$HERE"/src/sys/amigaos3/* "$D3ES_SRC/neo/sys/amigaos3/"

# OpenAL's headers only (the library is d3es's stub until the sound phase).
mkdir -p "$M/al-include"
cp -r /usr/include/AL "$M/al-include/"

# Windows entries out of PATH (WSL 1 passes them through; CMake searches them).
PATH=$(printf '%s' "$PATH" | tr ':' '\n' | grep -v '^/mnt/' | paste -sd:)
export PATH

# The port's OpenAL subset (build_al.sh) when it is built, else d3es's stub (patch 0024).
ALLIB=
[ -f "$DEPS/lib/libd3al.a" ] && ALLIB=$DEPS/lib/libd3al.a

# Release: d3es's -O2 -fomit-frame-pointer without assertions.
# AMIGA_LIBS go after SDL2 on the link line; -u ___xpg_strerror_r pulls the
# libnix shim from libd3amiga.a ahead of libstdc++, which needs it. No -lamiga:
# the driver adds amiga.lib after libnix itself, and named earlier its
# RawDoFmt sprintf (16-bit %d) replaced the C library's (the first A1200 start
# crashed in it).
rm -rf "$OUT"
cmake -S "$D3ES_SRC/neo" -B "$OUT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$HERE/cmake/m68k-amigaos.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DHARDLINK_GAME=ON -DBASE=ON -DD3XP=OFF -DDEDICATED=OFF -DCORE=ON -DSDL2=ON -DSINGLE_TASK=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_CURL=ON \
    -DZLIB_INCLUDE_DIR="$DEPS/include" -DZLIB_LIBRARY="$DEPS/lib/libz.a" \
    -DSDL2_INCLUDE_DIR="$SDL/inc/SDL2" -DSDL2_LIBRARY="$SDL/libSDL2.a" \
    -DOPENAL_INCLUDE_DIR="$M/al-include" \
    "-DAMIGA_LIBS=$DEPS/lib/libd3amiga.a;$GLLIB;$SDL/pistorm3d-sdk/lib/libminigl.a;-ldebug;-lm" \
    "-DAMIGA_LDFLAGS=-Wl,-u,___xpg_strerror_r" \
    "-DAMIGA_OPT=${AMIGA_OPT:-}"     ${ALLIB:+"-DAMIGA_OPENAL_LIBRARY=$ALLIB"} \
    >"$OUT.cmake.log" 2>&1 || { tail -40 "$OUT.cmake.log"; exit 1; }
ninja -C "$OUT" -k 0 >"$OUT.build.log" 2>&1 || {
    grep -E "error|undefined reference" "$OUT.build.log" | sort | uniq -c | sort -rn | head -40
    echo "build failed: $OUT.build.log"; exit 1; }
ls -l "$OUT/dhewm3"
