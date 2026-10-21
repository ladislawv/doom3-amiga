#!/bin/bash
# The Amiga build's libraries besides SDL2 (PLAN.md §8, phase 2), with GCC 16.2,
# libnix and the D2 flags, into ~/doom3-build/m68k/deps:
#   lib/libz.a, include/zlib.h, zconf.h   zlib 1.3.1 (sources.sh fetches it)
#   lib/libd3amiga.a                       the port's own: the allocator
#                                          (src/amiga/d3malloc.c, dlmalloc),
#                                          operator new/delete on it, and the
#                                          libnix shims
#   bash build_deps.sh
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
TC16=${TC16:-$HOME/amiga-gcc162-install}
DEPS_SRC=${DEPS:-$HOME/downloads/doom3-deps}
O=${O:-$HOME/doom3-build/m68k/deps}
CC="$TC16/bin/m68k-amigaos-gcc"
CXX="$TC16/bin/m68k-amigaos-g++"
AR="$TC16/bin/m68k-amigaos-ar"
FLAGS="-O2 -mcpu=68020 -m68881 -noixemul -fno-strict-aliasing"

[ -f "$DEPS_SRC/zlib-1.3.1.tar.gz" ] && [ -f "$DEPS_SRC/malloc-2.8.6.c" ] || { echo "run sources.sh first"; exit 1; }
rm -rf "$O"
mkdir -p "$O/lib" "$O/include" "$O/obj/zlib" "$O/obj/d3amiga"

# zlib: its library sources compiled directly (its configure script does not
# know the target).
tar -xzf "$DEPS_SRC/zlib-1.3.1.tar.gz" -C "$O/obj"
Z=$O/obj/zlib-1.3.1
for f in adler32 compress crc32 deflate gzclose gzlib gzread gzwrite infback inffast inflate inftrees trees uncompr zutil; do
    "$CC" $FLAGS -DZ_HAVE_UNISTD_H -w -c "$Z/$f.c" -o "$O/obj/zlib/$f.o"
done
"$AR" rcs "$O/lib/libz.a" "$O"/obj/zlib/*.o
cp "$Z/zlib.h" "$Z/zconf.h" "$O/include/"

# The port's support library.
"$CC" $FLAGS -Wall -I"$DEPS_SRC" -c "$HERE/src/amiga/d3malloc.c" -o "$O/obj/d3amiga/d3malloc.o"
"$CXX" -std=gnu++17 $FLAGS -Wall -c "$HERE/src/amiga/d3new.cpp" -o "$O/obj/d3amiga/d3new.o"
"$CC" $FLAGS -Wall -c "$HERE/src/amiga/libnix_shims.c" -o "$O/obj/d3amiga/libnix_shims.o"
"$CC" $FLAGS -Wall -c "$HERE/src/amiga/cxa_atexit.c" -o "$O/obj/d3amiga/cxa_atexit.o"
"$AR" rcs "$O/lib/libd3amiga.a" "$O"/obj/d3amiga/*.o
ls -l "$O/lib"
