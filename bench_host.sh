#!/bin/bash
# Host benchmark (PLAN.md §8): frames rendered in SECS seconds (default 30)
# after MAP has loaded (default game/demo_mars_city1, whose scripted opening
# runs the same everywhere), with precompressed textures. Uses patch 0007's
# com_benchmarkSeconds.
#   bash bench_host.sh [extra +commands]       SECS=60 MAP=game/demo_mc_underground ...
SECS=${SECS:-30}
MAP=${MAP:-game/demo_mars_city1}
TIMEOUT=$((SECS + 180)) bash "$(dirname "$0")/run_host.sh" +set s_noSound 1 +set image_usePrecompressedTextures 1 \
    +set com_benchmarkSeconds "$SECS" "$@" +map "$MAP" > /dev/null 2>&1
rc=$?
grep -h "msec to load\|^benchmark:\|ERROR\|SIGSEGV" "$HOME/doom3-build/host-run/console.log"
exit $rc
