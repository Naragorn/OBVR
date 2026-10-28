#!/usr/bin/env python3
"""Oblivion Frame Fixes on against off, from frame-fixes-ab.txt runs.

Usage: frame-fixes-ab.py <label>=<run folder> [<label>=<run folder> ...]

For each run: the first world frame after the save's load (the script
clock's frame with the most script runs), the largest script frame within
three seconds after each house exit (marks "house" and "house-2"), and the
frames over two stretches - standing outside among the monks (5 s after
"house" to "outside-30") and on the meadow (10 s after "meadow" to "end"):
the mean and the 95th percentile of the frame interval and of the frame's
own time (end minus start) from the capture's frames.csv. The first run
given is the reference the others are compared with, in per cent.
"""

import csv
import re
import sys

TICKS_PER_MS = 10000.0  # the performance counter at 10 MHz (Proton, Windows)


def marks_of(log):
    return {m.group(1): int(m.group(2)) for m in re.finditer(r"HandScript: mark (\S+) \(.*?tick (\d+)\)", log)}


def script_frames(log):
    return [(int(m.group(3)), float(m.group(1)), int(m.group(2)))
            for m in re.finditer(r"Script clock: a frame's scripts took ([\d.]+) ms in (\d+) runs of \d+ scripts, "
                                 r"at tick (\d+)", log)]


def percentile(values, p):
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(p / 100.0 * len(ordered)))]


def stretch(frames, start, end):
    inside = [f for f in frames if start <= f[0] <= end]
    intervals = [f[1] for f in inside if f[1] > 0]
    own = [f[2] for f in inside]
    mean = lambda v: sum(v) / len(v) if v else 0.0
    return {"frames": len(inside), "interval": mean(intervals), "interval95": percentile(intervals, 95),
            "own": mean(own), "own95": percentile(own, 95)}


def measure(folder):
    log = open(folder + "/OBVR.log", errors="ignore").read()
    marks = marks_of(log)
    scripts = script_frames(log)
    frames = []
    with open(folder + "/OBVR-performance-1/frames.csv") as f:
        for row in csv.DictReader(f):
            start, end, interval = int(row["start_tick"]), int(row["end_tick"]), int(row["interval_tick"])
            frames.append((start, interval / TICKS_PER_MS, (end - start) / TICKS_PER_MS))
    out = {}
    load = max(scripts, key=lambda s: s[2]) if scripts else (0, 0.0, 0)
    out["load frame ms"] = load[1]
    for name in ("house", "house-2"):
        at = marks.get(name)
        near = [s[1] for s in scripts if at is not None and at <= s[0] <= at + 3 * 1000 * TICKS_PER_MS]
        out["exit %s ms" % name] = max(near) if near else 0.0
    second = 1000 * TICKS_PER_MS
    if "house" in marks and "outside-30" in marks:
        s = stretch(frames, marks["house"] + 5 * second, marks["outside-30"])
        out.update({"monks " + k: v for k, v in s.items()})
    if "meadow" in marks and "end" in marks:
        s = stretch(frames, marks["meadow"] + 10 * second, marks["end"])
        out.update({"meadow " + k: v for k, v in s.items()})
    return out


def main(argv):
    runs = []
    for arg in argv:
        label, _, folder = arg.partition("=")
        runs.append((label, measure(folder)))
    if not runs:
        print(__doc__)
        return 2
    keys = list(runs[0][1].keys())
    print("| measure | " + " | ".join(label for label, _ in runs) + " |")
    print("|---|" + "---|" * len(runs))
    for key in keys:
        ref = runs[0][1].get(key, 0.0)
        cells = []
        for i, (_, m) in enumerate(runs):
            v = m.get(key, 0.0)
            text = "%d" % v if key.endswith("frames") else "%.2f" % v
            if i > 0 and ref and not key.endswith("frames"):
                text += " (%+.0f %%)" % (100.0 * (v - ref) / ref)
            cells.append(text)
        print("| %s | %s |" % (key, " | ".join(cells)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
