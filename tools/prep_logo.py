#!/usr/bin/env python3
"""Prepare the brand logo for the splash screen: resize to target width and
flatten the alpha channel onto the splash background color (the LVGL asset is
then an opaque INDEXED_8BIT image — cheap to draw, no per-pixel blending).

Usage: python prep_logo.py <Asset-3.png> <out.png>
"""
import sys

from PIL import Image

SPLASH_BG = (0xE1, 0xE4, 0xEA)  # must match the splash bg in create_splash() == COLOR_BG (screens.h)
TARGET_W = 380


def main():
    src = Image.open(sys.argv[1]).convert("RGBA")
    h = round(src.height * TARGET_W / src.width)
    img = src.resize((TARGET_W, h), Image.LANCZOS)
    flat = Image.new("RGB", img.size, SPLASH_BG)
    flat.paste(img, mask=img.split()[3])
    flat.save(sys.argv[2])
    print(f"wrote {sys.argv[2]} ({TARGET_W}x{h})")


if __name__ == "__main__":
    main()
