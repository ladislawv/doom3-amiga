#!/usr/bin/env python3
"""The release's icon art (original, drawn here): three GlowIcons, each with a
normal and a selected image, written as .d3ic files for release/src/mkicons.c,
which builds the .info files on the Amiga (icon.library's IconControl).

    python3 release/icons/art/make_icons.py [OUTDIR] [--preview PNG]

  game    the launcher (Doom3.info): a riveted steel plate, cracked, hellfire
          glowing through, a burning "3" under "DOOM"; selected: hotter
  drawer  the drawer (Doom3.info beside it): a sci-fi supply crate with hazard
          stripes and a "3" decal; selected: the drawer open, glowing inside
  readme  the guide (ReadMe.info): a datapad with amber text; selected: brighter

Drawn at 4x and scaled down (smooth edges), then each image mapped to at most
255 colours, colour 0 transparent (GlowIcons have no partial transparency).

.d3ic: "D3IC", width (u16 BE), height (u16 BE), then per image (2): colours
(u16 BE, at most 256), transparent colour (u8), palette (RGB bytes), pixels
(width * height bytes, one colour index each).
Needs Pillow (python3-pil) and the DejaVu fonts (fonts-dejavu-core).
"""
import os
import random
import struct
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

SIZE = 64            # the icons' size on the Workbench
S = SIZE * 4         # drawn at
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_C = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"


def font(path, px):
    return ImageFont.truetype(path if os.path.exists(path) else FONT, px)


