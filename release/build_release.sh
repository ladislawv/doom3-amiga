#!/bin/bash
# Builds the release: the drawer ~/doom3-build/release/Doom3 (with its icon
# Doom3.info beside it), the archive Doom3-Amiga-<version>.lha and its
# Aminet-style readme. Run in WSL after build.sh (the game) and with ogles2's
# Pi 3 repository (ogles2pi3) next to this one (its committed 1.4-beta1 user
# archive, for the Pi 4 and the Pi 3, gives
# ogles2.library, v3dglslc and their licences):
#   bash release/build_release.sh        (into ~/doom3-build/release)
#   bash release/build_release.sh dist   (and copies the archive and readme
#                                         into release/dist/)
# What goes in, and from where:
#   dhewm3                 ~/doom3-build/m68k/game/dhewm3 (build.sh)
#   Doom3                  tests/a1200/play/src/doom3_launcher.c, built here
#   Doom3.script, *.cfg    tests/a1200/play/
#   libs/, licences/ogles2 ogles2's release/dist/ogles2-<OGLES2>.lha
#   shaders/               release/shaders/ (the game's 11 GLSL programs as
#                          ogles2 1.4-beta1's compiler made them on the A1200,
#                          the Pi 4's; the Pi 3 compiles its own at first use; to
#                          be made again after an ogles2 or compiler update)
#   icons                  release/icons/ (colour icons, made on the A1200 by
#                          release/src/mkicons.c from the system's defaults and
#                          the art of release/icons/art/make_icons.py:
#                          "mkicons DIR ART", ART holding its .d3ic files)
#   ReadMe                 ReadMe at the repository root, as it is (its version,
#                          date and source link written in, so that GitHub shows
#                          them; checked against VERSION below)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
VERSION=$(tr -d ' \n' < "$HERE/VERSION")
DATE=${DATE:-$(date +%Y-%m-%d)}
SOURCE=${SOURCE:-https://github.com/ladislawv/doom3-amiga}
TC16=${TC16:-$HOME/amiga-gcc162-install}
M=${M:-$HOME/doom3-build/m68k}
OUT=${OUT:-$HOME/doom3-build/release}
OGLES2=${OGLES2:-1.4-beta1}
OGLES2_DIST=${OGLES2_DIST:-$HOME/ogles2pi3-sdk/dist}	# tools/get_ogles2.sh: the tag v<OGLES2>'s archives
LHA=${LHA:-$HOME/tools/bin/lha}
D3ES_SRC=${D3ES_SRC:-$HOME/d3es}
SDL2_SRC=${SDL2_SRC:-$HOME/d3-sdl2}
DEPS_SRC=${DEPS:-$HOME/downloads/doom3-deps}
DIST=0
for a in "$@"; do
    case $a in
    dist) DIST=1 ;;
    *) echo "usage: build_release.sh [dist]"; exit 1 ;;
    esac
done

GAME=$M/game/dhewm3
[ -f "$GAME" ] || { echo "no $GAME: run build.sh first"; exit 1; }
[ -f "$OGLES2_DIST/ogles2-$OGLES2.lha" ] || { echo "no $OGLES2_DIST/ogles2-$OGLES2.lha (OGLES2_DIST=)"; exit 1; }
[ -x "$LHA" ] || { echo "no lha at $LHA (LHA=)"; exit 1; }
[ -f "$D3ES_SRC/COPYING.txt" ] && [ -f "$SDL2_SRC/libSDL2-amigaos3-main/LICENSE" ] ||
    { echo "run sources.sh first (d3es and SDL2_68k for their licences)"; exit 1; }
[ -f "$DEPS_SRC/zlib-1.3.1.tar.gz" ] || { echo "no $DEPS_SRC/zlib-1.3.1.tar.gz: run sources.sh first"; exit 1; }
# the ReadMe names this version in its title, its archive name and its history
for want in "Version $VERSION (" "Doom3-Amiga-$VERSION.lha"; do
    grep -qF "$want" "$REPO/ReadMe" || { echo "ReadMe lacks \"$want\": update it for $VERSION"; exit 1; }
