#!/bin/bash
# The A1200 map package (tests/a1200/map: loads a map, takes the game's own
# screenshot WAIT frames in) with the current game build and extra console
# commands in its dhewm3.args, for comparing pictures between settings:
#   bash tools/map_pkg.sh "+set r_cullDynamicLightTris 1"
#   python tools/a1200.py runpkg \\wsl$\Ubuntu-24.04\home\l\doom3-build\m68k\pkg\map -a "WAIT=150" -t 600
# The screenshot comes back as results/shot00001.tga; tools/tga2png.py makes a PNG.
set -e
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
cd "$(dirname "$0")/.."
OGLES2_RELEASE=${OGLES2_RELEASE:-$HOME/ogles2-build/direct-libs} bash tests/a1200/build_pkg.sh map > /dev/null
printf '+set r_swapInterval 0 +set sensitivity 0 %s' "${1:-}" > ~/doom3-build/m68k/pkg/map/dhewm3.args
cat ~/doom3-build/m68k/pkg/map/dhewm3.args; echo
