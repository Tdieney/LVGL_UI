#!/usr/bin/env python3
"""Draw the 6 tab-bar icons as alpha masks, matching the finalized mockup's
hand-drawn SVG line-icons (viewBox 0 0 24 24, stroke 1.8, round caps) — one
PNG per icon. build_assets.bat then runs png2lvgl.py --cf alpha_4 on each, so
the tab bar can recolor a single alpha asset (gray when idle / accent when
active) exactly like a font glyph, but with the mockup's exact shapes instead
of FontAwesome's.

Usage: python gen_icons.py <out_dir>   (writes icon_<key>.png for each icon)
"""
import math
import os
import sys

from PIL import Image, ImageDraw

N = 24        # final icon size (px)
SS = 8        # supersample
C = N * SS    # canvas
S = C / 24.0  # mockup-unit -> canvas-px scale
W = round(1.8 * S)  # stroke width
INK = (255, 255, 255, 255)


def p(x, y):
    return (x * S, y * S)


def cap(d, x, y, w=W):
    r = w / 2.0
    cx, cy = p(x, y)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=INK)


def stroke(d, pts, w=W, close=False, caps=True):
    xy = [p(x, y) for x, y in pts]
    if close:
        xy.append(xy[0])
    d.line(xy, fill=INK, width=w, joint="curve")
    if caps:
        for x, y in (pts if not close else []):
            cap(d, x, y, w)


def dot(d, x, y, r):
    cx, cy = p(x, y)
    rr = r * S
    d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=INK)


def ring(d, cx, cy, r, w=W):
    a, b = p(cx - r, cy - r)
    c, e = p(cx + r, cy + r)
    d.ellipse([a, b, c, e], outline=INK, width=w)


def rrect(d, x1, y1, x2, y2, rad, w=W):
    a, b = p(x1, y1)
    c, e = p(x2, y2)
    d.rounded_rectangle([a, b, c, e], radius=rad * S, outline=INK, width=w)


def arc(d, cx, cy, r, a0, a1, w=W):
    a, b = p(cx - r, cy - r)
    c, e = p(cx + r, cy + r)
    d.arc([a, b, c, e], a0, a1, fill=INK, width=w)


def i_dash(d):  # gauge: top-arc + needle + hub dot
    bbox = [*p(4, 8), *p(20, 24)]  # circle center (12,16) r8
    d.arc(bbox, 180, 360, fill=INK, width=W)
    cap(d, 4, 16); cap(d, 20, 16)
    stroke(d, [(12, 16), (15, 10)])
    dot(d, 12, 16, 1.6)


def i_mon(d):  # heartbeat polyline
    stroke(d, [(2, 13), (7, 13), (10, 5), (14, 20), (17, 13), (22, 13)])


def i_ctrl(d):  # 3 horizontal sliders with knobs
    for y, kx in ((6, 14), (12, 9), (18, 17)):
        stroke(d, [(4, y), (20, y)])
        dot(d, kx, y, 2.2)


def i_graph(d):  # bar chart: baseline + 3 outlined bars
    stroke(d, [(3, 21), (21, 21)])
    for x, top in ((5, 13), (10.5, 8), (16, 4)):
        a, b = p(x, top)
        c, e = p(x + 3, 21)
        d.rectangle([a, b, c, e], outline=INK, width=W)


def i_diag(d):  # warning triangle + exclamation
    stroke(d, [(12, 3), (22, 20), (2, 20)], close=True)
    stroke(d, [(12, 9), (12, 14)])
    dot(d, 12, 17, 1.1)


def i_set(d):  # gear: ring + 8 teeth + hub dot (the old cross-hair read as "target")
    ring(d, 12, 12, 4.6)
    dot(d, 12, 12, 1.8)
    for k in range(8):
        a = math.radians(k * 45.0)
        stroke(d, [(12 + 6.7 * math.cos(a), 12 + 6.7 * math.sin(a)),
                   (12 + 9.2 * math.cos(a), 12 + 9.2 * math.sin(a))])


# ---- Monitor / Diagnostics row icons (match the mockup's .micon / .dicon SVGs) ----

def i_bolt(d):  # lightning bolt outline (CURRENT, over/under-voltage, overcurrent)
    stroke(d, [(13, 2), (5, 14), (11, 14), (10, 22), (19, 10), (13, 10), (14, 2)], close=True, caps=False)


