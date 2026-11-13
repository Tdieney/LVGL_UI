#!/usr/bin/env python3
"""Draw the Smart Studio icon and 360-degree radiant sun-ray masters.

Shared base masters (home, plus, minus, thermometer, drop and chevron_left)
are kept unchanged. This script draws the Studio icon masters on a 24-unit
grid (white ink on black, 256 x 256) and the 16 radial sun-ray masks in 4x
supersampled crop boxes around (240, 168) for the 480 x 320 Light page. Only
Pillow is needed.

Usage: python tools/draw_icons.py
Then:  python tools/generate_assets.py --force-icons
"""
import hashlib
import json
import math
import os

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_DIR = os.path.join(ROOT, "assets", "icons", "studio", "masters")
MANIFEST = os.path.join(ROOT, "assets", "icons", "studio", "manifest.json")
SIZE = 256
SCALE = SIZE / 24.0
STROKE = 1.75  # grid units; about 7 % of a 24-unit icon

SUN_CENTER = (240.0, 168.0)
RAY_R_IN = 60.0
RAY_R_OUT = 94.0
RAY_STROKE_PX = 5.5
RAY_SUPERSAMPLE = 4
RAYS = [
    ("ray_0",  -90.0, 234,  68, 12, 46),
    ("ray_1",  -67.5, 256,  74, 26, 46),
    ("ray_2",  -45.0, 276,  94, 38, 38),
    ("ray_3",  -22.5, 288, 126, 46, 26),
    ("ray_4",    0.0, 294, 162, 46, 12),
    ("ray_5",   22.5, 288, 184, 46, 26),
    ("ray_6",   45.0, 276, 204, 38, 38),
    ("ray_7",   67.5, 256, 216, 26, 46),
    ("ray_8",   90.0, 234, 222, 12, 46),
    ("ray_9",  112.5, 198, 216, 26, 46),
    ("ray_10", 135.0, 166, 204, 38, 38),
    ("ray_11", 157.5, 146, 184, 46, 26),
    ("ray_12", 180.0, 140, 162, 46, 12),
    ("ray_13", 202.5, 146, 126, 46, 26),
    ("ray_14", 225.0, 166,  94, 38, 38),
    ("ray_15", 247.5, 198,  74, 26, 46),
]


def arc(cx, cy, r, start_deg, end_deg, steps=None):
    """Points on a circle; angles in screen degrees (0 = right, 90 = down)."""
    span = end_deg - start_deg
    steps = steps or max(8, int(abs(span) / 4))
    return [(cx + r * math.cos(math.radians(start_deg + span * i / steps)),
             cy + r * math.sin(math.radians(start_deg + span * i / steps)))
            for i in range(steps + 1)]


def rounded_rect(x, y, w, h, r):
    pts = []
    pts += arc(x + w - r, y + r, r, -90, 0)
    pts += arc(x + w - r, y + h - r, r, 0, 90)
    pts += arc(x + r, y + h - r, r, 90, 180)
    pts += arc(x + r, y + r, r, 180, 270)
    pts.append(pts[0])
    return pts


def sliders():
    kr = 2.75
    y1, kx1 = 7.5, 9.0
    y2, kx2 = 16.5, 15.0
    return {"polylines": [
        [(3.0, y1), (kx1 - kr, y1)],
        [(kx1 + kr, y1), (21.0, y1)],
        arc(kx1, y1, kr, 0, 360, 96),
        [(3.0, y2), (kx2 - kr, y2)],
        [(kx2 + kr, y2), (21.0, y2)],
        arc(kx2, y2, kr, 0, 360, 96),
    ]}


def chevron_right():
    return {"polylines": [[(8.0, 3.5), (16.5, 12.0), (8.0, 20.5)]]}


def check():
    return {"polylines": [[(4.2, 12.6), (9.4, 17.8), (19.8, 6.4)]]}


def dots():
    return {"filled_circles": [
        (5.0, 12.0, 2.1),
        (12.0, 12.0, 2.1),
        (19.0, 12.0, 2.1),
    ]}


