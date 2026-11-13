#!/bin/bash
# The one ogles2 this project uses: ogles2pi3's ogles2.library (the Pi 4 and the
# Pi 3; github.com/ladislawv/ogles2pi3). Into OGLES2_HOME (default ~/ogles2pi3-sdk):
#   dist/ogles2-1.4-beta1.lha, dist/ogles2-1.4-beta1-sdk.lha  the published beta 1
#       archives, from the tag v1.4-beta1 (whatever branch is checked out there),
#       checked against the tag's SHA256SUMS: the shader compiler, the headers,
#       the licences (release/build_release.sh takes them from here)
#   ogles2/Libs/ogles2.library, ogles2/SDK/lib/libogles2.a  the library and its
#       import library built from OGLES2_COMMIT (default c5d7817: the branch
#       ogles2pi3-beta2, "1.4-beta2"); OGLES2_COMMIT= keeps beta 1's
#   ogles2/Libs/v3dglslc, ogles2/SDK/include   beta 1's (unchanged in beta 2)
#   bash tools/get_ogles2.sh
# The library's build: the commit's source (git archive, the checkout untouched)
# in $OGLES2_HOME/src, its make with B=$OGLES2_HOME/build (ogles2pi3's own
# Makefile; GCC 6.5 in ~/amiga-gcc-install, as it needs).
set -eu
HERE=$(cd "$(dirname "$0")/.." && pwd)
OGLES2=${OGLES2:-1.4-beta1}
OGLES2_COMMIT=${OGLES2_COMMIT-c5d7817}
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
V="$OGLES2 $(git -C "$OGLES2_REPO" rev-parse --short "$TAG")"
if [ -n "$OGLES2_COMMIT" ]; then
    git -C "$OGLES2_REPO" cat-file -e "$OGLES2_COMMIT^{commit}" 2>/dev/null ||
        { echo "no commit $OGLES2_COMMIT in $OGLES2_REPO: git -C $OGLES2_REPO fetch origin"; exit 1; }
    mkdir -p "$OUT/src"
    git -C "$OGLES2_REPO" archive "$OGLES2_COMMIT" | tar -x -C "$OUT/src"
    (cd "$OUT/src" && make -j8 B="$OUT/build" "$OUT/build/ogles2.library" "$OUT/build/libogles2.a") > "$OUT/build.log" 2>&1 ||
        { tail -15 "$OUT/build.log"; exit 1; }
    cp "$OUT/build/ogles2.library" "$OUT/ogles2/Libs/ogles2.library"
    cp "$OUT/build/libogles2.a" "$OUT/ogles2/SDK/lib/libogles2.a"
    V="$V; library and libogles2.a from $(git -C "$OGLES2_REPO" rev-parse --short "$OGLES2_COMMIT")"
fi
echo "$V" > "$OUT/VERSION"
cat "$OUT/VERSION"
grep -a -o '\$VER: ogles2.library[^"]*' "$OUT/ogles2/Libs/ogles2.library" | head -1
