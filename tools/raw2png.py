#!/usr/bin/env python3
"""Convert a raw RGB565 framebuffer dump from sim_pc --shot into a PNG.

Usage: python raw2png.py <in.raw> <out.png> [width] [height]
"""
import sys

from PIL import Image

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    in_path, out_path = sys.argv[1], sys.argv[2]
    w = int(sys.argv[3]) if len(sys.argv) > 3 else 800
    h = int(sys.argv[4]) if len(sys.argv) > 4 else 480

    with open(in_path, "rb") as f:
        data = f.read()
    expected = w * h * 2
    if len(data) != expected:
        print(f"error: {in_path} is {len(data)} bytes, expected {expected}")
        return 2

    # RGB565 little-endian, red in the high bits -> Pillow raw mode 'BGR;16'
    img = Image.frombytes("RGB", (w, h), data, "raw", "BGR;16")
    img.save(out_path)
    print(f"wrote {out_path}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
