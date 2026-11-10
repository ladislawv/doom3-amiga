#!/bin/bash
# The A1200 bench package (tests/a1200/bench) with the current game build
# (~/doom3-build/m68k/game) under several names, each with its own console
# commands, for tools/a1200.py runpkg:
#   bash tools/bench_pkg.sh "a:" "b:+set r_useCulling 3"
#   CFG=tests/a1200/play/low.cfg bash tools/bench_pkg.sh "a:+set r_customWidth 1920 +set r_customHeight 1080"
#   python tools/a1200.py runpkg \\wsl$\Ubuntu-24.04\home\l\doom3-build\m68k\pkg\bench -t 900
# Every build gets the measuring settings first: (the tick rate is not set: since
# patch 0034 it always follows the frame rate, 20-60 Hz; fixed 20 Hz before,
# 30 Hz from 2026-10-20 to 2026-10-29), no vsync, and
# a timedemo's demo read into memory at its start (com_preloadDemos 1: streamed
# from the pk4 it cost 3% at 1920x1080, value by value through zlib; the runs
# before 2026-10-29 22:31 streamed it, PLAN.md §14). CFG: a
# settings file every build runs (as bench.cfg). PCSAMPLE=1 adds the PC sampler
# (tests/a1200/build_pc_sample.sh bench). DUMP=1 turns on the OpenAL subset's
# output dump for every build (D3AL_DUMP; give them +set s_noSound 0).
# A build's SDL audio driver: "name@driver:args", e.g. "paula@paula:+set s_noSound 0".
# Another build than game (~/doom3-build/m68k/<build>): "name=build:args", e.g.
# "o3=game-ogles3:" (OGLES=3 build.sh; the package then holds ogles3.library too);
# with a driver "name=build@driver:args".
# Another map: edit .def MAP in the package's Run-Bench (amictl -a takes no "/").
# ogles2.library and v3dglslc come from ogles2's build (~/ogles2-build), copied
# to ~/ogles2-build/direct-libs; ogles3's from its release (build_pkg.sh).
set -e
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
cd "$(dirname "$0")/.."
L=~/ogles2-build/direct-libs
mkdir -p $L
cp ~/ogles2-build/m68k/ogles2.library $L/
cp ~/ogles2-build/v3dglslc/v3dglslc.stripped $L/v3dglslc
# the builds the variants name, besides game
B=()
for v in "$@"; do
    s=${v%%:*}; s=${s%%@*}
    case $s in *=*) b=${s#*=}; [ "$b" = game ] || [[ " ${B[*]} " == *" $b "* ]] || B+=("$b") ;; esac
done
OGLES2_RELEASE=$L bash tests/a1200/build_pkg.sh bench game "${B[@]}" > /dev/null
P=~/doom3-build/m68k/pkg/bench
A='+set r_swapInterval 0 +set com_preloadDemos 1'
mv $P/dhewm3-game $P/dhewm3-a
for v in "$@"; do
    n=${v%%:*}
    args=${v#*:}
    drv=
    case $n in *@*) drv=${n#*@}; n=${n%%@*} ;; esac
    b=
    case $n in *=*) b=${n#*=}; n=${n%%=*} ;; esac
    if [ -n "$b" ] && [ "$b" != game ]; then
        cp $P/dhewm3-$b $P/dhewm3-$n
    else
        [ -f $P/dhewm3-$n ] || cp $P/dhewm3-a $P/dhewm3-$n
    fi
    printf '%s %s' "$A" "$args" > $P/dhewm3-$n.args
    [ -n "$drv" ] && printf '%s' "$drv" > $P/dhewm3-$n.audio
    # a marker file must not be empty: amictl does not upload empty files
    [ -n "${DUMP:-}" ] && echo dump > $P/dhewm3-$n.dump
done
# the builds' own copies out (Run-Bench runs every dhewm3-<name>), unless a variant has its name
for b in "${B[@]}"; do
    [ -f $P/dhewm3-$b.args ] || rm -f $P/dhewm3-$b
done
[ -n "${CFG:-}" ] && cp "$CFG" $P/bench.cfg
[ -n "${PCSAMPLE:-}" ] && bash tests/a1200/build_pc_sample.sh bench > /dev/null
ls $P
for f in $P/*.args; do echo "$f: $(cat $f)"; done
