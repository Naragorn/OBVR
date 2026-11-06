#!/usr/bin/env python3
"""Reads the legs out of a run's "Body trace:" lines (Debug.BodyTrace;
tools/hand-scripts/draw-leg-flicker.txt), window by window between the hand
script's marks.

Per window it prints the frames, and for each foot and knee (against the
root, as the frame leaves them) the largest step from one frame to the next,
the largest flicker (the second difference |p[i+1] - 2 p[i] + p[i-1]|: a
steady move cancels in it, a jump there and back does not) and how many
frames stepped further than --jump units; the same for the feet as the
animation left them; how often each foot's solve switched on or off, how
often the trunk's hold switched, the range of the let-down, and how many
frames lost a bone write before the draw. With --frames it lists every frame
whose legs stepped further than --jump.

The gates (each answers 1 when broken): --most-step and --most-flick for the
feet in the windows that draw or sheathe (named draw... and sheathe...);
--most-still for the feet and the knees in every other window after the
first mark, where the player stands and nothing is asked of the legs. The
knees through a draw are printed and not gated: the view itself steps there
(docs/player-body-spec.md).

Usage: tools/draw-leg-flicker.py <run folder or OBVR.log> [--jump 0.5] [--frames]
       [--most-step units] [--most-flick units] [--most-still units]
"""

import argparse
import math
import os
import re
import sys

MARK = re.compile(r"^HandScript: mark (\S+)")
FRAME = re.compile(r"^Body trace: f=(\d+) ")
CAM = re.compile(r" cam (\S+) (\S+) (\S+) root (\S+) (\S+) (\S+) ")
LOW = re.compile(r" low (\S+) lift (\S+) yaw (\S+) chest (\S+) held (\d+)/(\d+) fp (\d+)/(\d+) trunk (\d)")
FEET = re.compile(r" feet (-?[\d.]+)(s?) (-?[\d.]+)(s?) \|")
LEG = (r"%s a (\S+) (\S+) (\S+) f (\S+) (\S+) (\S+) k (\S+) (\S+) (\S+)(?: h (\S+) (\S+) (\S+))?")
LEGS = re.compile(r"\| legs " + LEG % "R" + r" \| " + LEG % "L")

PARTS = ("R foot", "R knee", "L foot", "L knee", "R anim", "L anim")


