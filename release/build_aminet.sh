#!/bin/bash
# The release's archive with its source, for Aminet and GitHub alike. Run in
# WSL after build_release.sh (the drawer) and with the release's tag
# v<VERSION> made, the d3es clone as build.sh left it (patched):
#   bash release/build_aminet.sh        (into $OUT/aminet and $OUT/pub)
#   bash release/build_aminet.sh dist   (and the archive, its readme and
#                                        SHA256SUMS into release/dist/)
# Writes into $OUT/aminet/ (default ~/doom3-build/release/aminet):
#   Doom3-Amiga.lha, Doom3-Amiga.readme   game/shoot: the release's drawer
#     with the game's source added, packed as Doom3/source/Doom3-Amiga-src.lha
#     beside a ReadMe.txt (Aminet takes "GNU software" only with its sources
#     or a written offer; one file, not the source's 1,450, to unpack)
# and into $OUT/pub/ the GitHub release's Doom3-Amiga-<VERSION>-src.tar.gz
# (the same source). With dist, release/dist/Doom3-Amiga-<VERSION>.lha is this
# same archive: the one the GitHub release carries (the developer's decision,
# 2026-11-20: one archive everywhere, as the readmes describe it).
# Aminet's names carry no version (its rule: an update is uploaded under the
# same name and replaces the old one). Then checks Aminet's rules
# (wiki.aminet.net/Uploading_instructions and The_Readme_file): names of at
# most 30 characters of letters, digits, '.', '_', '-'; a readme per archive
# with the same base name; Short: first, at most 40 characters; Uploader:,
# Type: and Architecture: present; Distribution: only Aminet or NoCD (left
# out here: either would restrict the GPL's free redistribution); lines of
# at most 78 characters, LF only, plain ASCII; the archives readable.
# Upload: FTP to main.aminet.net, directory new, as anonymous (password:
# the uploader's address), each archive with its readme.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
VERSION=$(tr -d ' \n' < "$HERE/VERSION")
DATE=${DATE:-$(date +%Y-%m-%d)}
SOURCE=${SOURCE:-https://github.com/ladislawv/doom3-amiga}
OUT=${OUT:-$HOME/doom3-build/release}
D3ES_SRC=${D3ES_SRC:-$HOME/d3es}
LHA=${LHA:-$HOME/tools/bin/lha}
A=$OUT/aminet
P=$OUT/pub
N=Doom3-Amiga-$VERSION-src
DIST=0
for a in "$@"; do
    case $a in
    dist) DIST=1 ;;
    *) echo "usage: build_aminet.sh [dist]"; exit 1 ;;
    esac
done

[ -f "$OUT/Doom3-Amiga-$VERSION.lha" ] && [ -d "$OUT/Doom3" ] && [ -f "$OUT/Doom3.info" ] ||
    { echo "no $OUT/Doom3-Amiga-$VERSION.lha and its drawer: run build_release.sh first"; exit 1; }
git -C "$REPO" rev-parse -q --verify "refs/tags/v$VERSION" > /dev/null || { echo "no tag v$VERSION"; exit 1; }
[ -d "$D3ES_SRC/neo/sys/amigaos3" ] || { echo "$D3ES_SRC is not as build.sh leaves it (no neo/sys/amigaos3)"; exit 1; }

rm -rf "$A" "$P"
mkdir -p "$A" "$P/$N/d3es" "$P/$N/port"

# the source: the game's tree as the release's build left it, the port at the tag
(cd "$D3ES_SRC" && git ls-files -co --exclude-standard -z | tar --null -T - -cf - | tar -xf - -C "$P/$N/d3es")
git -C "$REPO" archive "v$VERSION" | tar -xf - -C "$P/$N/port"
rm -rf "$P/$N/port/tests/a1200/results" "$P/$N/port/release/dist"
D3ES_COMMIT=$(grep -o 'D3ES_COMMIT=[0-9a-f]*' "$REPO/sources.sh" | head -1 | cut -d= -f2)
LASTPATCH=$(ls "$REPO/patches" | tail -1 | cut -c1-4)
LASTSDL=$(ls "$REPO/sdl2/patches" | tail -1 | cut -c1-4)
cat > "$P/$N/SOURCE.txt" <<TXT
Doom 3 for AmigaOS 3 on the PiStorm, $VERSION, by Aleksandar Vojnic
(R-Typer) and Ladislav Vojnic (ladislawv):
the complete source of the release's game (dhewm3), GPLv3 with id
Software's additional terms (d3es/COPYING.txt).

d3es/   the game: d3es-multithread (github.com/emileb/d3es-multithread) at
        commit $D3ES_COMMIT, with the port's
        patches 0001-$LASTPATCH (port/patches/) applied, and the AmigaOS system
        layer (port/src/sys/amigaos3) in neo/sys/amigaos3, as build.sh puts it