def i_thermo(d):  # thermometer: stem + bulb
    rrect(d, 10, 3, 14, 15, 2)
    ring(d, 12, 18, 3)


def i_link(d):  # RS-485 link: two nodes + connector
    dot(d, 6, 12, 2.4); dot(d, 18, 12, 2.4)
    stroke(d, [(8.4, 12), (15.6, 12)])


def i_check(d):  # OK status mark
    stroke(d, [(5, 13), (10, 18), (19, 6)])


def i_xmark(d):  # fault status mark
    stroke(d, [(7, 7), (17, 17)]); stroke(d, [(17, 7), (7, 17)])


def i_chevron(d):  # dropdown arrow (replaces the Montserrat LV_SYMBOL_DOWN glyph)
    stroke(d, [(6, 10), (12, 16), (18, 10)])


def _rot_arrow(d, cw):  # "redo/undo" rotate icon: chunky open ring + SOLID triangle head
    # Thick near-full circle with a gap and a filled triangular arrowhead.
    # CCW is the exact horizontal mirror of CW.
    # phi: 0=east, CCW positive; screen y is down so a point is (cx+r*cos, cy-r*sin).
    cx = cy = 12.0
    r = 7.8
    sw = round(2.7 * S)  # chunky ring stroke (reference is a bold line)
    a, b = (58.0, 320.0) if cw else (122.0, -140.0)  # head at `a`; gap on the right (CW) / left (CCW)
    n = 60
    pts = [(cx + r * math.cos(math.radians(a + (b - a) * i / n)),
            cy - r * math.sin(math.radians(a + (b - a) * i / n))) for i in range(n + 1)]
    xy = [p(x, y) for x, y in pts]
    d.line(xy, fill=INK, width=sw, joint="curve")
    cap(d, pts[-1][0], pts[-1][1], sw)  # round tail
    cap(d, pts[0][0], pts[0][1], sw)    # smooth the head-base junction
    # Solid triangular arrowhead at the head end `a`, pointing along travel.
    phi = math.radians(a)
    ax, ay = cx + r * math.cos(phi), cy - r * math.sin(phi)
    tx, ty = (math.sin(phi), math.cos(phi)) if cw else (-math.sin(phi), -math.cos(phi))
    nx, ny = -ty, tx
    Hh, Wh = 6.4, 5.0  # head length / half-width (mockup units)
    d.polygon([p(ax + tx * Hh, ay + ty * Hh),
               p(ax + nx * Wh, ay + ny * Wh),
               p(ax - nx * Wh, ay - ny * Wh)], fill=INK)


def i_cw(d):   # clockwise rotation
    _rot_arrow(d, True)


def i_ccw(d):  # counter-clockwise rotation
    _rot_arrow(d, False)


ICONS = {"dash": i_dash, "mon": i_mon, "ctrl": i_ctrl,
         "graph": i_graph, "diag": i_diag, "set": i_set,
         "bolt": i_bolt, "thermo": i_thermo,
         "link": i_link, "check": i_check, "xmark": i_xmark, "chevron": i_chevron,
         "cw": i_cw, "ccw": i_ccw}


# Per-icon final size override (px). Sidebar navigation icons are native 36px
# masks so they stay crisp without runtime zooming. Row/status icons remain
# 24px; direction rotation arrows render bigger on 52px controls.
# button (alpha images can't be zoomed at runtime — see the CLAUDE.md gotcha —
# so a larger button icon must be drawn larger natively).
SIZES = {
    "dash": 44, "mon": 44, "ctrl": 44,
    "graph": 44, "diag": 44, "set": 44,
    "cw": 34, "ccw": 34,
}


def main():
    global N, C, S, W
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out_dir, exist_ok=True)
    for key, fn in ICONS.items():
        N = SIZES.get(key, 24)
        C = N * SS
        S = C / 24.0        # mockup-unit -> canvas-px scale (drawings are authored in 24u)
        W = round(1.8 * S)  # stroke width scales with the icon
        img = Image.new("RGBA", (C, C), (0, 0, 0, 0))
        fn(ImageDraw.Draw(img))
        img = img.resize((N, N), Image.LANCZOS)
        path = os.path.join(out_dir, f"icon_{key}.png")
        img.save(path)
        print(f"wrote {path} ({N}x{N})")


if __name__ == "__main__":
    main()