done
grep -q "^$VERSION (" "$REPO/ReadMe" || { echo "ReadMe has no history entry for $VERSION"; exit 1; }

R=$OUT/Doom3
W=$OUT/work
rm -rf "$R" "$OUT/Doom3.info" "$W" "$OUT/Doom3-Amiga-$VERSION.lha" "$OUT/Doom3-Amiga-$VERSION.readme"
mkdir -p "$R/libs" "$R/shaders" "$R/base" "$R/demo" "$R/licences/ogles2" "$W"

# the launcher
"$TC16/bin/m68k-amigaos-gcc" -Os -s -mcpu=68020 -noixemul -Wall \
    "$REPO/tests/a1200/play/src/doom3_launcher.c" -o "$R/Doom3"

# the game, its script and settings
cp "$GAME" "$R/dhewm3"
cp "$REPO/tests/a1200/play/Doom3.script" "$R/"
cp "$REPO"/tests/a1200/play/{low,medium,high,ultra}.cfg "$R/"
cp "$HERE"/shaders/* "$R/shaders/"
cp "$HERE/base-ReadMe.txt" "$R/base/ReadMe.txt"
cp "$HERE/demo-ReadMe.txt" "$R/demo/ReadMe.txt"

# ogles2: its compiler and licences as its release ships them; the library from
# tools/get_ogles2.sh's folder (1.4-beta2, built from the ogles2pi3-beta2 branch)
(cd "$W" && "$LHA" xq "$OGLES2_DIST/ogles2-$OGLES2.lha")
OGLES2_LIB=${OGLES2_LIB:-$OGLES2_DIST/../ogles2/Libs/ogles2.library}
cp "$OGLES2_LIB" "$W/ogles2/Libs/v3dglslc" "$R/libs/"
cp "$W/ogles2/LICENSE" "$R/licences/ogles2/"
cp -r "$W/ogles2/licences/." "$R/licences/ogles2/"

# the other licences
cp "$D3ES_SRC/COPYING.txt" "$R/licences/Doom3-GPL.txt"
cp "$SDL2_SRC/libSDL2-amigaos3-main/LICENSE" "$R/licences/SDL2.txt"
tar -xzf "$DEPS_SRC/zlib-1.3.1.tar.gz" -C "$W" zlib-1.3.1/LICENSE
cp "$W/zlib-1.3.1/LICENSE" "$R/licences/zlib.txt"

# the texts
fill() { sed -e "s|@VERSION@|$VERSION|g" -e "s|@DATE@|$DATE|g" -e "s|@SOURCE@|$SOURCE|g" "$1"; }
fill "$REPO/ReadMe" > "$R/ReadMe"
fill "$HERE/Doom3-Amiga.readme" > "$OUT/Doom3-Amiga-$VERSION.readme"

# the icons
cp "$HERE/icons/Doom3-tool.info" "$R/Doom3.info"
cp "$HERE/icons/ReadMe.info" "$R/ReadMe.info"
cp "$HERE/icons/Doom3-drawer.info" "$OUT/Doom3.info"
rm -rf "$W"

# the archive: lh5, which every Amiga LhA reads (as ogles2's)
(cd "$OUT" && "$LHA" -ao5q "Doom3-Amiga-$VERSION.lha" Doom3 Doom3.info)
(cd "$OUT" && "$LHA" -l "Doom3-Amiga-$VERSION.lha" | tail -2)
ls -l "$OUT/Doom3-Amiga-$VERSION.lha"
echo "dhewm3 $(md5sum < "$R/dhewm3" | cut -d' ' -f1), ogles2.library $(md5sum < "$R/libs/ogles2.library" | cut -d' ' -f1)"

if [ $DIST = 1 ]; then
    mkdir -p "$HERE/dist"
    rm -f "$HERE"/dist/Doom3-Amiga-*.lha "$HERE"/dist/Doom3-Amiga-*.readme
    cp "$OUT/Doom3-Amiga-$VERSION.lha" "$OUT/Doom3-Amiga-$VERSION.readme" "$HERE/dist/"
    (cd "$HERE/dist" && sha256sum Doom3-Amiga-$VERSION.lha > SHA256SUMS)
    echo "copied to $HERE/dist"
fi