port/   the port's repository at tag v$VERSION ($SOURCE):
        the patches, the port's own code (src/: the AmigaOS system layer,
        the OpenAL subset, the libnix additions, the port's game data that
        tools/make_amiga_pk4.py packs into amiga.pk4), SDL2_68k's patches
        (sdl2/patches, 0001-$LASTSDL), the build scripts (sources.sh,
        build_deps.sh, build_sdl2.sh, build_al.sh, build.sh), the release's
        files (release/), the launcher (tests/a1200/play/); without the
        test results

Building: see port/CLAUDE.md ("Build machine") and port/PLAN.md section 4.
The libraries used: SDL2_68k (zlib licence), ogles2.library (its SDK:
port/tools/get_ogles2.sh), zlib, dlmalloc, libnix. Doom 3's game data is
not included.
TXT
(cd "$P" && tar --sort=name --owner=0 --group=0 -czf "$N.tar.gz" "$N")

# the Aminet archive: the release's drawer and the source in its source/
S=$OUT/aminet-work
rm -rf "$S"
mkdir -p "$S/Doom3/source"
cp -a "$OUT/Doom3/." "$S/Doom3/"
cp "$OUT/Doom3.info" "$S/"
(cd "$P" && "$LHA" -ao5q "$S/Doom3/source/Doom3-Amiga-src.lha" "$N")
rm -rf "$P/$N"
cat > "$S/Doom3/source/ReadMe.txt" <<TXT
The complete source code of this version of the game (dhewm3), as its
licence, the GNU General Public License version 3 with id Software's
additional terms (../licences/Doom3-GPL.txt), asks.

Doom3-Amiga-src.lha unpacks into the drawer $N:
its SOURCE.txt says what is where and how to build it. Not needed to play:
this drawer can be deleted.

The same source, and its history: $SOURCE
TXT
(cd "$S" && "$LHA" -ao5q "$A/Doom3-Amiga.lha" Doom3 Doom3.info)
rm -rf "$S"

# the readme filled in
fill() { sed -e "s|@VERSION@|$VERSION|g" -e "s|@DATE@|$DATE|g" -e "s|@SOURCE@|$SOURCE|g" "$1"; }
fill "$HERE/Doom3-Amiga.readme" > "$A/Doom3-Amiga.readme"

# Aminet's rules
bad=0
fail() { echo "AMINET: $*"; bad=1; }
for f in "$A"/*; do
    b=$(basename "$f")
    [ ${#b} -le 30 ] || fail "$b: name longer than 30 characters"
    [[ $b =~ ^[A-Za-z0-9._-]+$ ]] || fail "$b: characters other than letters, digits, . _ -"
done
for arc in "$A"/*.lha; do
    r=${arc%.lha}.readme
    [ -f "$r" ] || fail "$(basename "$arc") has no $(basename "$r")"
    "$LHA" -tq "$arc" > /dev/null || fail "$(basename "$arc"): lha test failed"
done
for r in "$A"/*.readme; do
    b=$(basename "$r")
    [ -f "${r%.readme}.lha" ] || fail "$b has no archive"
    head -1 "$r" | grep -q '^Short:' || fail "$b: Short: is not the first line"
    s=$(sed -n '1s/^Short:[[:space:]]*//p' "$r")
    [ ${#s} -ge 1 ] && [ ${#s} -le 40 ] || fail "$b: Short: is ${#s} characters (1-40)"
    hdr=$(sed '/^$/q' "$r")
    for k in Uploader Type Architecture; do
        echo "$hdr" | grep -q "^$k:" || fail "$b: no $k:"
    done
    d=$(echo "$hdr" | sed -n 's/^Distribution:[[:space:]]*//p')
    [ -z "$d" ] || [ "$d" = Aminet ] || [ "$d" = NoCD ] || fail "$b: Distribution: $d (only Aminet or NoCD)"
    grep -q $'\r' "$r" && fail "$b: CR line endings (LF only)"
    grep -q $'\t' "$r" && fail "$b: tabs"
    LC_ALL=C grep -q '[^ -~]' "$r" && fail "$b: characters outside plain ASCII"
    grep -q '@[A-Z]*@' "$r" && fail "$b: a placeholder left"
    awk -v b="$b" 'length > 78 { printf "AMINET: %s line %d is %d characters (78 at most)\n", b, NR, length; e=1 } END { exit e }' "$r" || bad=1
done
[ $bad = 0 ] || { echo "not ready for Aminet"; exit 1; }

if [ $DIST = 1 ]; then
    mkdir -p "$HERE/dist"
    rm -f "$HERE"/dist/Doom3-Amiga-*.lha "$HERE"/dist/Doom3-Amiga-*.readme
    cp "$A/Doom3-Amiga.lha" "$HERE/dist/Doom3-Amiga-$VERSION.lha"
    cp "$A/Doom3-Amiga.readme" "$HERE/dist/Doom3-Amiga-$VERSION.readme"
    (cd "$HERE/dist" && sha256sum Doom3-Amiga-$VERSION.lha > SHA256SUMS)
    echo "copied to $HERE/dist"
fi
ls -l "$A" "$P"
echo "Aminet: upload each .lha with its .readme to ftp://main.aminet.net/new"
