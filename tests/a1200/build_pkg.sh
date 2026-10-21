#!/bin/bash
# Builds an A1200 package of the game: tests/a1200/<NAME>/ (its Run script
# and files) plus the game and the libraries it needs, into
# ~/doom3-build/m68k/pkg/<NAME>/. Run it with tools/a1200.py runpkg.
#   bash tests/a1200/build_pkg.sh start
#   bash tests/a1200/build_pkg.sh bench game game-o3 ...   (several builds, as
#     dhewm3-<build>; the bench package runs each)
# The game data is not in packages: it is on the Amiga once (DATA= of the
# Run scripts, default Stuff:doom3; tools/a1200_put_large.py puts it there).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
NAME=${1:?package name (a directory in tests/a1200)}
shift
BUILDS=("$@")
M=${M:-$HOME/doom3-build/m68k}
OGLES2_RELEASE=${OGLES2_RELEASE:-$HOME/ogles2-build/release/ogles2/Libs}
PKG=$M/pkg/$NAME

[ -d "$HERE/$NAME" ] || { echo "no $HERE/$NAME"; exit 1; }
[ -f "$OGLES2_RELEASE/ogles2.library" ] && [ -f "$OGLES2_RELEASE/v3dglslc" ] ||
    { echo "no ogles2 release in $OGLES2_RELEASE (ogles2.library, v3dglslc)"; exit 1; }
rm -rf "$PKG"
mkdir -p "$PKG/libs"
cp -r "$HERE/$NAME"/. "$PKG/"
if [ ${#BUILDS[@]} -eq 0 ]; then
    [ -f "$M/game/dhewm3" ] || { echo "no $M/game/dhewm3: run build.sh first"; exit 1; }
    cp "$M/game/dhewm3" "$PKG/"
else
    for b in "${BUILDS[@]}"; do
        [ -f "$M/$b/dhewm3" ] || { echo "no $M/$b/dhewm3: OUT_NAME=$b bash build.sh"; exit 1; }
        cp "$M/$b/dhewm3" "$PKG/dhewm3-$b"
    done
fi
# ogles2's release: the library and its on-target GLSL compiler, which it finds
# in PROGDIR:libs/ when LIBS: has none
cp "$OGLES2_RELEASE/ogles2.library" "$OGLES2_RELEASE/v3dglslc" "$PKG/libs/"
ls -l "$PKG" "$PKG/libs"
