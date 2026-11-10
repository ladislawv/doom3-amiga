#!/usr/bin/env python3
"""Inventory of a game folder's pk4s for the first start's preparation (PLAN.md §16, step 0).

    python3 tools/prep/inventory.py ~/doom3-data/base [--limit 60]

Per type: files, bytes compressed and inflated. Ogg sounds: length, rate and
channels from their headers, the PCM bytes of those below 6 s, the limit, and all.
TGAs whose plain image has a DDS (dds/<name>.dds). Later pk4s override earlier
ones, as in the game. Ends with the prepared set's size: what is read at a load
stored, the TGAs a DDS covers copied compressed, the Oggs below the limit as PCM.
"""
import argparse
import os
import struct
import sys
import zipfile
from collections import defaultdict


def ogg_info(data):
    """(rate, channels, samples per channel) of an Ogg Vorbis file, or None."""
    i = data.find(b"\x01vorbis")
    if i < 0 or i + 16 > len(data):
        return None
    channels = data[i + 11]
    rate = struct.unpack_from("<I", data, i + 12)[0]
    # the last page's granule position is the stream's length in samples
    j = data.rfind(b"OggS")
    if j < 0 or j + 14 > len(data):
        return None
    granule = struct.unpack_from("<q", data, j + 6)[0]
    if granule < 0 or rate == 0:
        return None
    return rate, channels, granule


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("--limit", type=int, default=60)
    a = ap.parse_args()

    paks = sorted(f for f in os.listdir(a.folder) if f.lower().endswith(".pk4"))
    files = {}  # name (lower) -> (pak, ZipInfo); later paks override
    for p in paks:
        z = zipfile.ZipFile(os.path.join(a.folder, p))
        for zi in z.infolist():
            if zi.is_dir():
                continue
            files[zi.filename.lower()] = (p, zi)

    by_type = defaultdict(lambda: [0, 0, 0])
    for name, (p, zi) in files.items():
        ext = os.path.splitext(name)[1] or "(none)"
        t = by_type[ext]
        t[0] += 1
        t[1] += zi.compress_size
        t[2] += zi.file_size

    print(f"{a.folder}: {len(paks)} pk4s, {len(files)} files")
    print(f"{'type':10} {'files':>7} {'compressed MB':>14} {'inflated MB':>12}")
    for ext, (n, c, u) in sorted(by_type.items(), key=lambda kv: -kv[1][1]):
        print(f"{ext:10} {n:7} {c/1e6:14.1f} {u/1e6:12.1f}")

    # sounds
    zips = {p: zipfile.ZipFile(os.path.join(a.folder, p)) for p in paks}
    oggs = [(n, p, zi) for n, (p, zi) in files.items() if n.endswith(".ogg")]
    pcm = {6: [0, 0, 0], a.limit: [0, 0, 0], "all": [0, 0, 0]}  # count, ogg bytes, pcm bytes
    rates = defaultdict(int)
    bad = 0
    for n, p, zi in oggs:
        info = ogg_info(zips[p].read(zi))
        if not info:
            bad += 1
            continue
        rate, ch, samples = info
        rates[(rate, ch)] += 1
        secs = samples / rate
        size = samples * ch * 2
        for k in pcm:
            # idSoundSample::Load decodes when objectSize < rate * limit (objectSize: samples * channels)
            if k == "all" or samples * ch < rate * k:
                pcm[k][0] += 1
                pcm[k][1] += zi.compress_size
                pcm[k][2] += size
    print(f"\nOgg: {len(oggs)} files ({bad} unreadable); rate, channels: "
          + ", ".join(f"{r} Hz x{c}: {n}" for (r, c), n in sorted(rates.items())))
    for k, (n, ob, pb) in pcm.items():
        label = "all" if k == "all" else f"below {k} s"
        print(f"  {label:12} {n:6} files, Ogg {ob/1e6:8.1f} MB -> PCM {pb/1e6:8.1f} MB")

    # TGAs covered by a DDS
    tga = [(n, zi) for n, (p, zi) in files.items() if n.endswith(".tga")]
    covered = [(n, zi) for n, zi in tga if "dds/" + n[:-4] + ".dds" in files]
    print(f"\nTGA: {len(tga)} files, {len(covered)} with a DDS of the same name "
          f"({sum(zi.compress_size for _, zi in covered)/1e6:.1f} MB compressed, "
          f"{sum(zi.file_size for _, zi in covered)/1e6:.1f} MB inflated)")

    # the prepared set
    covered_names = {n for n, _ in covered}
    lim = pcm[a.limit]
    stored = raw = 0
    for n, (p, zi) in files.items():
        if n in covered_names:
            raw += zi.compress_size
        elif n.endswith(".ogg"):
            pass
        else:
            stored += zi.file_size
    ogg_kept = sum(zi.compress_size for n, p, zi in oggs) - lim[1]
    total = stored + raw + lim[2] + ogg_kept
    orig = sum(os.path.getsize(os.path.join(a.folder, p)) for p in paks)
    print(f"\nPrepared set (limit {a.limit} s): stored {stored/1e6:.1f} MB, TGAs compressed "
          f"{raw/1e6:.1f} MB, PCM {lim[2]/1e6:.1f} MB, Ogg kept {ogg_kept/1e6:.1f} MB: "
          f"{total/1e6:.1f} MB (originals {orig/1e6:.1f} MB)")
    inflate = sum(zi.file_size for n, (p, zi) in files.items()
                  if n not in covered_names and not n.endswith(".ogg"))
    print(f"To inflate at the preparation: {inflate/1e6:.1f} MB; Oggs to decode: {lim[0]}")


if __name__ == "__main__":
    sys.exit(main())
