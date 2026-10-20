#!/bin/bash
# Fetches the sources this port builds from, at pinned commits (PLAN.md §4):
#   ~/d3es      d3es-multithread, the base (reset to the pin; build.sh patches it)
#   ~/d3es-ref  dhewm3 1.5.5, for reference and code taken as patches
#   ~/d3-sdl2   SDL2_68k, the commit ogles2's ports/sdl2 is tested with
# Run in WSL: bash sources.sh. Re-running resets each clone to its pin.
set -e

D3ES_SRC=${D3ES_SRC:-$HOME/d3es}
D3ES_URL=https://github.com/emileb/d3es-multithread.git
D3ES_COMMIT=be1fa3f04eb2618c94ab0d502ab72f9af6267a04   # 2024-04-20

DHEWM3_SRC=${DHEWM3_SRC:-$HOME/d3es-ref}
DHEWM3_URL=https://github.com/dhewm/dhewm3.git
DHEWM3_TAG=1.5.5

SDL2_SRC=${SDL2_SRC:-$HOME/d3-sdl2}
SDL2_URL=https://github.com/SteffenHaeuser/SDL2_68k.git
SDL2_COMMIT=3632f48dc4d085355954322ae452ecc7b007ebc6   # 2026-10-06, SDL 2.33.0

# clone_at DIR URL REV: clone if missing, then check out REV and drop local changes.
clone_at() {
    local dir=$1 url=$2 rev=$3
    if [ ! -d "$dir/.git" ]; then
        git clone -q "$url" "$dir" || { echo "could not clone $url"; exit 1; }
    fi
    if ! git -C "$dir" cat-file -e "$rev^{commit}" 2>/dev/null; then
        git -C "$dir" fetch -q --tags origin
    fi
    git -C "$dir" -c advice.detachedHead=false checkout -q "$rev"
    git -C "$dir" reset -q --hard
    git -C "$dir" clean -qfdx
    echo "$dir: $(git -C "$dir" log -1 --format='%h %cd' --date=short)"
}

clone_at "$D3ES_SRC" "$D3ES_URL" "$D3ES_COMMIT"
clone_at "$DHEWM3_SRC" "$DHEWM3_URL" "$DHEWM3_TAG"
clone_at "$SDL2_SRC" "$SDL2_URL" "$SDL2_COMMIT"