def cross():
    return {"polylines": [
        [(5.5, 5.5), (18.5, 18.5)],
        [(18.5, 5.5), (5.5, 18.5)],
    ]}


def snowflake():
    lines = []
    c = (12.0, 12.0)
    for k in range(6):
        a = math.radians(-90 + 60 * k)
        u = (math.cos(a), math.sin(a))
        n = (-u[1], u[0])
        tip = (c[0] + 10 * u[0], c[1] + 10 * u[1])
        if k < 3:
            back = (c[0] - 10 * u[0], c[1] - 10 * u[1])
            lines.append([tip, back])
        q = (c[0] + 6.4 * u[0], c[1] + 6.4 * u[1])
        for side in (1, -1):
            b = (q[0] + 2.9 * (0.72 * u[0] + side * 0.70 * n[0]),
                 q[1] + 2.9 * (0.72 * u[1] + side * 0.70 * n[1]))
            lines.append([q, b])
    return {"polylines": lines}


def moon():
    # Crescent between the circle (12, 12, r 9) and the circle (18, 6) that
    # passes through its top and right points.
    r2 = math.hypot(6, 3)
    outer = arc(12, 12, 9, 0, 270, 70)
    a_top = math.degrees(math.atan2(-3, -6)) % 360    # (12, 3) seen from (18, 6)
    a_right = math.degrees(math.atan2(6, 3)) % 360    # (21, 12) seen from (18, 6)
    inner = arc(18, 6, r2, a_top, a_right, 40)
    return {"polylines": [outer + inner]}


def away():
    return {"polylines": [
        [(10.5, 3.6), (6.2, 3.6)] + arc(6.2, 6.2, 2.6, 270, 180) +
        arc(6.2, 17.8, 2.6, 180, 90) + [(10.5, 20.4)],
        [(9.6, 12), (20.6, 12)],
        [(16.4, 7.8), (20.6, 12), (16.4, 16.2)],
    ]}


def scenes():
    return {"polylines": [
        rounded_rect(3.2, 3.2, 7.4, 7.4, 2.0),
        rounded_rect(13.4, 3.2, 7.4, 7.4, 2.0),
        rounded_rect(3.2, 13.4, 7.4, 7.4, 2.0),
        rounded_rect(13.4, 13.4, 7.4, 7.4, 2.0),
    ]}


def render(spec):
    """Anti-aliased strokes and filled shapes from an exact distance field."""
    if isinstance(spec, list):
        spec = {"polylines": spec}

    polylines = spec.get("polylines", [])
    filled_circles = spec.get("filled_circles", [])

    alpha = [[0.0] * SIZE for _ in range(SIZE)]
    half = STROKE * SCALE / 2.0

    for line in polylines:
        pts = [(x * SCALE, y * SCALE) for x, y in line]
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            dx, dy = bx - ax, by - ay
            length2 = dx * dx + dy * dy
            x0 = max(0, int(min(ax, bx) - half - 2))
            x1 = min(SIZE - 1, int(max(ax, bx) + half + 2))
            y0 = max(0, int(min(ay, by) - half - 2))
            y1 = min(SIZE - 1, int(max(ay, by) + half + 2))
            for py in range(y0, y1 + 1):
                cy = py + 0.5
                row = alpha[py]
                for px in range(x0, x1 + 1):
                    cx = px + 0.5
                    t = 0.0 if length2 == 0 else ((cx - ax) * dx + (cy - ay) * dy) / length2
                    t = 0.0 if t < 0 else 1.0 if t > 1 else t
                    ex, ey = cx - (ax + t * dx), cy - (ay + t * dy)
                    cover = half + 0.5 - math.sqrt(ex * ex + ey * ey)
                    if cover > row[px]:
                        row[px] = 1.0 if cover > 1 else cover

    for gx, gy, gr in filled_circles:
        ccx, ccy, cr = gx * SCALE, gy * SCALE, gr * SCALE
        x0 = max(0, int(ccx - cr - 2))
        x1 = min(SIZE - 1, int(ccx + cr + 2))
        y0 = max(0, int(ccy - cr - 2))
        y1 = min(SIZE - 1, int(ccy + cr + 2))
        for py in range(y0, y1 + 1):
            cy = py + 0.5
            row = alpha[py]
            for px in range(x0, x1 + 1):
                cx = px + 0.5
                dist = math.hypot(cx - ccx, cy - ccy)
                cover = cr + 0.5 - dist
                if cover > row[px]:
                    row[px] = 1.0 if cover > 1 else cover

    img = Image.new("L", (SIZE, SIZE), 0)
    img.putdata([int(round(255 * max(0.0, v))) for row in alpha for v in row])
    return img


