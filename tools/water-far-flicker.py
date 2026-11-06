#!/usr/bin/env python3
"""Reads the water-far-flicker runs (tools/hand-scripts/water-far-flicker-*.txt).

For each run and each mark (still, micro) it takes the right eye's thirty
frames in a row and prints, per band of screen rows, the mean frame-to-frame
change (first difference) and the mean flicker (the second difference,
|f[i+1] - 2 f[i] + f[i-1]|: a steady drift of the waves or the view cancels
in it, a value that jumps back and forth does not). Grey 0..255.

Usage: tools/water-far-flicker.py <run folder> [<run folder> ...]
       [--bands name:top:bottom,...]
"""

import argparse
import os
import sys

import numpy as np
from PIL import Image

DEFAULT_BANDS = "land:300:440,far:470:600,mid:600:800,near:800:1280"


def frames(run, mark):
    out = []
    for i in range(30):
        path = os.path.join(run, f"OBVR-Eye-Right-{mark}-f{i:02d}.png")
        if not os.path.exists(path):
            break
        out.append(np.asarray(Image.open(path).convert("L"), dtype=np.float32))
    return np.stack(out) if out else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("runs", nargs="+")
    parser.add_argument("--bands", default=DEFAULT_BANDS)
    args = parser.parse_args()
    bands = []
    for item in args.bands.split(","):
        name, top, bottom = item.split(":")
        bands.append((name, int(top), int(bottom)))
    header = "run/mark".ljust(34) + "".join(f"{n:>16}" for n, _, _ in bands)
    print(header)
    print(" " * 34 + "".join(f"{'change flicker':>16}" for _ in bands))
    for run in args.runs:
        name = os.path.basename(os.path.dirname(run.rstrip("/"))).replace("water-far-flicker-", "")
        for mark in ("still", "micro"):
            f = frames(run, mark)
            if f is None or len(f) < 3:
                print(f"{name}/{mark}: no frames", file=sys.stderr)
                continue
            first = np.abs(np.diff(f, axis=0))
            second = np.abs(f[2:] - 2 * f[1:-1] + f[:-2])
            cells = ""
            for _, top, bottom in bands:
                cells += f"{first[:, top:bottom].mean():8.2f}{second[:, top:bottom].mean():8.2f}"
            print(f"{name}/{mark}".ljust(34) + cells)


if __name__ == "__main__":
    main()
