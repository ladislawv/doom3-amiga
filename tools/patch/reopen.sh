#!/bin/bash
# Opens one of patches/ for editing in the WSL clone (~/d3es): resets the clone,
# applies and stages every patch before NNNN, then applies NNNN unstaged, so
# that `git diff` shows that patch alone and edits extend it. Write it back with
# export.sh, then check the later patches with checkrest.sh.
#   bash tools/patch/reopen.sh 0016          (the number, or the whole name)
#   bash tools/patch/reopen.sh 9999          (all applied and staged: a new patch's edits show alone)
set -e
HERE=$(cd "$(dirname "$0")/../.." && pwd)
D3ES_SRC=${D3ES_SRC:-$HOME/d3es}
[ -n "${1:-}" ] || { echo "usage: reopen.sh NNNN"; exit 1; }
cd "$D3ES_SRC"
git reset -q --hard
git clean -qfdx
for p in "$HERE/patches/"*.patch; do
    n=${p##*/}
    if [[ "$n" < "$1" ]]; then
        git apply "$p"
        git add -A
        echo "staged $n"
    elif [[ "$n" == "$1"* ]]; then
        git apply "$p"
        echo "open   $n"
    fi
done
