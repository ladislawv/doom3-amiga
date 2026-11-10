#!/bin/bash
# The one ogles2 this project uses: the shipped ogles2.library 1.4-beta1 of
# ogles2pi3 (the Pi 4 and the Pi 3; github.com/ladislawv/ogles2pi3), as its
# published release has it: the archives committed at the tag v<OGLES2> in the
# ogles2pi3 repository next to this one (whatever branch is checked out there),
# checked against the tag's SHA256SUMS. Into ~/ogles2pi3-sdk:
#   dist/ogles2-<OGLES2>.lha, dist/ogles2-<OGLES2>-sdk.lha  (release/build_release.sh)
#   ogles2/Libs/ogles2.library, ogles2/Libs/v3dglslc        (packages, benchmarks)
#   ogles2/SDK/include, ogles2/SDK/lib/libogles2.a          (build_sdl2.sh, build.sh)
#   bash tools/get_ogles2.sh
# After a new ogles2pi3 release: git -C ../ogles2pi3 fetch --tags --force, a new OGLES2.
set -eu
HERE=$(cd "$(dirname "$0")/.." && pwd)
OGLES2=${OGLES2:-1.4-beta1}
OGLES2_REPO=${OGLES2_REPO:-$HERE/../ogles2pi3}
OUT=${OGLES2_HOME:-$HOME/ogles2pi3-sdk}
LHA=${LHA:-$HOME/tools/bin/lha}
TAG=v$OGLES2
git -C "$OGLES2_REPO" rev-parse -q --verify "refs/tags/$TAG" >/dev/null ||
    { echo "no tag $TAG in $OGLES2_REPO: git -C $OGLES2_REPO fetch --tags --force"; exit 1; }
rm -rf "$OUT"
mkdir -p "$OUT/dist"
git -C "$OGLES2_REPO" show "$TAG:release/dist/SHA256SUMS" > "$OUT/dist/SHA256SUMS"
for a in "ogles2-$OGLES2.lha" "ogles2-$OGLES2-sdk.lha"; do
    git -C "$OGLES2_REPO" show "$TAG:release/dist/$a" > "$OUT/dist/$a"
done
(cd "$OUT/dist" && sha256sum -c --ignore-missing SHA256SUMS)
(cd "$OUT" && "$LHA" xq "dist/ogles2-$OGLES2-sdk.lha")
echo "$OGLES2 $(git -C "$OGLES2_REPO" rev-parse --short "$TAG")" > "$OUT/VERSION"
cat "$OUT/VERSION"
strings "$OUT/ogles2/Libs/ogles2.library" | grep -m1 '\$VER'
