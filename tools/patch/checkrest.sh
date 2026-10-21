#!/bin/bash
# After export.sh NNNN: applies (and stages) the patches after NNNN, saying
# whether each still applies. A failure means that patch must be reopened and
# made again on top of the new NNNN.
#   bash tools/patch/checkrest.sh 0016
set -e
HERE=$(cd "$(dirname "$0")/../.." && pwd)
D3ES_SRC=${D3ES_SRC:-$HOME/d3es}
[ -n "${1:-}" ] || { echo "usage: checkrest.sh NNNN"; exit 1; }
cd "$D3ES_SRC"
for p in "$HERE/patches/"*.patch; do
    n=${p##*/}
    if [[ "$n" > "$1" ]] && [[ "$n" != "$1"* ]]; then
        git apply --check "$p" && git apply "$p" && git add -A && echo "still applies: $n"
    fi
done
