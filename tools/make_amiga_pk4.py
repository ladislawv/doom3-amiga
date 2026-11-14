#!/usr/bin/env python3
"""Builds amiga.pk4, the Amiga port's own game data: the attract screen on the
Mars City arcade cabinet (patch 0037). The text files come from src/data/amiga/;
the pictures and the sound are drawn and synthesised here (nothing taken from
the game's data or any other program):
  guis/amiga/room.tga   the grey room with its purple grid (512x512, shown 640x480)
  guis/amiga/ball.tga   16 frames of the turning red and white ball (2048x128)
  sound/amiga/bounce.wav the bounce (22,050 Hz, 16-bit mono)
The archive is the same bytes on every run (fixed dates, fixed order).
  python3 tools/make_amiga_pk4.py OUT.pk4
Needs Pillow."""
import math, os, struct, sys, zipfile, io, random
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "src", "data", "amiga")
DATE = (2026, 11, 14, 0, 0, 0)

GREY = (170, 170, 170, 255)
PURPLE = (160, 0, 160, 255)


def room():
    """The room in GUI units (640x480), drawn at 4x and scaled to 512x512. The
    cabinet's bezel covers the GUI below about y 388, so the room ends above it."""
    S = 4
    img = Image.new("RGBA", (640 * S, 480 * S), GREY)
    d = ImageDraw.Draw(img)
    w = 2 * S
    left, right, top, wall, floor = 80, 560, 36, 276, 336
    # the back wall: 40-unit squares
    for x in range(left, right + 1, 40):
        d.line([(x * S, top * S), (x * S, wall * S)], fill=PURPLE, width=w)
    for y in range(top, wall + 1, 40):
        d.line([(left * S, y * S), (right * S, y * S)], fill=PURPLE, width=w)
    # the floor: the wall's lines coming towards the viewer, rows closer together far away
    spread = 1.45
    for x in range(left, right + 1, 40):
        x2 = 320 + (x - 320) * spread
        d.line([(x * S, wall * S), (x2 * S, floor * S)], fill=PURPLE, width=w)
    for i in range(1, 5):
        f = (i / 4) ** 1.5
        y = wall + (floor - wall) * f
        half = (right - left) / 2 * (1 + (spread - 1) * f)
        d.line([((320 - half) * S, y * S), ((320 + half) * S, y * S)], fill=PURPLE, width=w)
    return img.resize((512, 512), Image.LANCZOS)


def ball():
    """16 frames of 128x128: the ball tilted 20 degrees, 8 checkers from pole to
    pole and 16 around, turned 45/16 degrees a frame; lit from the upper left."""
    F, N, SS = 128, 16, 3
    R = 58.0
    tilt = math.radians(-20)
    ct, st = math.cos(tilt), math.sin(tilt)
    lx, ly, lz = -0.45, 0.55, 0.70
    ln = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / ln, ly / ln, lz / ln
    red, white = (210, 20, 20), (240, 240, 240)
    strip = Image.new("RGBA", (F * N, F), (0, 0, 0, 0))
    big = F * SS
    for frame in range(N):
        phase = frame / N * (math.pi / 4)
        img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
        px = img.load()
        c = big / 2
        r = R * SS
        for py in range(big):
            y = (c - py - 0.5) / r
            if abs(y) >= 1:
                continue
            for qx in range(big):
                x = (qx + 0.5 - c) / r
                zz = 1 - x * x - y * y
                if zz <= 0:
                    continue
                z = math.sqrt(zz)
                # into the ball's own frame: undo the tilt about the view axis
                bx = x * ct + y * st
                by = -x * st + y * ct
                lat = math.asin(max(-1.0, min(1.0, by)))
                lon = math.atan2(bx, z) + phase
                row = int(math.floor((lat + math.pi / 2) / (math.pi / 8)))
                col = int(math.floor(lon / (math.pi / 8)))
                base = red if (row + col) % 2 == 0 else white
                diff = max(0.0, x * lx + y * ly + z * lz)
                k = 0.35 + 0.65 * diff
                spec = max(0.0, diff) ** 30 * 0.5
                px[qx, py] = tuple(min(255, int(ch * k + 255 * spec)) for ch in base) + (255,)
        strip.paste(img.resize((F, F), Image.LANCZOS), (frame * F, 0))
    return strip


def bounce():
    """A low thump that drops in pitch, with a short ring on top."""
    rate, secs = 22050, 0.45
    rnd = random.Random(1985)
    out = bytearray()
    ph1 = ph2 = 0.0
    for i in range(int(rate * secs)):
        t = i / rate
        f1 = 55 + 110 * math.exp(-t * 9)
        ph1 += 2 * math.pi * f1 / rate
        ph2 += 2 * math.pi * 640 / rate
        v = math.sin(ph1) * math.exp(-t * 7)
        v += 0.25 * math.sin(ph2) * math.exp(-t * 25)
        if t < 0.012:
            v += (rnd.random() * 2 - 1) * 0.5 * (1 - t / 0.012)
        out += struct.pack("<h", int(max(-1, min(1, v * 0.8)) * 32767))
    data = bytes(out)
    hdr = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    hdr += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16)
    hdr += b"data" + struct.pack("<I", len(data))
    return hdr + data


def tga(img):
    b = io.BytesIO()
    img.save(b, "TGA", rle=False)
    return b.getvalue()


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: make_amiga_pk4.py OUT.pk4")
    files = {}
    for root, _, names in os.walk(SRC):
        for n in names:
            p = os.path.join(root, n)
            files[os.path.relpath(p, SRC).replace(os.sep, "/")] = open(p, "rb").read()
    files["guis/amiga/room.tga"] = tga(room())
    files["guis/amiga/ball.tga"] = tga(ball())
    files["sound/amiga/bounce.wav"] = bounce()
    with zipfile.ZipFile(sys.argv[1], "w", zipfile.ZIP_DEFLATED) as z:
        for name in sorted(files):
            zi = zipfile.ZipInfo(name, DATE)
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = 0o644 << 16
            z.writestr(zi, files[name])
    for name in sorted(files):
        print(f"{len(files[name]):>9}  {name}")


if __name__ == "__main__":
    main()
