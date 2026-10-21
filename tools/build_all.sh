#!/bin/bash
# Builds the A1200 game (build.sh) and the host game (build_host.sh) from the
# current patches; on an error, the first error lines of its log.
#   bash tools/build_all.sh
# Both reset ~/d3es and apply patches/ (export first what you edited there).
# Logs: ~/doom3-build/game.build.log, ~/doom3-build/host.build.log.
set -e
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
cd "$(dirname "$0")/.."
bash build.sh > ~/doom3-build/game.build.log 2>&1 || { grep -n "error" ~/doom3-build/game.build.log | head -20; exit 1; }
echo "amiga build ok"
bash build_host.sh > ~/doom3-build/host.build.log 2>&1 || { grep -n "error" ~/doom3-build/host.build.log | head -20; exit 1; }
echo "host build ok"
