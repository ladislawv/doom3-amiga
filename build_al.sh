#!/bin/bash
# The port's OpenAL subset (src/al/al.c; PLAN.md section 7, D7, phase 7) as a
# static library, for the A1200 and for the host:
#   ~/doom3-build/m68k/deps/lib/libd3al.a   linked by build.sh in place of d3es's OpenAL stub
#   ~/doom3-build/host-al/libd3al.a         linked by build_host.sh with AL=1, in place of OpenAL Soft
#   bash build_al.sh
# Needs build_sdl2.sh's output (SDL2's headers for the Amiga) and OpenAL's
# headers (the same ones OpenAL Soft installs: build.sh copies them to
# ~/doom3-build/m68k/al-include; on the host /usr/include/AL).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
TC16=${TC16:-$HOME/amiga-gcc162-install}
M=${M:-$HOME/doom3-build/m68k}
SDL=$M/sdl2
HOST=${HOST_OUT:-$HOME/doom3-build/host-al}
PATH=$(printf '%s' "$PATH" | tr ':' '\n' | grep -v '^/mnt/' | paste -sd:)
export PATH

[ -f "$SDL/inc/SDL2/SDL.h" ] || { echo "no $SDL/inc/SDL2: run build_sdl2.sh first"; exit 1; }
[ -f /usr/include/AL/al.h ] || { echo "no OpenAL headers: sudo apt install libopenal-dev"; exit 1; }
mkdir -p "$M/al-include" "$M/deps/lib" "$M/deps/obj/al" "$HOST"
cp -r /usr/include/AL "$M/al-include/"

"$TC16/bin/m68k-amigaos-gcc" -O2 -mcpu=68020 -m68881 -noixemul -fno-strict-aliasing -Wall -Wextra -Wno-unused-parameter \
    -I"$M/al-include" -I"$SDL/inc/SDL2" -I"$SDL/inc" -c "$HERE/src/al/al.c" -o "$M/deps/obj/al/al.o"
rm -f "$M/deps/lib/libd3al.a"
"$TC16/bin/m68k-amigaos-ar" rcs "$M/deps/lib/libd3al.a" "$M/deps/obj/al/al.o"

gcc -O2 -g -fPIC -Wall -Wextra -Wno-unused-parameter $(sdl2-config --cflags) -c "$HERE/src/al/al.c" -o "$HOST/al.o"
rm -f "$HOST/libd3al.a"
ar rcs "$HOST/libd3al.a" "$HOST/al.o"
ls -l "$M/deps/lib/libd3al.a" "$HOST/libd3al.a"
