#!/usr/bin/env python3
"""The A1200 test station for this port: ogles2's amictl client, used as it
is (read only, from the ogles2 checkout), with results written into this
repository (tests/a1200/results/<package>-<time>/) instead of ogles2's.
Same commands and arguments as amictl:
    python tools/a1200.py hello | info | run <command> | runpkg <dir> ...
The station's address and token come from ogles2's tools/amictl/station.local
(or AMICTL_HOST / AMICTL_TOKEN), as for amictl itself.
    OGLES2_REPO   the ogles2 checkout (default: ../ogles2 next to this one)
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
OGLES2 = os.environ.get("OGLES2_REPO", os.path.join(os.path.dirname(REPO), "ogles2"))
sys.path.insert(0, os.path.join(OGLES2, "tools", "amictl"))

import amictl  # noqa: E402

amictl.REPO = REPO  # runpkg's results go to this repository's tests/a1200/results

_runpkg = amictl.runpkg


def runpkg_clean(st, pkgdir, *args, **kwargs):
    """runpkg, after deleting the package's directory on the Amiga: amictl
    uploads into it without removing what an earlier package left there (a
    bench run once ran the previous package's builds as well)."""
    name = os.path.basename(os.path.abspath(pkgdir).rstrip("/\\"))
    rc, out, _ = st.run("CD", "", 30)
    workdir = out.decode("latin-1").strip().splitlines()[-1] if rc == 0 and out.strip() else ""
    if ":" in workdir and name:
        st.run('Delete "%s" ALL QUIET FORCE >NIL:' % amictl.join(workdir, name), "", 120)
    return _runpkg(st, pkgdir, *args, **kwargs)


amictl.runpkg = runpkg_clean

if __name__ == "__main__":
    sys.exit(amictl.main(sys.argv[1:]))