def vgrad(w, h, top, bottom):
    """A vertical gradient, RGBA."""
    g = Image.new("RGBA", (w, h))
    d = ImageDraw.Draw(g)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line([(0, y), (w, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
    return g


def masked(fill, mask):
    """fill (RGBA) where mask (L) is set."""
    out = Image.new("RGBA", fill.size, (0, 0, 0, 0))
    out.paste(fill, (0, 0), mask)
    return out


def glow(mask, colour, radius, strength=1.0):
    """A coloured glow around a mask."""
    m = mask.filter(ImageFilter.GaussianBlur(radius))
    if strength != 1.0:
        m = m.point(lambda v: min(255, int(v * strength)))
    return masked(Image.new("RGBA", mask.size, colour + (255,)), m)


def text_mask(text, fnt, centre, size=(S, S), stretch_y=1.0):
    m = Image.new("L", size, 0)
    d = ImageDraw.Draw(m)
    box = d.textbbox((0, 0), text, font=fnt)
    w, h = box[2] - box[0], box[3] - box[1]
    if stretch_y != 1.0:
        t = Image.new("L", (w + 8, h + 8), 0)
        ImageDraw.Draw(t).text((4 - box[0], 4 - box[1]), text, font=fnt, fill=255)
        t = t.resize((t.width, int(t.height * stretch_y)), Image.LANCZOS)
        m.paste(t, (centre[0] - t.width // 2, centre[1] - t.height // 2))
    else:
        d.text((centre[0] - w // 2 - box[0], centre[1] - h // 2 - box[1]), text, font=fnt, fill=255)
    return m


def rounded_mask(box, r, size=(S, S)):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).rounded_rectangle(box, r, fill=255)
    return m


def bevel(img, mask, light=70, dark=70, width=6):
    """Light on the top-left edges, shade on the bottom-right ones."""
    inner = mask.filter(ImageFilter.MinFilter(width * 2 + 1))
    edge = ImageChops.subtract(mask, inner)
    tl = ImageChops.subtract(mask, ImageChops.offset(mask, width, width))
    br = ImageChops.subtract(mask, ImageChops.offset(mask, -width, -width))
    tl = ImageChops.multiply(tl, edge)
    br = ImageChops.multiply(br, edge)
    img.alpha_composite(masked(Image.new("RGBA", img.size, (255, 255, 255, 255)), tl.point(lambda v: v * light // 255)))
    img.alpha_composite(masked(Image.new("RGBA", img.size, (0, 0, 0, 255)), br.point(lambda v: v * dark // 255)))


def scratches(img, mask, box, n, seed, colour=(255, 255, 255), alpha=26):
    rnd = random.Random(seed)
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    x0, y0, x1, y1 = box
    for _ in range(n):
        x, y = rnd.randint(x0, x1), rnd.randint(y0, y1)
        l = rnd.randint(8, 40)
        d.line([(x, y), (x + l, y + rnd.randint(-6, 6))], fill=colour + (alpha,), width=2)
    img.alpha_composite(masked(layer, ImageChops.multiply(mask, layer.split()[3].point(lambda v: 255 if v else 0))))


def rivet(img, c, r=9):
    d = ImageDraw.Draw(img)
    x, y = c
    d.ellipse([x - r, y - r, x + r, y + r], fill=(22, 23, 26, 255))
    d.ellipse([x - r + 2, y - r + 2, x + r - 2, y + r - 2], fill=(112, 116, 124, 255))
    d.ellipse([x - r + 5, y - r + 4, x + 1, y], fill=(190, 194, 200, 255))


def cracks(seed, starts, size=(S, S)):
    """Jagged crack lines, as a mask."""
    rnd = random.Random(seed)
    m = Image.new("L", size, 0)
    d = ImageDraw.Draw(m)
    for (x, y, ang) in starts:
        import math
        for branch in range(2):
            px, py, a = x, y, ang + (0.5 if branch else 0)
            w = 5
            for _ in range(7):
                a += rnd.uniform(-0.7, 0.7)
                nx, ny = px + math.cos(a) * rnd.randint(12, 22), py + math.sin(a) * rnd.randint(12, 22)
                d.line([(px, py), (nx, ny)], fill=255, width=w)
                px, py = nx, ny
                w = max(2, w - 1)
    return m


# ---------------------------------------------------------------- the game


def flames(seed, base_y, x0, x1, height, n, peak=1.0):
    """A flame front rising from base_y: n tongues of uneven height, tallest in
    the middle, each leaning and narrowing to a curled tip; a smooth mask."""
    import math
    rnd = random.Random(seed)
    m = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(m)
    span = x1 - x0
    for i in range(n):
        cx = x0 + span * (i + 0.5 + rnd.uniform(-0.25, 0.25)) / n
        centre = 1.0 - abs((cx - (x0 + x1) / 2) / (span / 2)) ** 2 * 0.55
        h = height * centre * peak * rnd.uniform(0.7, 1.05)
        w = span / n * rnd.uniform(0.62, 0.8)
        lean = rnd.uniform(-0.45, 0.45) * w
        left, right = [], []
        for k in range(17):
            t = k / 16
            half = w * (1 - t) ** 0.8 * (1 + 0.25 * math.sin(t * math.pi))
            x = cx + lean * t * t + math.sin(t * 5.5 + i) * w * 0.12 * t
            y = base_y - h * t
            left.append((x - half, y))
            right.append((x + half, y))
        d.polygon(left + right[::-1], fill=255)
    d.rectangle((x0, base_y - 2, x1, base_y + 60), fill=255)
    return m.filter(ImageFilter.GaussianBlur(4)).point(lambda v: 255 if v > 100 else 0)


def gashes(seed, starts):
    """Cracks: angular zig-zags in from the edge, a dark gash with a hot core."""
    import math
    rnd = random.Random(seed)
    dark = Image.new("L", (S, S), 0)
    core = Image.new("L", (S, S), 0)
    dd, dc = ImageDraw.Draw(dark), ImageDraw.Draw(core)
    for (x, y, ang, steps) in starts:
        px, py, w, side = x, y, 14, 1
        for i in range(steps):
            a = ang + side * rnd.uniform(0.35, 0.75)
            side = -side
            l = rnd.randint(12, 20)
            nx, ny = px + math.cos(a) * l, py + math.sin(a) * l
            dd.line([(px, py), (nx, ny)], fill=255, width=w)
            dd.ellipse([nx - w / 2, ny - w / 2, nx + w / 2, ny + w / 2], fill=255)
            cw = max(3, w - 9)
            dc.line([(px, py), (nx, ny)], fill=255, width=cw)
            if i == 1:	# a short branch
                ba = a + side * 1.0
                ex, ey = nx + math.cos(ba) * 16, ny + math.sin(ba) * 16
                dd.line([(nx, ny), (ex, ey)], fill=255, width=7)
                dc.line([(nx, ny), (ex, ey)], fill=255, width=3)
            px, py = nx, ny
            w = max(6, w - 3)
    return dark, core


def draw_game(selected):
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    box = (22, 48, S - 22, S - 10)
    plate = rounded_mask(box, 30)
    # hellfire rising behind the plate (opaque: GlowIcons have no half-transparency)
    hot = 1.22 if selected else 1.0
    outer = flames(4, 80, 18, S - 18, 76, 7, hot)
    mid = ImageChops.multiply(flames(9, 80, 34, S - 34, 56, 6, hot), outer)
    inner = ImageChops.multiply(flames(13, 80, 56, S - 56, 36, 5, hot), mid)
    img.alpha_composite(masked(vgrad(S, S, (120, 0, 0), (230, 40, 0)), outer))
    img.alpha_composite(masked(vgrad(S, S, (230, 50, 0), (255, 150, 10)), mid))
    img.alpha_composite(masked(vgrad(S, S, (255, 170, 20), (255, 245, 170)), inner))
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (0, 0, 0, 255)), ImageChops.offset(plate, 0, 4)))
    metal = vgrad(S, S, (98, 102, 110), (28, 30, 35))
    img.alpha_composite(masked(metal, plate))
    scratches(img, plate, box, 80, 3)
    # cracks: hellfire behind them
    dark, core = gashes(17, [(24, 196, -0.45, 3), (232, 136, 3.45, 3), (196, 246, -2.0, 3)])
    dark, core = ImageChops.multiply(dark, plate), ImageChops.multiply(core, plate)
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (14, 8, 6, 255)), dark))
    img.alpha_composite(glow(core, (255, 80, 0), 5, 1.8 if selected else 1.2))
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (255, 230, 140, 255) if selected else (255, 160, 40, 255)), core))
    bevel(img, plate, 100, 120, 7)
    for c in [(42, 68), (S - 42, 68), (42, S - 30), (S - 42, S - 30)]:
        rivet(img, c)
    # "DOOM", embossed red steel with a dark rim
    dm = text_mask("DOOM", font(FONT_C, 46), (S // 2, 90))
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (10, 0, 0, 255)), dm.filter(ImageFilter.MaxFilter(7))))
    img.alpha_composite(masked(vgrad(S, S, (255, 150, 120), (110, 6, 0)), dm))
    # the burning "3"
    three = text_mask("3", font(FONT, 150), (S // 2, 170), stretch_y=1.08)
    img.alpha_composite(glow(three, (255, 60, 0), 16, 2.2 if selected else 1.5))
    img.alpha_composite(glow(three, (255, 170, 30), 6, 1.6 if selected else 0.9))
    outline = three.filter(ImageFilter.MaxFilter(7))
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (40, 0, 0, 255)), outline))
    fire = vgrad(S, S, (255, 255, 200), (255, 200, 40)) if selected else vgrad(S, S, (255, 236, 120), (210, 30, 0))
    img.alpha_composite(masked(fire, three))
    # a highlight on the 3's upper part
    hi = ImageChops.multiply(three, vgrad(S, S, (255, 255, 255), (0, 0, 0)).convert("L").point(lambda v: max(0, v - 150) * 2))
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (255, 255, 255, 255)), hi.point(lambda v: v // 2)))
    return img


# ---------------------------------------------------------------- the drawer


def hazard(w, h, phase=0):
    img = Image.new("RGBA", (w, h), (230, 180, 20, 255))
    d = ImageDraw.Draw(img)
    for x in range(-h * 2 + phase, w + h, 36):
        d.polygon([(x, h), (x + 18, h), (x + 18 + h, 0), (x + h, 0)], fill=(25, 22, 18, 255))
    return img


def draw_drawer(selected):
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    # the crate's body
    body_box = (18, 40, S - 18, S - 20)
    body = rounded_mask(body_box, 16)
    img.alpha_composite(masked(vgrad(S, S, (78, 88, 76), (34, 40, 34)), body))
    scratches(img, body, body_box, 50, 7)
    bevel(img, body, 80, 110, 6)
    # top lid with a rim
    lid = rounded_mask((12, 30, S - 12, 62), 10)
    img.alpha_composite(masked(vgrad(S, S, (120, 130, 118), (58, 66, 56)), lid))
    bevel(img, lid, 100, 90, 5)
    for x in (34, S - 34):
        rivet(img, (x, 46), 7)
    # hazard stripes along the bottom
    hz = hazard(S - 60, 22, 4)
    hzm = Image.new("L", (S, S), 0)
    ImageDraw.Draw(hzm).rectangle((30, S - 52, S - 30, S - 32), fill=255)
    hzl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    hzl.paste(hz, (30, S - 52))
    img.alpha_composite(masked(hzl, hzm))
    # the drawer
    d_box = (40, 82, S - 40, 168)
    if selected:
        # open: the front slid out and down, the inside glowing
        hole = rounded_mask(d_box, 6)
        img.alpha_composite(masked(Image.new("RGBA", (S, S), (10, 6, 6, 255)), hole))
        inner = ImageChops.multiply(hole, vgrad(S, S, (0, 0, 0), (255, 255, 255)).convert("L"))
        img.alpha_composite(glow(inner, (255, 60, 0), 10, 1.8))
        img.alpha_composite(masked(Image.new("RGBA", (S, S), (255, 120, 20, 255)), inner.point(lambda v: v // 2)))
        front_box = (30, 120, S - 30, 206)
    else:
        front_box = d_box
    front = rounded_mask(front_box, 8)
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (0, 0, 0, 255)), ImageChops.offset(front, 0, 6)))
    img.alpha_composite(masked(vgrad(S, S, (150, 158, 148), (70, 78, 68)), front))
    bevel(img, front, 110, 120, 5)
    fx0, fy0, fx1, fy1 = front_box
    d = ImageDraw.Draw(img)
    # handle
    hy = fy0 + 26
    d.rounded_rectangle((S // 2 - 34, hy - 7, S // 2 + 34, hy + 7), 7, fill=(28, 30, 28, 255))
    d.rounded_rectangle((S // 2 - 30, hy - 4, S // 2 + 30, hy + 1), 4, fill=(170, 176, 170, 255))
    # the "3" decal and a status light
    m3 = text_mask("3", font(FONT, 48), (fx0 + 30, fy1 - 26))
    img.alpha_composite(glow(m3, (255, 40, 0), 5, 1.4 if selected else 0.8))
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (230, 40, 10, 255)), m3))
    lm = Image.new("L", (S, S), 0)
    ImageDraw.Draw(lm).ellipse((fx1 - 36, fy1 - 34, fx1 - 18, fy1 - 16), fill=255)
    img.alpha_composite(glow(lm, (255, 30, 0) if not selected else (60, 255, 60), 8, 1.6))
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (255, 80, 60, 255) if not selected else (150, 255, 150, 255)), lm))
    return img


