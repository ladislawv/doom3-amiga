#!/bin/bash
# Builds the PC sampler (PLAN.md section 14) into
# ~/doom3-build/m68k/pkg/pc-sample/ (pc_sample; flight, the flight recorder
# for a freeze of the whole machine; frame_probe and Run-Frame, which
# measured exec's saved frame). With "bench", also puts pc_sample into
# the bench package already built (build_pkg.sh bench ...), whose bench-one
# then samples each build's counted seconds into results/<build>-pc.txt;
# tools/pcsample_report.py turns those into a profile.
#   bash tests/a1200/build_pc_sample.sh [bench]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
TC16=${TC16:-$HOME/amiga-gcc162-install}
M=${M:-$HOME/doom3-build/m68k}
PKG=${PKG:-$M/pkg/pc-sample}
rm -rf "$PKG"
mkdir -p "$PKG"
for p in frame_probe pc_sample flight; do
    "$TC16/bin/m68k-amigaos-gcc" -O2 -mcpu=68020 -m68881 -noixemul -Wall \
        "$HERE/pc-sample/src/$p.c" -o "$PKG/$p"
done
cp "$HERE/pc-sample/Run-Frame" "$PKG/"
if [ "${1:-}" = bench ]; then
    B=$M/pkg/bench
    [ -d "$B" ] || { echo "no $B: build_pkg.sh bench first"; exit 1; }
    cp "$PKG/pc_sample" "$B/"
    # the executables as they ran, for the report (results/build-ids.txt names them by MD5)
    mkdir -p "$M/../pcsample-bin"
    : > "$B/build-ids.txt"
    for f in "$B"/dhewm3-*; do
        case "$f" in *.args) continue ;; esac
        sum=$(md5sum "$f" | cut -d' ' -f1)
        cp -n "$f" "$M/../pcsample-bin/$sum"
        echo "$(basename "$f") $sum" >> "$B/build-ids.txt"
    done
    cat "$B/build-ids.txt"
fi
ls -l "$PKG"
