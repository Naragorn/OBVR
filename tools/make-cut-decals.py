#!/usr/bin/env python3
"""Writes the cut marks' decal textures (game/CutMarkLogic.h CutMarkTexture)
to assets/textures/Effects/OBVR/: a slash along the texture's u across its
middle, as uncompressed 32-bit DDS with alpha (the engine's decals blend by
it). Skin a red wound, cloth a thin dark slit with frayed edges, leather a
dark brown gash. Small and generated, so they can be remade.

Usage: tools/make-cut-decals.py [--size 64]
"""

import argparse
import math
import os
import random
import struct

KINDS = {
    # name: (core rgb, rim rgb, half-thickness in pixels at the middle, rim width, fray)
    "CutSkin": ((70, 6, 6), (150, 25, 22), 2.6, 3.0, 0.0),
    "CutCloth": ((22, 18, 16), (95, 85, 75), 1.3, 1.6, 1.2),
    "CutLeather": ((30, 16, 8), (75, 45, 25), 2.0, 2.4, 0.4),
}


def dds_header(size):
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x8  # caps, height, width, pixelformat, pitch
    pf = struct.pack("<II4sIIIII", 32, 0x41, b"\0\0\0\0", 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    header = struct.pack("<IIIIIII", 124, flags, size, size, size * 4, 0, 0) + b"\0" * 44 + pf
    header += struct.pack("<IIIII", 0x1000, 0, 0, 0, 0)
    return b"DDS " + header


def pixels(size, core, rim, half, rim_width, fray, seed):
    rnd = random.Random(seed)
    jag = [rnd.uniform(-0.6, 0.6) for _ in range(size)]
    out = bytearray()
    for y in range(size):
        for x in range(size):
            u = (x + 0.5) / size
            if u < 0.08 or u > 0.92:
                out += bytes((0, 0, 0, 0))
                continue
            taper = math.sin((u - 0.08) / 0.84 * math.pi)  # thin at both ends
            centre = size / 2 + jag[x] * (0.5 + fray)
            d = abs((y + 0.5) - centre)
            t = half * taper
            if d <= t:
                r, g, b = core
                a = 235
            elif d <= t + rim_width * (0.4 + taper):
                k = (d - t) / (rim_width * (0.4 + taper))
                r, g, b = (int(rim[i] * (1 - k) + core[i] * 0.0) for i in range(3))
                a = int(200 * (1 - k) * (1 - fray * 0.3 * rnd.random()))
            else:
                r = g = b = a = 0
            out += bytes((b, g, r, max(0, min(255, a))))
    return bytes(out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=64)
    args = parser.parse_args()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    folder = os.path.join(root, "assets", "textures", "Effects", "OBVR")
    os.makedirs(folder, exist_ok=True)
    for i, (name, (core, rim, half, rim_width, fray)) in enumerate(KINDS.items()):
        path = os.path.join(folder, name + ".dds")
        with open(path, "wb") as f:
            f.write(dds_header(args.size))
            f.write(pixels(args.size, core, rim, half * args.size / 64, rim_width * args.size / 64, fray, 7 + i))
        print(path)


if __name__ == "__main__":
    main()