# ---------------------------------------------------------------- the guide


def draw_readme(selected):
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    box = (46, 14, S - 46, S - 14)
    pad = rounded_mask(box, 26)
    img.alpha_composite(glow(pad, (255, 120, 0), 10, 0.7 if selected else 0.35))
    img.alpha_composite(masked(vgrad(S, S, (70, 72, 78), (24, 25, 29)), pad))
    scratches(img, pad, box, 40, 21)
    bevel(img, pad, 90, 120, 6)
    # the screen
    sb = (64, 38, S - 64, 182)
    scr = rounded_mask(sb, 8)
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (6, 8, 6, 255)), scr))
    base = (255, 190, 60) if selected else (230, 150, 30)
    img.alpha_composite(glow(scr, base, 6, 0.25 if selected else 0.12))
    d = ImageDraw.Draw(img)
    # a header bar and text lines
    d.rectangle((sb[0] + 10, sb[1] + 12, sb[2] - 10, sb[1] + 28), fill=base + (255,))
    rnd = random.Random(5)
    y = sb[1] + 44
    lines = Image.new("L", (S, S), 0)
    ld = ImageDraw.Draw(lines)
    while y < sb[3] - 14:
        w = rnd.randint(50, sb[2] - sb[0] - 24)
        ld.rectangle((sb[0] + 12, y, sb[0] + 12 + w, y + 6), fill=255)
        y += 15
    img.alpha_composite(glow(lines, base, 3, 0.8 if selected else 0.5))
    img.alpha_composite(masked(Image.new("RGBA", (S, S), tuple(min(255, c + 40) for c in base) + (255,)), lines))
    # scan lines over the screen
    sl = Image.new("L", (S, S), 0)
    sld = ImageDraw.Draw(sl)
    for yy in range(sb[1], sb[3], 6):
        sld.line([(sb[0], yy), (sb[2], yy)], fill=70, width=2)
    img.alpha_composite(masked(Image.new("RGBA", (S, S), (0, 0, 0, 255)), ImageChops.multiply(sl, scr)))
    # buttons
    for i, x in enumerate((92, 128, 164)):
        col = (200, 40, 20) if i == 1 else (90, 94, 100)
        if selected and i == 1:
            col = (255, 90, 40)
        d.ellipse((x - 11, 200, x + 11, 222), fill=(18, 18, 20, 255))
        d.ellipse((x - 8, 202, x + 8, 218), fill=col + (255,))
    return img


