#!/usr/bin/env python3
"""Procedurally render the static gauge dial face (ticks, scale numbers, hub).

The dynamic parts (colored arc, needle, RPM digits) are LVGL widgets drawn on
top of this image at runtime. Geometry mirrors the React prototype's ArcGauge
(R=78 @ 200px viewbox) scaled 1.3x, then bumped another ~1.19x for the bigger
"to hon" gauge treatment (user request: the dial felt small next to the new
20px+ text scale), so the LVGL arc widget must use:
  - image size   318 x 276, dial center at (159, 164)
  - track radius 120, track width 16  ->  arc object 256x256, arc_width 16
  - needle spans 76 to 105 from center, stopping just short of the track
    (dynamic lv_line, see dash_update_needle() in screens.c)

Usage: python gen_dial.py [out.png]
"""
import math
import sys

from PIL import Image, ImageDraw, ImageFont

# Logical geometry (final image pixels)
W, H = 318, 276
CX, CY = 159.0, 164.0
R = 120.0            # track centerline radius
TRACK_W = 16.0
ANGLE0 = 135.0       # PIL degrees: 0 = 3 o'clock, clockwise (y-down)
SWEEP = 270.0
SS = 4               # supersample factor

# Must match UI_MAX_RPM in screens.h (this motor's real ceiling). NUM_MAJOR
# is the number of major-tick INTERVALS (5 -> 6 labels: 0,100,200,300,400,500);
# pick a divisor of MAX_RPM that lands on round numbers.
MAX_RPM = 500
NUM_MAJOR = 5
MINOR_PER_MAJOR = 3  # matches the original density: 2 minor ticks between majors

# Palette — must track the Apple-derived tokens in screens.h/screens.c.
# Re-run this (and tools/build_assets.bat) any time COLOR_BG/COLOR_BORDER/
# COLOR_TEXT_VL/COLOR_TEXT_DIM change, or the image's baked-in background
# will show a visible seam against the live COLOR_BG behind it.
BG = (0xF2, 0xF2, 0xF7)         # = COLOR_BG (screens.h)
TRACK = (0xD2, 0xD2, 0xD7)      # = COLOR_BORDER (screens.h)
TICK_MINOR = (0xD2, 0xD2, 0xD7) # = COLOR_BORDER — subtle, same tone as the track
TICK_MAJOR = (0x7C, 0x7C, 0x82) # = COLOR_TEXT_VL (screens.h) — darkened +10% pass
LABEL = (0x7C, 0x7C, 0x82)      # = COLOR_TEXT_VL — same weight as its tick
HUB = (0xFF, 0xFF, 0xFF)        # = COLOR_CARD_BG (screens.h) — white, like a card
HUB_RING = (0xD2, 0xD2, 0xD7)   # = COLOR_BORDER


def polar(cx, cy, r, deg):
    rad = math.radians(deg)
    return (cx + r * math.cos(rad), cy + r * math.sin(rad))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "dial.png"

    w, h = W * SS, H * SS
    cx, cy, r = CX * SS, CY * SS, R * SS
    img = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(img)

    # Track arc with round caps
    tw = TRACK_W * SS
    bbox = [cx - r - tw / 2, cy - r - tw / 2, cx + r + tw / 2, cy + r + tw / 2]
    d.arc(bbox, ANGLE0, ANGLE0 + SWEEP, fill=TRACK, width=int(tw))
    for deg in (ANGLE0, ANGLE0 + SWEEP):
        ex, ey = polar(cx, cy, r, deg)
        d.ellipse([ex - tw / 2, ey - tw / 2, ex + tw / 2, ey + tw / 2], fill=TRACK)

    # Minor ticks: MINOR_PER_MAJOR subdivisions per major interval, skipping
    # the positions that land exactly on a major tick.
    total_minor_slots = NUM_MAJOR * MINOR_PER_MAJOR
    for i in range(total_minor_slots + 1):
        if i % MINOR_PER_MAJOR == 0:
            continue
        deg = ANGLE0 + SWEEP * i / total_minor_slots
        a = polar(cx, cy, r + (TRACK_W / 2 + 4) * SS, deg)
        b = polar(cx, cy, r + (TRACK_W / 2 + 1) * SS, deg)
        d.line([a, b], fill=TICK_MINOR, width=1 * SS)

    # Major ticks + labels: NUM_MAJOR+1 round-number labels spanning 0..MAX_RPM
    try:
        font = ImageFont.truetype("tools/ttf/JetBrainsMono-Regular.ttf", 13 * SS)
    except OSError:
        font = ImageFont.truetype("ttf/JetBrainsMono-Regular.ttf", 13 * SS)
    for i in range(NUM_MAJOR + 1):
        deg = ANGLE0 + SWEEP * i / NUM_MAJOR
        a = polar(cx, cy, r + (TRACK_W / 2 + 6) * SS, deg)
        b = polar(cx, cy, r + (TRACK_W / 2 - 1) * SS, deg)
        d.line([a, b], fill=TICK_MAJOR, width=2 * SS)
        lx, ly = polar(cx, cy, r + (TRACK_W / 2 + 16) * SS, deg)
        text = str(round(MAX_RPM * i / NUM_MAJOR))
        d.text((lx, ly), text, fill=LABEL, font=font, anchor="mm")

    # Needle hub (base ring; the colored center dot is a dynamic widget)
    hub_r = 10 * SS
    d.ellipse([cx - hub_r, cy - hub_r, cx + hub_r, cy + hub_r], fill=HUB, outline=HUB_RING, width=1 * SS)

    img = img.resize((W, H), Image.LANCZOS)
    img.save(out)
    print(f"wrote {out} ({W}x{H})")


if __name__ == "__main__":
    main()