def parse(path):
    frames = []
    window = "start"
    with open(path, encoding="utf-8", errors="replace") as log:
        for line in log:
            mark = MARK.match(line)
            if mark:
                window = mark.group(1)
                continue
            if not FRAME.match(line):
                continue
            cam, low, feet, legs = CAM.search(line), LOW.search(line), FEET.search(line), LEGS.search(line)
            if not (cam and low and feet and legs):
                continue
            g = [float(x) if x is not None else 0.0 for x in legs.groups()]
            v = g[0:9] + g[12:21]
            hips = (tuple(g[9:12]), tuple(g[21:24]))
            c = [float(x) for x in cam.groups()]
            frames.append({
                "window": window,
                "f": int(FRAME.match(line).group(1)),
                "rootcam": (c[3] - c[0], c[4] - c[1], c[5] - c[2]),
                "low": float(low.group(1)),
                "lift": float(low.group(2)),
                "yaw": float(low.group(3)),
                "lost": int(low.group(6)) - int(low.group(5)),
                "trunk": low.group(9) == "1",
                "delta": (float(feet.group(1)), float(feet.group(3))),
                "solved": (feet.group(2) == "s", feet.group(4) == "s"),
                "R anim": tuple(v[0:3]), "R foot": tuple(v[3:6]), "R knee": tuple(v[6:9]),
                "L anim": tuple(v[9:12]), "L foot": tuple(v[12:15]), "L knee": tuple(v[15:18]),
                "R hip": hips[0], "L hip": hips[1],
            })
    return frames


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def second(a, b, c):
    return math.sqrt(sum((x - 2 * y + z) ** 2 for x, y, z in zip(a, b, c)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("run")
    parser.add_argument("--jump", type=float, default=0.5)
    parser.add_argument("--frames", action="store_true")
    parser.add_argument("--most-step", type=float, default=None)
    parser.add_argument("--most-flick", type=float, default=None)
    parser.add_argument("--most-still", type=float, default=None)
    args = parser.parse_args()
    path = os.path.join(args.run, "OBVR.log") if os.path.isdir(args.run) else args.run
    frames = parse(path)
    if not frames:
        print("no Body trace lines with legs", file=sys.stderr)
        return 1
    windows = []
    for frame in frames:
        if not windows or windows[-1][0] != frame["window"]:
            windows.append((frame["window"], []))
        windows[-1][1].append(frame)
    print("units; step = largest move from one frame to the next, flick = largest second difference, n = frames past "
          f"{args.jump}")
    head = f"{'window':<10}{'frames':>7}"
    for part in PARTS:
        head += f"  {part + ' step/flick/n':>22}"
    head += f"  {'solveR':>6} {'solveL':>6} {'trunk':>5} {'low min..max':>14} {'lost':>5} {'rootcam':>8}"
    print(head)
    worst_step = 0.0
    worst_flick = 0.0
    worst_knee = 0.0
    worst_still = 0.0
    for name, rows in windows:
        line = f"{name:<10}{len(rows):>7}"
        moving = name.startswith("draw") or name.startswith("sheathe")
        for part in PARTS:
            steps = [dist(rows[i][part], rows[i - 1][part]) for i in range(1, len(rows))]
            flicks = [second(rows[i + 1][part], rows[i][part], rows[i - 1][part]) for i in range(1, len(rows) - 1)]
            jumps = sum(1 for s in steps if s > args.jump)
            if moving and part in ("R foot", "L foot"):
                worst_step = max(worst_step, max(steps, default=0))
                worst_flick = max(worst_flick, max(flicks, default=0))
            elif moving and part in ("R knee", "L knee"):
                worst_knee = max(worst_knee, max(steps, default=0))
            elif name != "start" and part in PARTS[:4]:
                worst_still = max(worst_still, max(steps, default=0))
            line += f"  {max(steps, default=0):>8.2f}/{max(flicks, default=0):>6.2f}/{jumps:>5}"
        flips = [sum(1 for i in range(1, len(rows)) if rows[i]["solved"][s] != rows[i - 1]["solved"][s])
                 for s in (0, 1)]
        trunk = sum(1 for i in range(1, len(rows)) if rows[i]["trunk"] != rows[i - 1]["trunk"])
        lows = [r["low"] for r in rows]
        lost = sum(1 for r in rows if r["lost"] > 0)
        rootcam = max((dist(rows[i]["rootcam"], rows[i - 1]["rootcam"]) for i in range(1, len(rows))), default=0)
        line += f"  {flips[0]:>6} {flips[1]:>6} {trunk:>5} {min(lows):>6.2f}..{max(lows):<6.2f} {lost:>5} {rootcam:>8.2f}"
        print(line)
    if args.frames:
        print()
        for i in range(1, len(frames)):
            a, b = frames[i - 1], frames[i]
            moved = {part: dist(a[part], b[part]) for part in PARTS[:4]}
            if max(moved.values()) <= args.jump:
                continue
            cells = " ".join(f"{part} {moved[part]:.2f}" for part in PARTS[:4])
            print(f"{b['window']:<10} f={b['f']:<6} {cells} | anim R {dist(a['R anim'], b['R anim']):.2f} "
                  f"L {dist(a['L anim'], b['L anim']):.2f} | delta {b['delta'][0]:.2f}{'s' if b['solved'][0] else ''} "
                  f"{b['delta'][1]:.2f}{'s' if b['solved'][1] else ''} | low {b['low']:.2f} trunk {int(b['trunk'])} "
                  f"lost {b['lost']} | R foot {b['R foot'][0]:.1f} {b['R foot'][1]:.1f} {b['R foot'][2]:.1f} "
                  f"L foot {b['L foot'][0]:.1f} {b['L foot'][1]:.1f} {b['L foot'][2]:.1f} | hips moved "
                  f"{dist(a['R hip'], b['R hip']):.2f} {dist(a['L hip'], b['L hip']):.2f}")
    failed = False
    print(f"drawing and sheathing: a foot's largest step {worst_step:.2f}, largest flicker {worst_flick:.2f}; "
          f"a knee's largest step {worst_knee:.2f}; standing: a foot's or knee's largest step {worst_still:.2f}")
    for name, worst, most in (("step", worst_step, args.most_step), ("flick", worst_flick, args.most_flick),
                              ("still", worst_still, args.most_still)):
        if most is not None and worst > most:
            failed = True
            print(f"--most-{name}: {worst:.2f} is past {most:.2f}")
    if args.most_step is not None or args.most_flick is not None or args.most_still is not None:
        print("FAIL" if failed else "PASS")
    return 1 if failed else 0

if __name__ == "__main__":
    sys.exit(main())