# ---------------------------------------------------------------- output


def to_icon(img):
    """Scaled down; colours: index 0 transparent, at most 255 others."""
    small = img.resize((SIZE, SIZE), Image.LANCZOS)
    alpha = small.split()[3]
    # edge pixels: their colour over black, so the outline stays dark
    rgb = Image.new("RGB", small.size, (0, 0, 0))
    rgb.paste(small, (0, 0), small)
    opaque = [a >= 110 for a in alpha.getdata()]
    q = rgb.quantize(colors=255, method=Image.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:255 * 3]
    idx = [0 if not o else p + 1 for p, o in zip(q.getdata(), opaque)]
    ncol = 1 + max(idx)
    palette = [0, 0, 0] + pal[:3 * (ncol - 1)]
    return idx, palette, ncol


def write_d3ic(path, images):
    with open(path, "wb") as f:
        f.write(b"D3IC" + struct.pack(">HH", SIZE, SIZE))
        for idx, palette, ncol in images:
            f.write(struct.pack(">HB", ncol, 0))
            f.write(bytes(palette[:3 * ncol]))
            f.write(bytes(idx))


def preview(icons, path):
    """The icons as the Workbench shows them (transparent colour dropped) on
    two backgrounds, 3x, for looking at; not shipped."""
    cols = []
    for name, (normal, sel) in icons.items():
        for idx, palette, ncol in (normal, sel):
            im = Image.new("RGBA", (SIZE, SIZE))
            im.putdata([(0, 0, 0, 0) if i == 0 else (palette[3 * i], palette[3 * i + 1], palette[3 * i + 2], 255) for i in idx])
            cols.append(im)
    pad = 12
    W = len(cols) * (SIZE + pad) + pad
    out = Image.new("RGB", (W * 3, (SIZE * 2 + pad * 3) * 3))
    for row, bg in enumerate([(170, 170, 170), (58, 88, 130)]):
        band = Image.new("RGBA", (W, SIZE + 2 * pad), bg + (255,))
        for i, im in enumerate(cols):
            band.alpha_composite(im, (pad + i * (SIZE + pad), pad))
        out.paste(band.convert("RGB").resize((W * 3, (SIZE + 2 * pad) * 3), Image.NEAREST), (0, row * (SIZE + pad * 2) * 3))
    out.save(path)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else os.path.dirname(os.path.abspath(__file__))
    prev = sys.argv[sys.argv.index("--preview") + 1] if "--preview" in sys.argv else None
    icons = {}
    for name, fn in (("game", draw_game), ("drawer", draw_drawer), ("readme", draw_readme)):
        icons[name] = (to_icon(fn(False)), to_icon(fn(True)))
        write_d3ic(os.path.join(outdir, name + ".d3ic"), icons[name])
        print(name, "colours", icons[name][0][2], icons[name][1][2])
    if prev:
        preview(icons, prev)
        print("preview", prev)


if __name__ == "__main__":
    main()