def render_ray(angle_deg, x0, y0, w, h):
    """4x supersampled radial sun ray with round endcaps in its screen crop box."""
    ss = RAY_SUPERSAMPLE
    mw, mh = w * ss, h * ss
    rad = math.radians(angle_deg)
    ux = round(math.cos(rad), 12)
    uy = round(math.sin(rad), 12)
    ax = (SUN_CENTER[0] + RAY_R_IN * ux - x0) * ss
    ay = (SUN_CENTER[1] + RAY_R_IN * uy - y0) * ss
    bx = (SUN_CENTER[0] + RAY_R_OUT * ux - x0) * ss
    by = (SUN_CENTER[1] + RAY_R_OUT * uy - y0) * ss
    dx, dy = bx - ax, by - ay
    length2 = dx * dx + dy * dy
    mhalf = (RAY_STROKE_PX / 2.0) * ss

    data = [0] * (mw * mh)
    for py in range(mh):
        cy = py + 0.5
        row_off = py * mw
        for px in range(mw):
            cx = px + 0.5
            t = ((cx - ax) * dx + (cy - ay) * dy) / length2
            t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
            dist = math.hypot(cx - (ax + t * dx), cy - (ay + t * dy))
            cover = mhalf + 0.5 - dist
            if cover > 0.0:
                data[row_off + px] = 255 if cover >= 1.0 else int(round(255.0 * cover))

    img = Image.new("L", (mw, mh), 0)
    img.putdata(data)
    return img


ICONS = {
    "scenes": scenes,
    "sliders": sliders,
    "chevron_right": chevron_right,
    "snowflake": snowflake,
    "moon": moon,
    "away": away,
    "check": check,
    "dots": dots,
    "cross": cross,
}


def update_manifest(drawn):
    """Record each drawn master's hash and ink box; preserve shared base masters."""
    with open(MANIFEST, "r", encoding="utf-8") as f:
        manifest = json.load(f)
    stale = [k for k, v in manifest["masters"].items()
             if v.get("source") == "tools/draw_icons.py" and k not in drawn]
    for k in stale:
        del manifest["masters"][k]
    for name, path in drawn.items():
        with open(path, "rb") as f:
            digest = hashlib.sha256(f.read()).hexdigest()
        img = Image.open(path)
        manifest["masters"][name] = {
            "source": "tools/draw_icons.py",
            "master_file": f"masters/{name}_master.png",
            "master_size": list(img.size),
            "master_sha256": digest,
            "ink_bbox": list(img.getbbox()),
        }
    with open(MANIFEST, "w", encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    drawn = {}
    for name, shape in ICONS.items():
        path = os.path.join(OUT_DIR, f"{name}_master.png")
        render(shape()).save(path, optimize=True)
        drawn[name] = path
        print(f"wrote {os.path.relpath(path, ROOT)}")

    for name, angle_deg, x0, y0, w, h in RAYS:
        path = os.path.join(OUT_DIR, f"{name}_master.png")
        render_ray(angle_deg, x0, y0, w, h).save(path, optimize=True)
        drawn[name] = path
        print(f"wrote {os.path.relpath(path, ROOT)}")

    update_manifest(drawn)
    print(f"updated {os.path.relpath(MANIFEST, ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
