#!/bin/bash
# The host build (PLAN.md §8, phase 1): d3es for x86-64 Linux in WSL, with the
# port's options and patches, drawn by Mesa's llvmpipe under xvfb-run.
#   bash build_host.sh            reset ~/d3es, apply patches/, build
#   NOPATCH=1 bash build_host.sh  the same without patches (upstream as pinned)
# Output: ~/doom3-build/host/dhewm3. Run it with run_host.sh.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
D3ES_SRC=${D3ES_SRC:-$HOME/d3es}
OUT=${OUT:-$HOME/doom3-build/host}
[ -n "$NOPATCH" ] && OUT=${OUT}-upstream

[ -d "$D3ES_SRC/.git" ] || { echo "no $D3ES_SRC: run sources.sh first"; exit 1; }
mkdir -p "$(dirname "$OUT")"
for p in sdl2 openal; do
    pkg-config --exists $p || { echo "missing $p development files: sudo apt install libsdl2-dev libopenal-dev"; exit 1; }
done

# Reset the clone to the pinned commit and apply the port's patches in order.
git -C "$D3ES_SRC" reset -q --hard
git -C "$D3ES_SRC" clean -qfdx
if [ -z "$NOPATCH" ]; then
    for p in "$HERE"/patches/*.patch; do
        [ -e "$p" ] || continue
        git -C "$D3ES_SRC" apply "$p" || { echo "$(basename "$p") does not apply"; exit 1; }
        echo "applied $(basename "$p")"
    done
fi
# Staged, so that edits made in the clone afterwards show alone in `git diff`
# (that is how the next patch is made).
git -C "$D3ES_SRC" add -A

# HARDLINK_GAME with BASE: the game code in the executable (D5); no dedicated
# server, no tools. RelWithDebInfo, as d3es's default. SINGLE_TASK (patch
# 0001, D4) only with the patches.
SINGLE=-DSINGLE_TASK=ON
[ -n "$NOPATCH" ] && SINGLE=
# No curl (multiplayer downloads only). WSL 1 passes the Windows PATH through,
# and CMake's package search follows it into Windows folders (it found a
# vcpkg curl in /mnt/c): the build runs with the Windows entries removed.
PATH=$(printf '%s' "$PATH" | tr ':' '\n' | grep -v '^/mnt/' | paste -sd:)
export PATH
rm -rf "$OUT"
cmake -S "$D3ES_SRC/neo" -B "$OUT" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_DISABLE_FIND_PACKAGE_CURL=ON \
    -DHARDLINK_GAME=ON -DBASE=ON -DD3XP=OFF -DDEDICATED=OFF -DCORE=ON \
    -DSDL2=ON $SINGLE >"$OUT.cmake.log" 2>&1 || { tail -30 "$OUT.cmake.log"; exit 1; }
ninja -C "$OUT" >"$OUT.build.log" 2>&1 || { grep -m20 -B2 -A6 "error" "$OUT.build.log"; exit 1; }
ls -l "$OUT/dhewm3"
