#!/bin/bash
# Runs the host build headless (WSL 1 has no display): xvfb-run, Mesa's llvmpipe.
#   bash run_host.sh [+engine commands ...]
# Data from ~/doom3-data (base/, or demo/ which d3es finds by itself); saves,
# logs and screenshots in ~/doom3-build/host-run/save. The console log is
# written to ~/doom3-build/host-run/console.log as well as shown.
#   BIN=...     another executable (default ~/doom3-build/host/dhewm3)
#   TIMEOUT=s   kill after s seconds (default 600)
#   W=, H=      window size (default 640x480)
set -e
BIN=${BIN:-$HOME/doom3-build/host/dhewm3}
DATA=${DATA:-$HOME/doom3-data}
RUN=$HOME/doom3-build/host-run
W=${W:-640}
H=${H:-480}
mkdir -p "$RUN/save"
[ -x "$BIN" ] || { echo "no $BIN: run build_host.sh first"; exit 1; }
command -v xvfb-run >/dev/null || { echo "no xvfb-run: sudo apt install xvfb"; exit 1; }

# llvmpipe for GL ES 2.0 through SDL2's X11 driver; no audio device in WSL.
export LIBGL_ALWAYS_SOFTWARE=1 SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy}
timeout "${TIMEOUT:-600}" xvfb-run -a -s "-screen 0 ${W}x${H}x24" \
    "$BIN" +set fs_basepath "$DATA" +set fs_savepath "$RUN/save" \
    +set r_fullscreen 0 +set r_mode -1 +set r_customWidth "$W" +set r_customHeight "$H" \
    +set logFile 2 "$@" 2>&1 | tee "$RUN/console.log"
