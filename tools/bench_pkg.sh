#!/bin/bash
# The A1200 bench package (tests/a1200/bench) with the current game build
# (~/doom3-build/m68k/game) under several names, each with its own console
# commands, for tools/a1200.py runpkg:
#   bash tools/bench_pkg.sh "a:" "b:+set r_useCulling 3"
#   CFG=tests/a1200/play/low.cfg bash tools/bench_pkg.sh "a:+set r_customWidth 1920 +set r_customHeight 1080"
#   python tools/a1200.py runpkg \\wsl$\Ubuntu-24.04\home\l\doom3-build\m68k\pkg\bench -t 900
# Every build gets the measuring settings first: 30 game tics a second (the
# benchmarks before 2026-10-20 ran so; play uses 20) and no vsync. CFG: a
# settings file every build runs (as bench.cfg). PCSAMPLE=1 adds the PC sampler
# (tests/a1200/build_pc_sample.sh bench). DUMP=1 turns on the OpenAL subset's
# output dump for every build (D3AL_DUMP; give them +set s_noSound 0).
# Another map: edit .def MAP in the package's Run-Bench (amictl -a takes no "/").
# ogles2.library and v3dglslc come from ogles2's build (~/ogles2-build), copied
# to ~/ogles2-build/direct-libs.
set -e
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
cd "$(dirname "$0")/.."
L=~/ogles2-build/direct-libs
mkdir -p $L
cp ~/ogles2-build/m68k/ogles2.library $L/
cp ~/ogles2-build/v3dglslc/v3dglslc.stripped $L/v3dglslc
OGLES2_RELEASE=$L bash tests/a1200/build_pkg.sh bench game > /dev/null
P=~/doom3-build/m68k/pkg/bench
A='+set com_gameHz 30 +set r_swapInterval 0'
mv $P/dhewm3-game $P/dhewm3-a
for v in "$@"; do
    n=${v%%:*}
    args=${v#*:}
    [ -f $P/dhewm3-$n ] || cp $P/dhewm3-a $P/dhewm3-$n
    printf '%s %s' "$A" "$args" > $P/dhewm3-$n.args
    # a marker file must not be empty: amictl does not upload empty files
    [ -n "${DUMP:-}" ] && echo dump > $P/dhewm3-$n.dump
done
[ -n "${CFG:-}" ] && cp "$CFG" $P/bench.cfg
[ -n "${PCSAMPLE:-}" ] && bash tests/a1200/build_pc_sample.sh bench > /dev/null
ls $P
for f in $P/*.args; do echo "$f: $(cat $f)"; done
