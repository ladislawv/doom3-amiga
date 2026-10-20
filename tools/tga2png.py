#!/usr/bin/env python3
"""Converts Doom 3's screenshots (uncompressed or RLE 24/32-bit TGA) to PNG,
with only the standard library.   tga2png.py IN.tga [OUT.png]"""
import struct
import sys
import zlib


def read_tga(path):
    data = open(path, "rb").read()
    idlen, cmaptype, itype = data[0], data[1], data[2]
    w, h, bpp, desc = struct.unpack_from("<HHBB", data, 12)
    if cmaptype or itype not in (2, 10) or bpp not in (24, 32):
        raise SystemExit("%s: only true-colour 24/32-bit TGA (type %d, %d bits)" % (path, itype, bpp))
    n = bpp // 8
    pos = 18 + idlen
    if itype == 2 and bpp == 24 and len(data) == pos + w * h * 4:
        n = 4  # unpatched d3es: RGBA data under a 24-bit header (fixed by patch 0003)
    pixels = bytearray()
    if itype == 2:
        pixels = data[pos:pos + w * h * n]
    else:  # run-length encoded
        while len(pixels) < w * h * n:
            c = data[pos]
            pos += 1
            count = (c & 0x7F) + 1
            if c & 0x80:
                pixels += data[pos:pos + n] * count
                pos += n
            else:
                pixels += data[pos:pos + n * count]
                pos += n * count
    rows = [bytes(pixels[y * w * n:(y + 1) * w * n]) for y in range(h)]
    if not desc & 0x20:  # bottom-up
        rows.reverse()
    out = []
    for r in rows:  # BGR(A) to RGB
        rgb = bytearray(w * 3)
        rgb[0::3] = r[2::n]
        rgb[1::3] = r[1::n]
        rgb[2::3] = r[0::n]
        out.append(bytes(rgb))
    return w, h, out


def write_png(path, w, h, rows):
    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)
    raw = b"".join(b"\0" + r for r in rows)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


if __name__ == "__main__":
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else src.rsplit(".", 1)[0] + ".png"
    w, h, rows = read_tga(src)
    write_png(dst, w, h, rows)
    print("%s: %dx%d" % (dst, w, h))
