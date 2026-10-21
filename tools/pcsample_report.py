#!/usr/bin/env python3
"""The profile from a PC sampler's file (tests/a1200/pc-sample, PLAN.md section 14).

    python3 tools/pcsample_report.py RESULTS/<build>-pc.txt [BINARY|-] [TOP]

BINARY is the executable that ran (with its symbols, as the packages carry
it). Without it (or with "-"), the one named by MD5 in the results'
build-ids.txt, kept in ~/doom3-build/pcsample-bin/ by build_pc_sample.sh
bench: a binary rebuilt since the run would give wrong names. Run in WSL: it
calls the toolchain's nm. Prints the samples by module (the game, each
library), by engine area, the game's functions with the most samples, an
approximate inclusive profile and the callers of collision and ogles2 (from
the stacks), and where the waiting samples wait. A function's {bytes} is the
span to the next symbol: the toolchain keeps no symbols for static
functions, which are counted with the global one before them.
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


# the game's functions by engine area: the first pattern found in the name wins
AREAS = [
    ("collision", ("idCollisionModel", "idClip", "idPush::", "idTraceModel", "cm_")),
    ("physics", ("idPhysics", "idAF", "idLCP", "idIK", "RunPhysics")),
    ("animation and skinning", ("idAnim", "idMD5", "idRenderModelMD5", "TransformJoints", "TransformVerts",
                                "ConvertJoint", "MultiplyJoints", "BlendJoints", "UpdateAnimation")),
    ("renderer back end", ("RB_", "GL_", "idImage::Bind", "idImage::IsBoundOn", "idVertexCache::Position",
                           "myGlMultMatrix", "agl", "gl")),
    ("renderer front end", ("R_", "idInteraction", "idRenderWorld", "idRenderEntity", "idRenderLight",
                            "idFrustum", "idWinding", "idScreenRect", "idRenderModel", "idSIMD_Generic",
                            "idMaterial", "idVertexCache", "idGuiModel", "idBox", "idBounds")),
    ("scripts", ("idInterpreter", "idThread", "idProgram", "idScript", "idEvent")),
    ("guis", ("idWindow", "idUserInterface", "idWinVar", "idWin", "idGui", "idListGUI", "idRegister",
              "idSimpleWindow", "idDeviceContext", "vfscanf", "sscanf", "atof", "strtod")),
    ("sound", ("idSound", "Snd")),
    ("C library", ("memset", "memcpy", "memmove", "qsort", "strcmp", "strlen", "malloc", "free", "Mem_",
                   "idHeap", "operator new", "operator delete")),
    ("game logic", ("idEntity", "idActor", "idAI", "idPlayer", "idGameLocal", "idMover", "idLight", "idWeapon",
                    "idItem", "idAnimated", "idMoveable", "idProjectile", "idBrittle", "idEntityFx", "idSmoke")),
]


def area(fn):
    for name, pats in AREAS:
        if any(p in fn for p in pats):
            return name
    return "other"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 60
    binary = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != "-" else None
    if binary is None:
        build = open(sys.argv[1]).readline().split()[2]
        ids = os.path.join(os.path.dirname(sys.argv[1]), "build-ids.txt")
        for line in open(ids):
            name, md5 = line.split()
            if name.lower() == build.lower():
                binary = os.path.expanduser("~/doom3-build/pcsample-bin/" + md5)
        if binary is None or not os.path.exists(binary):
            print("no executable for %s in %s" % (build, ids))
            return 1
    segs, libs, pcs, waits, totals = [], [], collections.Counter(), collections.Counter(), ""
    stacks = []
    for line in open(sys.argv[1]):
        f = line.split()
        if not f:
            continue
        if f[0] == "segment":
            segs.append((int(f[2], 16), int(f[3], 16)))
        elif f[0] == "lib":
            low, high = int(f[-2], 16), int(f[-1], 16)
            if low:
                libs.append((" ".join(f[1:-2]), low, high + 0x8000))
        elif f[0] == "samples":
            totals = line.strip()
        elif f[0] == "pc":
            pcs[int(f[1], 16)] += int(f[2])
        elif f[0] == "wait":
            waits[int(f[1], 16)] += int(f[2])
        elif f[0] == "stack":
            stacks.append([int(x, 16) for x in f[1:]])
    addrs, names = symbols(binary)
    code, codeSize = segs[0]

    def where(pc):
        if code <= pc < code + codeSize:
            # only global functions have symbols (the toolchain keeps no static
            # ones): a span much larger than the function holds the static
            # functions that follow it in its file, so it is printed
            i = bisect.bisect_right(addrs, pc - code) - 1
            if i < 0:
                return "game", "?"
            span = (addrs[i + 1] if i + 1 < len(addrs) else codeSize) - addrs[i]
            return "game", "%s {%d bytes}" % (names[i], span)
        for s, (start, size) in enumerate(segs[1:], 1):
            if start <= pc < start + size:
                return "game", "segment %d" % s
        for name, low, high in libs:
            if low <= pc < high:
                return name, name
        # a library loaded from disk: its vectors lead to stubs, its code lies
        # above them (ogles2.library's 200 KB start 10 KB above its stubs)
        below = [(low, name) for name, low, high in libs if low <= pc < low + 0x100000]
        if below:
            name = max(below)[1]
            return name, name + " (above its vectors)"
        return "elsewhere", "%04x...." % (pc >> 16)

    n = sum(pcs.values())
    print(totals)
    print("%d samples with a PC (the program ready to run, stopped by the sampler)\n" % n)
    modules, funcs = collections.Counter(), collections.Counter()
    for pc, c in pcs.items():
        m, fn = where(pc)
        modules[m] += c
        funcs[fn if m == "game" else "[%s]" % fn] += c
    print("by module:")
    for m, c in modules.most_common():
        print("  %6.2f%%  %6d  %s" % (100.0 * c / n, c, m))
    areas = collections.Counter()
    for fn, c in funcs.items():
        if not fn.startswith("["):
            areas[area(fn)] += c
    print("\nthe game's samples by area (by function name; inlined code counts where it was inlined):")
    for a, c in areas.most_common():
        print("  %6.2f%%  %6d  %s" % (100.0 * c / n, c, a))
    print("\nby function (the game's; libraries in brackets), top %d:" % top)
    acc = 0
    for fn, c in funcs.most_common(top):
        acc += c
        print("  %6.2f%%  %6.2f%%  %6d  %s" % (100.0 * c / n, 100.0 * acc / n, c, fn[:150]))
    if stacks:
        # approximate inclusive time: the PC's function and those of the code
        # addresses on the stack (return addresses, and some stale values)
        ns = len(stacks)
        incl = collections.Counter()
        callers = {"collision": collections.Counter(), "ogles2.library": collections.Counter()}

        def fname(a):
            m, fn = where(a)
            return fn if m == "game" else "[%s]" % m

        for st in stacks:
            fns = [fname(a) for a in st]
            for fn in set(fns):
                incl[fn] += 1
            # the first function on the stack outside the PC's area
            m, fn = where(st[0])
            key = "collision" if m == "game" and area(fn) == "collision" else m
            if key in callers:
                for f2 in fns[1:]:
                    # (libogles2.a's stubs, gl* and agl*, count as the library)
                    if (key == "collision" and area(f2) != "collision") or \
                            (key != "collision" and f2 != fns[0] and not f2.startswith(("gl", "agl"))):
                        callers[key][f2] += 1
                        break
                else:
                    callers[key]["(none found)"] += 1
        print("\napproximate inclusive time (the PC's function or a code address on the stack; %d samples), top %d:"
              % (ns, top))
        for fn, c in incl.most_common(top):
            print("  %6.2f%%  %6d  %s" % (100.0 * c / ns, c, fn[:150]))
        for key, cnt in callers.items():
            tot = sum(cnt.values())
            if not tot:
                continue
            print("\n%s (%d samples): the first function outside it on the stack:" % (key, tot))
            for fn, c in cnt.most_common(25):
                print("  %6.2f%%  %6d  %s" % (100.0 * c / tot, c, fn[:150]))
    nw = sum(waits.values())
    if nw:
        print("\nwaiting (%d samples), by the first return address found:" % nw)
        wf = collections.Counter()
        for pc, c in waits.items():
            wf["none found" if pc == 0 else "%s: %s" % where(pc)] += c
        for fn, c in wf.most_common(15):
            print("  %6.2f%%  %6d  %s" % (100.0 * c / nw, c, fn[:150]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
