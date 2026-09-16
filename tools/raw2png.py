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

    # Exact RGB565 bit-replication expansion: (v<<3)|(v>>2) for R/B, (v<<2)|(v>>4) for G
    lut = bytearray(65536 * 3)
    for v in range(65536):
        r5 = (v >> 11) & 0x1F
        g6 = (v >> 5) & 0x3F
        b5 = v & 0x1F
        lut[v * 3]     = (r5 << 3) | (r5 >> 2)
        lut[v * 3 + 1] = (g6 << 2) | (g6 >> 4)
        lut[v * 3 + 2] = (b5 << 3) | (b5 >> 2)

    import struct
    vals = struct.unpack(f"<{len(data)//2}H", data)
    rgb_bytes = bytearray(len(vals) * 3)
    for i, v in enumerate(vals):
        idx = v * 3
        rgb_bytes[i*3 : i*3+3] = lut[idx : idx+3]

    img = Image.frombytes("RGB", (w, h), bytes(rgb_bytes))
    img.save(out_path)
    print(f"wrote {out_path}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
