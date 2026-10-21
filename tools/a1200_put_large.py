#!/usr/bin/env python3
"""Uploads a file of any size to the test station: amictl's PUT takes at most
64 MB, so the file goes up in pieces, which AmigaDOS Join puts together on the
Amiga; the pieces are deleted afterwards and the size is checked.
    python tools/a1200_put_large.py LOCAL REMOTE      (REMOTE: e.g. Stuff:doom3/demo/demo00.pk4)
Uses ogles2's amictl client read only, as tools/a1200.py does."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import a1200  # noqa: E402  (sets up amictl)

amictl = a1200.amictl
PIECE = 48 * 1024 * 1024  # below PUT's 64 MB limit
MAX_JOIN = 15             # AmigaDOS Join takes at most 15 files


def main(local, remote):
    size = os.path.getsize(local)
    st = amictl.Station()
    remote_dir = remote.rsplit("/", 1)[0] if "/" in remote else remote.split(":", 1)[0] + ":"
    st.run('MakeDir "%s" ALL' % remote_dir, "", 60)  # AmigaOS 3.2: ALL makes the parents too
    pieces = []
    with open(local, "rb") as f:
        i = 0
        while True:
            data = f.read(PIECE)
            if not data:
                break
            name = "%s.part%02d" % (remote, i)
            print("piece %d: %d bytes -> %s" % (i, len(data), name), flush=True)
            st.put(name, data)
            pieces.append(name)
            i += 1
    if len(pieces) > MAX_JOIN:
        raise SystemExit("too many pieces for one Join: %d" % len(pieces))
    if len(pieces) == 1:
        rc, out, _ = st.run('Rename "%s" "%s"' % (pieces[0], remote), "", 60)
    else:
        rc, out, _ = st.run("Join %s AS \"%s\"" % (" ".join('"%s"' % p for p in pieces), remote), "", 600)
    print(out.decode("latin-1", "replace"), end="")
    if rc != 0:
        raise SystemExit("Join failed (rc %s); the pieces are left on the Amiga" % rc)
    for p in pieces:
        st.run('Delete "%s" QUIET' % p, "", 60)
    rc, out, _ = st.run('List "%s" LFORMAT "%%L"' % remote, "", 60)
    got = out.decode("latin-1").strip()
    print("on the Amiga: %s bytes, local %d" % (got, size))
    if got != str(size):
        raise SystemExit("size differs")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
