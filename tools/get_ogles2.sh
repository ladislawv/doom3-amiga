#!/bin/bash
# The one ogles2 this project uses: the shipped ogles2.library 1.4-beta1 of
# ogles2pi3 (the Pi 4 and the Pi 3; github.com/ladislawv/ogles2pi3), from its
# committed SDK archive. Unpacks it into ~/ogles2pi3-sdk:
#   ogles2/Libs/ogles2.library, ogles2/Libs/v3dglslc   (packages, benchmarks)
#   ogles2/SDK/include, ogles2/SDK/lib/libogles2.a     (build_sdl2.sh, build.sh)
# The release takes the user archive from the same folder (release/build_release.sh).
#   bash tools/get_ogles2.sh
# OGLES2_DIST: ogles2pi3's release/dist (default: ../ogles2pi3 next to this repository).
set -eu
HERE=$(cd "$(dirname "$0")/.." && pwd)
OGLES2=${OGLES2:-1.4-beta1}
OGLES2_DIST=${OGLES2_DIST:-$HERE/../ogles2pi3/release/dist}
OUT=${OGLES2_HOME:-$HOME/ogles2pi3-sdk}
LHA=${LHA:-$HOME/tools/bin/lha}
A=$OGLES2_DIST/ogles2-$OGLES2-sdk.lha
[ -f "$A" ] || { echo "no $A (OGLES2_DIST=)"; exit 1; }
rm -rf "$OUT"
mkdir -p "$OUT"
(cd "$OUT" && "$LHA" xq "$A")
echo "$OGLES2" > "$OUT/VERSION"
strings "$OUT/ogles2/Libs/ogles2.library" | grep -m1 '\$VER'
ls "$OUT/ogles2/Libs" "$OUT/ogles2/SDK/lib"
