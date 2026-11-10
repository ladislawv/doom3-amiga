#!/bin/bash
# The sound probes of 2026-10-21 (PLAN.md section 14, the AHI noise), each a
# 440/441 Hz tone, as A1200 packages for tools/a1200.py runpkg:
#   ~/doom3-build/m68k/pkg/tone       sound-probes/tone.c: through SDL's audio
#                                     driver (ahi or paula), in steps of loudness;
#                                     prints how late the callbacks came
#   ~/doom3-build/m68k/pkg/ahiprobe   sound-probes/ahiprobe.c: AHI's audio modes in
#                                     memory, and the tone through AHI's low-level
#                                     API with a given mode ID
#   ~/doom3-build/m68k/pkg/ahistream  sound-probes/ahistream.c: the tone through
#                                     AHI's device interface (CMD_WRITE, double
#                                     buffered), no SDL, with SDL's driver's ways
#                                     as switches
#   bash tests/a1200/build_sound_probes.sh
# Each package's Run script plays the default case; other arguments are in the
# sources' comments. Needs build_sdl2.sh's and build_deps.sh's output.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
TC16=${TC16:-$HOME/amiga-gcc162-install}
OGLES2_SDK=${OGLES2_SDK:-$HOME/ogles2pi3-sdk/ogles2/SDK}	# tools/get_ogles2.sh
M=${M:-$HOME/doom3-build/m68k}
SDL=$M/sdl2
CC="$TC16/bin/m68k-amigaos-gcc"
FLAGS="-O2 -mcpu=68020 -m68881 -noixemul -Wall -Wno-pointer-sign"

P=$M/pkg/tone
rm -rf $P; mkdir -p $P
"$CC" $FLAGS -I"$SDL/inc/SDL2" -I"$SDL/inc" "$HERE/sound-probes/tone.c" -o $P/tone \
    -Wl,-u,___xpg_strerror_r -L"$M/deps/lib" -ld3amiga -L"$SDL" -lSDL2 -L"$OGLES2_SDK/lib" -logles2 \
    -L"$SDL/pistorm3d-sdk/lib" -lminigl -ldebug -lm
cat > $P/Run-Tone <<'EOF'
.key DRV/K
.def DRV "ahi"
.bra {
.ket }
MakeDir results >NIL:
Stack 200000
tone {DRV} 1024 >results/tone-{DRV}.txt
Type results/tone-{DRV}.txt
EOF

P=$M/pkg/ahiprobe
rm -rf $P; mkdir -p $P
"$CC" $FLAGS -I"$SDL/inc" "$HERE/sound-probes/ahiprobe.c" -o $P/ahiprobe -lm
cat > $P/Run-Probe <<'EOF'
.key AMP/K,IDS/K
.def AMP 8000
.def IDS "0x2000d"
.bra {
.ket }
MakeDir results >NIL:
ahiprobe {AMP} {IDS} >results/ahiprobe.txt
Type results/ahiprobe.txt
EOF

P=$M/pkg/ahistream
rm -rf $P; mkdir -p $P
"$CC" $FLAGS -I"$SDL/inc" "$HERE/sound-probes/ahistream.c" -o $P/ahistream -lm
cat > $P/Run-Stream <<'EOF'
.key UNIT/K
.def UNIT 0
.bra {
.ket }
MakeDir results >NIL:
ahistream {UNIT} 8000 1024 >results/stream.txt
Type results/stream.txt
EOF

ls -l $M/pkg/tone $M/pkg/ahiprobe $M/pkg/ahistream
