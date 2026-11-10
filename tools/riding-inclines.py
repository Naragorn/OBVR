#!/usr/bin/env python3
"""A horse-ride-incline-<slope>.txt run read per ride: the horse node's pitch and roll,
how far the horse went, and how far the eyes over the saddle stand off where
they stand on the level - forward and sideways in the heading's frame (the
riding base line's "the eyes over the seat"). Upright over the saddle that
stays within the stride's bob on every incline; turned with the whole body
it grows with the pitch, (eyes - saddle) sin(pitch) back uphill.

    python3 tools/riding-inclines.py artifacts/hand-script/horse-ride-incline-steep/<run>/OBVR.log --uphill 201
        [--most-median 5] [--most-p95 10] [--least-moved 80] [--least-frames 30]

The rides are the frames after the "rides" mark, sorted by the horse's own
heading against the slope's uphill (a mounted horse takes no setangle, so the
script turns it on the left stick and the heading it reached decides): up,
down, across with the slope rising on the right or the left, diagonally up
and down. The level the offsets are read against is the frames whose pitch
and roll are both under 1.5 degrees (the rides across).

Exit 1 when a ride stood off over the bounds or did not get going (a horse
stopped by a tree or a rock counts as not ridden, not as a pass).
"""
import argparse
import math
import re
import statistics
import sys

BASE = re.compile(
    r"Camera: riding base frame (\d+) - .*?the horse at (\S+) (\S+) (\S+) heading (\S+), "
    r"the node's pitch (\S+) deg, roll (\S+) deg, the seat (read|not read) right (\S+) forward (\S+) up (\S+), "
    r"upright (\S+), the eyes over the seat right (\S+) forward (\S+) up ([-0-9.]+)")
MARK = re.compile(r"HandScript: mark (\S+)")


DIRECTIONS = (("up", 0.0), ("diag-up-right", -45.0), ("across-right-up", -90.0), ("diag-down-right", -135.0),
              ("down", 180.0), ("diag-down-left", 135.0), ("across-left-up", 90.0), ("diag-up-left", 45.0))


def direction(heading_degrees, uphill):
    """The ride's name by the heading against uphill, to the nearest eighth."""
    rel = (heading_degrees - uphill + 540.0) % 360.0 - 180.0
    best = min(DIRECTIONS, key=lambda d: abs((rel - d[1] + 540.0) % 360.0 - 180.0))
    return best[0]


def rides(lines, uphill):
    """The base lines after the "rides" mark, by direction."""
    out = {}
    on = False
    for line in lines:
        m = MARK.search(line)
        if m:
            if m.group(1) == "rides":
                on = True
            elif m.group(1) == "rides-done":
                on = False
            continue
        b = BASE.search(line)
        if b and on:
            g = b.groups()
            r = dict(at=(float(g[1]), float(g[2]), float(g[3])), heading=math.degrees(float(g[4])) % 360.0,
                     pitch=float(g[5]), roll=float(g[6]), seat=g[7] == "read", upright=float(g[11]),
                     right=float(g[12]), forward=float(g[13]), up=float(g[14]))
            out.setdefault(direction(r["heading"], uphill), []).append(r)
    return out


def percentile(values, share):
    s = sorted(values)
    return s[min(len(s) - 1, int(round(share * (len(s) - 1))))]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("log")
    p.add_argument("--most-median", type=float, default=5.0)
    p.add_argument("--most-p95", type=float, default=10.0)
    p.add_argument("--least-moved", type=float, default=80.0)
    p.add_argument("--least-frames", type=int, default=30)
    p.add_argument("--uphill", type=float, required=True, help="the slope's uphill heading, degrees")
    a = p.parse_args()
    with open(a.log, errors="replace") as f:
        by = rides(f.readlines(), a.uphill)
    order = [d[0] for d in DIRECTIONS if d[0] in by]
    level = [r for rs in by.values() for r in rs if abs(r["pitch"]) < 1.5 and abs(r["roll"]) < 1.5]
    if not level:
        print("no level frames among the rides (pitch and roll under 1.5 degrees)")
        return 1
    ref_r = statistics.median(r["right"] for r in level)
    ref_f = statistics.median(r["forward"] for r in level)
    print(f"the level: the eyes over the seat right {ref_r:.1f} forward {ref_f:.1f} ({len(level)} frames)")
    print(f"{'ride':22} {'frames':>6} {'moved':>6} {'pitch':>12} {'roll':>12} {'upright':>7} "
          f"{'off med':>7} {'off p95':>7} {'back':>6} {'side':>6}  verdict")
    failed = 0
    for name in order:
        rs = by[name]
        if not rs:
            print(f"{name:22} no frames - the horse did not move")
            failed += 1
            continue
        moved = sum(math.dist(rs[i]["at"][:2], rs[i + 1]["at"][:2]) for i in range(len(rs) - 1))
        pitches = [r["pitch"] for r in rs]
        rolls = [r["roll"] for r in rs]
        offs = [math.hypot(r["right"] - ref_r, r["forward"] - ref_f) for r in rs]
        back = statistics.median(r["forward"] - ref_f for r in rs)
        side = statistics.median(r["right"] - ref_r for r in rs)
        med = statistics.median(offs)
        p95 = percentile(offs, 0.95)
        verdict = "PASS"
        if len(rs) < a.least_frames:
            verdict = f"TOO FEW FRAMES ({len(rs)})"
        elif moved < a.least_moved:
            verdict = f"NOT RIDDEN (moved {moved:.0f})"
        elif med > a.most_median or p95 > a.most_p95:
            verdict = "FAIL"
        if verdict != "PASS":
            failed += 1
        print(f"{name:22} {len(rs):6d} {moved:6.0f} {min(pitches):5.1f}..{max(pitches):5.1f} "
              f"{min(rolls):5.1f}..{max(rolls):5.1f} {statistics.median(r['upright'] for r in rs):7.2f} "
              f"{med:7.1f} {p95:7.1f} {back:6.1f} {side:6.1f}  {verdict}")
    print(f"{len(order) - failed} of {len(order)} rides within the bounds "
          f"(median {a.most_median}, p95 {a.most_p95} units off the level, moved at least {a.least_moved})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
