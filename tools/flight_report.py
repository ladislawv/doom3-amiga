#!/usr/bin/env python3
"""The flight recorder's file with names (tests/a1200/pc-sample/src/flight.c, PLAN.md section 14).

    python3 tools/flight_report.py flight.txt GAME OGLES2_DEBUG [LAST]

GAME is the executable that ran, OGLES2_DEBUG ogles2.library's file with
symbols (ogles2.library.debug from the same build as the library that ran).
Prints the last LAST records (default 40), each address as function+offset:
the game's from its symbols; ogles2's after finding where the library was
loaded (its vectors' targets matched with the symbols); others by library.
Run in WSL: it calls the toolchain's nm. As in pcsample_report.py, static
functions have no symbols and count with the global one before them.
"""
import bisect
import collections
import os
import subprocess
import sys

NM = os.path.expanduser(os.environ.get("TC16", "~/amiga-gcc162-install")) + "/bin/m68k-amigaos-nm"


def symbols(binary):
    """(sorted addresses, names) of the code symbols, offsets into the first code hunk."""
    out = subprocess.run([NM, "-n", "-C", binary], check=True, capture_output=True, text=True).stdout
    addrs, names = [], []
    for line in out.splitlines():
        parts = line.split(" ", 2)
        if len(parts) == 3 and parts[1] in "tT":
            addrs.append(int(parts[0], 16))
            names.append(parts[2])
    return addrs, names


def lookup(syms, off):
    addrs, names = syms
    i = bisect.bisect_right(addrs, off) - 1
    return "%s+%x" % (names[i], off - addrs[i]) if i >= 0 else "?+%x" % off


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    last = int(sys.argv[4]) if len(sys.argv) > 4 else 40
    segs, libs, vecs, records, tail = [], [], [], [], []
    for line in open(sys.argv[1]):
        f = line.split()
        if not f:
            continue
        if f[0] == "segment":
            segs.append((int(f[2], 16), int(f[3], 16)))
        elif f[0] == "ogvec":
            vecs.append(int(f[2], 16))
        elif f[0] == "lib":
            low, high = int(f[-2], 16), int(f[-1], 16)
            if low:
                libs.append((" ".join(f[1:-2]), low, high + 0x8000))
        elif f[0].isdigit():
            records.append(line.rstrip("\n"))
        elif f[0] in ("flight", "ogles2"):
            print(line.rstrip())
        else:
            tail.append(line.rstrip("\n"))
    game = symbols(sys.argv[2])
    og = symbols(sys.argv[3])
    # the load address: the commonest difference between a vector's target and a symbol
    deltas = collections.Counter(v - a for v in vecs for a in og[0] if v >= a)
    base, hits = deltas.most_common(1)[0] if deltas else (0, 0)
    print("ogles2 loaded at %08x (%d of %d vectors on a symbol)" % (base, hits, len(vecs)))
    code, code_size = segs[0]
    og_end = base + og[0][-1] + 0x10000 if og[0] else 0

    def where(a):
        if code <= a < code + code_size:
            return lookup(game, a - code)
        if base <= a < og_end:
            return "ogles2:" + lookup(og, a - base)
        for name, low, high in libs:
            if low <= a < high:
                return "%s+%x" % (name, a - low)
        return "%08x" % a

    print("%d records; the last %d:" % (len(records), min(last, len(records))))
    for rec in records[-last:]:
        head, _, ready = rec.partition("|")
        f = head.split()
        n, ticks, state, at, sigwait = f[0], int(f[1]), f[2], int(f[3], 16), f[4]
        rets = [int(x, 16) for x in f[6:]]
        print("#%s %02d:%02d:%05.2f %s %s%s" % (n, ticks // 180000, ticks // 3000 % 60, ticks % 3000 / 50.0,
                                               state, where(at) if at else "-",
                                               " sigwait " + sigwait if state == "W" else ""))
        for r in rets:
            print("      " + where(r))
        print("    ready:" + ready)
    for line in tail:
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
