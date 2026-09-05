#!/usr/bin/env python3
"""Render the complete static Dashboard gauge face as one Alpha-4 asset.

The face follows ``drafts/dashboard_display_only/01_telemetry_gauge.png``:
outer gray arc, speed-scale lane, labels, inner ticks, thin inner ring and a
faint BLDC rotor/stator motif. LVGL overlays only the dynamic green speed arc,
RPM digits and direction icon, keeping xSPI updates small.
"""

import math
import sys

from PIL import Image, ImageDraw, ImageFont


W = H = 320
CX = CY = 160.0
ANGLE0 = 135.0
SWEEP = 270.0
SS = 4

BG = (255, 255, 255, 0)
# The asset is recolored to near-black in LVGL. Alpha alone creates the gray
# hierarchy on the white card, so Alpha-4 is enough and costs 1/4 of RGB565.
OUTER = (255, 255, 255, 54)
SCALE = (255, 255, 255, 190)
INNER = (255, 255, 255, 42)
MOTIF = (255, 255, 255, 36)
MOTIF_SOFT = (255, 255, 255, 24)


def polar(cx, cy, radius, degrees):
    angle = math.radians(degrees)
    return cx + radius * math.cos(angle), cy + radius * math.sin(angle)


def arc(draw, cx, cy, radius, start, end, color, width):
    draw.arc([cx - radius, cy - radius, cx + radius, cy + radius],
             start, end, fill=color, width=width)


def faded_arc(draw, cx, cy, radius, start, end, color, width, fade):
    """Draw a reference arc whose two outer tails dissolve into the card."""
    step = 1.0
    angle = start
    while angle < end:
        next_angle = min(angle + step, end)
        mid = (angle + next_angle) * 0.5
        edge_distance = min(mid - start, end - mid)
        opacity = min(1.0, max(0.0, edge_distance / fade))
        segment = (color[0], color[1], color[2],
                   int(round(color[3] * opacity)))
        arc(draw, cx, cy, radius, angle, next_angle + 0.25,
            segment, width)
        angle = next_angle


def font(size):
    paths = (
        "tools/ttf/JetBrainsMono-Bold.ttf",
        "ttf/JetBrainsMono-Bold.ttf",
    )
    for path in paths:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            pass
    return ImageFont.load_default()


def draw_motor_cutaway(draw, cx, cy, scale):
    """Detailed motor cross-section matching the supplied gauge reference."""
    def line(points, color=MOTIF, width=1, closed=False):
        if closed:
            points = list(points) + [points[0]]
        draw.line(points, fill=color, width=width * scale, joint="curve")

    # Lamination pack.  The inner broken ring and the winding traces are what
    # make this read as a motor cutaway instead of a generic decorative rotor.
    arc(draw, cx, cy, 75 * scale, 0, 359.9, MOTIF, 2 * scale)
    arc(draw, cx, cy, 70 * scale, 0, 359.9, MOTIF_SOFT, 1 * scale)
    arc(draw, cx, cy, 50 * scale, 0, 359.9, MOTIF, 2 * scale)

    # Eight stator teeth, pole shoes and four visible winding turns per pole.
    for index in range(8):
        mid = -90.0 + index * 45.0
        tooth = [
            polar(cx, cy, 51 * scale, mid - 8.5),
            polar(cx, cy, 68 * scale, mid - 14.0),
            polar(cx, cy, 68 * scale, mid + 14.0),
            polar(cx, cy, 51 * scale, mid + 8.5),
        ]
        line(tooth, width=1, closed=True)
        for radius in (56, 59, 62, 65):
            arc(draw, cx, cy, radius * scale, mid - 11.5, mid + 11.5,
                MOTIF_SOFT, 1 * scale)

        # Narrow pole shoe pointing toward the rotor air gap.
        shoe = [
            polar(cx, cy, 45 * scale, mid - 7.0),
            polar(cx, cy, 51 * scale, mid - 8.5),
            polar(cx, cy, 51 * scale, mid + 8.5),
            polar(cx, cy, 45 * scale, mid + 7.0),
        ]
        line(shoe, width=1, closed=True)

    # Rotor body, alternating magnet pockets, bearing and shaft.  These remain
    # intentionally pale so the live RPM digits stay dominant.
    arc(draw, cx, cy, 43 * scale, 0, 359.9, MOTIF, 2 * scale)
    arc(draw, cx, cy, 34 * scale, 0, 359.9, MOTIF_SOFT, 1 * scale)
    for index in range(8):
        mid = -90.0 + index * 45.0
        pocket = [
            polar(cx, cy, 35 * scale, mid - 11.0),
            polar(cx, cy, 41 * scale, mid - 8.0),
            polar(cx, cy, 41 * scale, mid + 8.0),
            polar(cx, cy, 35 * scale, mid + 11.0),
        ]
        line(pocket, color=MOTIF_SOFT, width=1, closed=True)
    arc(draw, cx, cy, 22 * scale, 0, 359.9, MOTIF, 2 * scale)
    arc(draw, cx, cy, 10 * scale, 0, 359.9, MOTIF, 2 * scale)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "dashboard_gauge.png"
    scale = SS
    cx = CX * scale
    cy = CY * scale
    image = Image.new("RGBA", (W * scale, H * scale), BG)
    draw = ImageDraw.Draw(image)

    # Layer 1: the gray reference arc extends beyond the live 0..200 range.
    # Its extra tails fade out, like the supplied telemetry-gauge mockup.
    # Radius 146: sits just inside the card edges.
    reference_extension = 14.0
    faded_arc(draw, cx, cy, 146 * scale,
              ANGLE0 - reference_extension,
              ANGLE0 + SWEEP + reference_extension,
              OUTER, 7 * scale, 18.0)

    # Layer 2 is dynamic in LVGL: green arc centered on radius 129.

    # Layer 3: scale labels outside the tick lane. Six labels keep the 250 RPM
    # product range legible without crowding this 280px face. Radius 114:
    # operator noted 150/200/250 sat comfortably outside the tick marks.
    label_font = font(14 * scale)
    for index, value in enumerate((0, 50, 100, 150, 200, 250)):
        angle = ANGLE0 + SWEEP * index / 5
        x, y = polar(cx, cy, 114 * scale, angle)
        draw.text((x, y), str(value), font=label_font, fill=SCALE,
                  anchor="mm", stroke_width=0)

    # Layer 4: inward-facing major/minor ticks between labels and inner ring.
    slots = 25
    for index in range(slots + 1):
        angle = ANGLE0 + SWEEP * index / slots
        major = index % 5 == 0
        outer_r = 95 * scale
        inner_r = (82 if major else 87) * scale
        start = polar(cx, cy, inner_r, angle)
        end = polar(cx, cy, outer_r, angle)
        draw.line([start, end], fill=SCALE,
                  width=(2 if major else 1) * scale)

    # Layer 5: thin inner gray ring.
    arc(draw, cx, cy, 78 * scale, ANGLE0, ANGLE0 + SWEEP,
        INNER, 2 * scale)

    # Layer 6: static center motor detail, intentionally subtle.
    draw_motor_cutaway(draw, cx, cy, scale)

    image = image.resize((W, H), Image.Resampling.LANCZOS)
    image.save(out)
    print(f"wrote {out}: layered alpha mask, {W}x{H}")


if __name__ == "__main__":
    main()
