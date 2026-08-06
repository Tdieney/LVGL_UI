#!/usr/bin/env python3
"""Draw the BLDC motor cross-section watermark (outrunner face: housing ring +
stator teeth + hub + shaft) as a white alpha mask on transparent — the "E2"
motif chosen for the Dashboard speed tile. Rendered ~120px, supersampled.
build_assets.bat runs png2lvgl.py --cf alpha_4 on it so it recolors (ink) and
draws faint (low opa) behind the RPM number, like the other alpha icons.

Usage: python gen_motor_wm.py <out.png>
"""
import math
import sys

from PIL import Image, ImageDraw

N = 175       # final size (px) — native; tucked in the Dashboard bottom-right corner (E2-style bleed)
SS = 4        # supersample
C = N * SS
INK = (255, 255, 255, 255)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "motor_wm.png"
    img = Image.new("RGBA", (C, C), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = cy = C / 2.0
    w = C * 0.045

    def ring(r):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=INK, width=int(round(w)))

    ring(C * 0.44)   # housing
    ring(C * 0.17)   # hub

    r1, r2, tw = C * 0.235, C * 0.345, int(round(C * 0.03))  # stator teeth
    for k in range(12):
        a = k * math.pi / 6.0
        d.line([cx + r1 * math.cos(a), cy + r1 * math.sin(a),
                cx + r2 * math.cos(a), cy + r2 * math.sin(a)], fill=INK, width=tw)

    rs = C * 0.055   # shaft
    d.ellipse([cx - rs, cy - rs, cx + rs, cy + rs], fill=INK)

    img = img.resize((N, N), Image.LANCZOS)
    img.save(out)
    print(f"wrote {out} ({N}x{N})")


if __name__ == "__main__":
    main()
