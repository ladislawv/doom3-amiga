#!/bin/bash
# Writes the clone's unstaged edits (new files included) to patches/NAME.patch,
# then stages them. An existing patches/NAME.patch is kept first in
# ~/doom3-build/patch-backup/NAME.<time>.patch.
#   bash tools/patch/export.sh 0016-texture-bind-cache
# Run it only on a clone prepared by reopen.sh: on any other state the patch
# gets whatever is unstaged (a later patch's lines, or nothing).
set -e
HERE=$(cd "$(dirname "$0")/../.." && pwd)
D3ES_SRC=${D3ES_SRC:-$HOME/d3es}
[ -n "${1:-}" ] || { echo "usage: export.sh NNNN-name"; exit 1; }
cd "$D3ES_SRC"
P="$HERE/patches/$1.patch"
if [ -f "$P" ]; then
    mkdir -p ~/doom3-build/patch-backup
    cp "$P" ~/doom3-build/patch-backup/"$1.$(date +%H%M%S).patch"
fi
# new files into the diff; not neo/sys/amigaos3 (build.sh copies src/sys/amigaos3 there)
git add -A -N -- . ':!neo/sys/amigaos3'
git diff --stat
git diff > "$P"
git add -A -- . ':!neo/sys/amigaos3'
