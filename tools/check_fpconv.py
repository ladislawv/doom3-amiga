#!/usr/bin/env python3
"""Checks a disassembly of the 68k game for float-to-integer conversions
without truncation (PLAN.md §5): with -m68020 -m68881, GCC converts with
`fintrz %fpN,%fpN` then `fmove.l %fpN,<dest>`; GCC 6.5's optimiser was seen to
delete the fintrz (ogles2's notes), and then the conversion rounds instead.
    m68k-amigaos-objdump -d dhewm3 > dhewm3.dis
    tools/check_fpconv.py dhewm3.dis
Prints the counts and the first cases without fintrz; exit status 1 if there
are any (some may be deliberate rounding: look at them)."""
import collections
import re
import sys

STORE = re.compile(r"^fmovel\s+%fp(\d),(\S+)$")
FPDEST = re.compile(r"^(f\w+)\s+.*,%fp(\d)$")      # any FPU instruction writing %fpN
FUNC = re.compile(r"^[0-9a-f]+ <.*>:$")             # a function starts


def main(path):
    """Per register, whether its last write was a truncation (fintrz, from a
    register or from memory); the state is forgotten at each function."""
    stats = collections.Counter()
    cases = []
    funcs = collections.Counter()
    truncated = {}
    last = {}
    func = "?"
    for line in open(path, errors="replace"):
        line = line.rstrip("\n")
        if FUNC.match(line):
            truncated, last = {}, {}
            func = line.split("<", 1)[1].rstrip(">:")
            continue
        parts = line.split("\t")
        if len(parts) < 3:
            continue
        ins = parts[-1].strip()
        m = STORE.match(ins)
        if m and not m.group(2).startswith("%fp"):
            reg = m.group(1)
            if truncated.get(reg):
                stats["with fintrz"] += 1
            else:
                stats["without fintrz"] += 1
                funcs[func] += 1
                if len(cases) < 10:
                    cases.append((parts[0].strip().rstrip(":"), last.get(reg, "?"), ins))
            continue
        d = FPDEST.match(ins)
        if d:
            truncated[d.group(2)] = d.group(1).startswith("fintrz")
            last[d.group(2)] = ins
    print("float-to-integer stores: %d with fintrz, %d without" %
          (stats["with fintrz"], stats["without fintrz"]))
    for addr, p, i in cases:
        print("  %s: %s  (the register's last write: %s)" % (addr, i, p))
    print("functions with them (count, name):")
    for name, n in funcs.most_common(40):
        print("  %4d  %s" % (n, name))
    return 1 if stats["without fintrz"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
