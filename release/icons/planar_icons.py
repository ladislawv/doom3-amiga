#!/usr/bin/env python3
"""The release icons' planar images, from the port's own art.

    python3 release/icons/planar_icons.py [--preview PNG]

release/src/mkicons.c builds the icons on the Amiga from the system's default
icons: their colour images (GlowIcons, the FORM ICON chunk) are this port's art,
but the planar image older Workbenches show (and the only one in the
DiskObject's gadget) stayed the system's default icon's, which is Workbench
material and may not be distributed (Aminet's rule). This replaces it in
Doom3-tool.info, ReadMe.info and Doom3-drawer.info with the art of
art/game.d3ic, art/readme.d3ic and art/drawer.d3ic: both images (normal and
selected, so the gadget highlights with its selected image), at the art's
size (64x64), two bitplanes in the standard Workbench colours (0 grey, the
background and the transparent colour; 1 black; 2 white; 3 blue): the art's
brightness in three levels with a black outline (to_wb).
Everything else in the file (Tool Types, default tool, stack, drawer data,
the FORM ICON chunk) is copied unchanged. Run again on its own output, it
gives the same bytes.

Needs Pillow (python3-pil).
"""
import os
import struct
import sys

from PIL import Image, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))
ICONS = [("Doom3-tool.info", "game"), ("ReadMe.info", "readme"), ("Doom3-drawer.info", "drawer")]
# the Workbench's default four colours (AmigaOS 2 and 3)
WB = [(0xAA, 0xAA, 0xAA), (0x00, 0x00, 0x00), (0xFF, 0xFF, 0xFF), (0x66, 0x88, 0xBB)]
GFLG_GADGHIGHBITS = 0x0003
GFLG_GADGHIMAGE = 0x0002


def read_d3ic(path):
    b = open(path, "rb").read()
    assert b[:4] == b"D3IC", path
    w, h = struct.unpack(">HH", b[4:8])
    o = 8
    images = []
    for _ in range(2):
        n, = struct.unpack(">H", b[o:o + 2])
        transparent = b[o + 2]
        palette = b[o + 3:o + 3 + 3 * n]
        o += 3 + 3 * n
        pixels = b[o:o + w * h]
        o += w * h
        rgb = Image.new("RGB", (w, h))
        rgb.putdata([tuple(palette[3 * p:3 * p + 3]) for p in pixels])
        mask = [p != transparent for p in pixels]
        images.append((rgb, mask))
    return w, h, images


def to_wb(rgb, mask, dark=60, light=150):
    """The image in the four Workbench colours, as index rows: the art's
    brightness (contrast stretched) in three levels, black, blue and white,
    a black outline round it, the transparent area 0 (the background). The
    art's colours dithered into these four were unreadable; the levels keep
    its shapes and lettering."""
    w, h = rgb.size
    grey = list(ImageOps.autocontrast(rgb.convert("L"), cutoff=2).getdata())
    idx = [0 if not mask[i] else 1 if v < dark else 3 if v < light else 2
           for i, v in enumerate(grey)]
    out = idx[:]
    for y in range(h):
        for x in range(w):
            if not mask[y * w + x]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                xx, yy = x + dx, y + dy
                if not (0 <= xx < w and 0 <= yy < h) or not mask[yy * w + xx]:
                    out[y * w + x] = 1
                    break
    return out


def planar(idx, w, h, depth=2):
    """An Image's data: each plane's rows, padded to 16 bits, plane 0 first."""
    rowbytes = (w + 15) // 16 * 2
    out = bytearray()
    for plane in range(depth):
        for y in range(h):
            row = bytearray(rowbytes)
            for x in range(w):
                if idx[y * w + x] >> plane & 1:
                    row[x >> 3] |= 0x80 >> (x & 7)
            out += row
    return bytes(out)


def image_struct(w, h, depth, data):
    # LeftEdge, TopEdge, Width, Height, Depth, ImageData (non-zero: data
    # follows), PlanePick, PlaneOnOff, NextImage
    return struct.pack(">hhhhhIBBI", 0, 0, w, h, depth, 1, (1 << depth) - 1, 0, 0) + data


def skip_image(b, o):
    w, h, d = struct.unpack(">hhh", b[o + 4:o + 10])
    return o + 20 + d * h * ((w + 15) // 16) * 2


def rewrite(info_path, d3ic_path):
    b = open(info_path, "rb").read()
    magic, version = struct.unpack(">HH", b[0:4])
    assert magic == 0xE310, info_path
    (nxt, le, te, gw, gh, flags, act, gtype, grender, srender, gtext, mex, spec, gid,
     udata) = struct.unpack(">IhhhhHHHIIIIIHI", b[4:48])
    drawer = struct.unpack(">I", b[66:70])[0]
    o = 78 + (56 if drawer else 0)
    head_end = o
    if grender:
        o = skip_image(b, o)
    if srender:
        o = skip_image(b, o)
    tail = b[o:]

    w, h, images = read_d3ic(d3ic_path)
    imgs = [to_wb(rgb, mask) for rgb, mask in images]
    flags = (flags & ~GFLG_GADGHIGHBITS) | GFLG_GADGHIMAGE
    gadget = struct.pack(">IhhhhHHHIIIIIHI", nxt, le, te, w, h, flags, act, gtype,
                         grender or 1, srender or 1, gtext, mex, spec, gid, udata)
    out = (b[0:4] + gadget + b[48:head_end]
           + image_struct(w, h, 2, planar(imgs[0], w, h))
           + image_struct(w, h, 2, planar(imgs[1], w, h))
           + tail)
    open(info_path, "wb").write(out)
    return w, h, imgs


def main():
    previews = []
    for info, art in ICONS:
        w, h, imgs = rewrite(os.path.join(HERE, info), os.path.join(HERE, "art", art + ".d3ic"))
        print("%s: planar %dx%d, 2 images, from art/%s.d3ic" % (info, w, h, art))
        for idx in imgs:
            im = Image.new("RGB", (w, h))
            im.putdata([WB[i] for i in idx])
            previews.append(im)
    if "--preview" in sys.argv:
        path = sys.argv[sys.argv.index("--preview") + 1]
        w, h = previews[0].size
        sheet = Image.new("RGB", ((w + 8) * len(previews), h + 8), WB[0])
        for i, im in enumerate(previews):
            sheet.paste(im, (4 + i * (w + 8), 4))
        sheet.resize((sheet.width * 4, sheet.height * 4), Image.NEAREST).save(path)


if __name__ == "__main__":
    main()
