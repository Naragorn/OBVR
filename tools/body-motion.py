#!/usr/bin/env python3
"""The body's motion window by window, from Debug.BodyTrace (game/BodyIk.cpp).

Reads a run's OBVR.log (or the run's folder) and, between each hand script's
marks, says how far the camera moved from one frame to the next (camstep: the
largest; with the head held still that is the view's own wobble), how far the
body's root went (root), the torso's yaw against the head (yaw min..max), and
each foot in the world (the root plus the foot against it): its largest move
in a frame (Rmove, Lmove), how many separate steps it took (runs of frames
moving more than --moving units; Rsteps, Lsteps), and how many "foot steps
after the body" lines the window has (said).

  tools/body-motion.py <run> [--gate WINDOW:METRIC<=VALUE] [--gate WINDOW:METRIC>=VALUE]
                             [--facing MARK,MARK,...<=DEGREES]

A gate's WINDOW is a mark's name, or the start of names with a * after it
(draw* matches draw1, draw2...). METRIC is one of camstep, root, yawspan, Rmove, Lmove, Rsteps,
Lsteps, steps (both feet's), said. A failed gate prints FAIL and exits 1.

--facing reads the "Body: at the mark - the body faces" lines: the span of
the body's world yaw over the marks named (for the horse: the body faces the
horse whichever way the head looks) must be within DEGREES.
"""

from __future__ import annotations

import argparse
import math
import os
import re
import sys

MARK = re.compile(r"HandScript: mark (\S+)")
TRACE = re.compile(r"Body trace: f=(\d+) cam (\S+) (\S+) (\S+) root (\S+) (\S+) (\S+) ")
YAW = re.compile(r" yaw (\S+) chest ")
LEG = r"%s a \S+ \S+ \S+ f (\S+) (\S+) (\S+) k "
LEGS = re.compile(r"\| legs " + LEG % "R" + r".*\| " + LEG % "L")
SAID = re.compile(r"Body IK: the (right|left) foot steps after the body")
FACES = re.compile(r"Body: at the mark - the body faces (\S+) degrees, the head (\S+)")
FACING = re.compile(r"^([^<]+)<=(-?[\d.]+)$")
GATE = re.compile(r"^([^:]+):(\w+)(<=|>=)(-?[\d.]+)$")


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def parse(path, faces):
    windows = [("start", [], [0])]
    with open(path, encoding="utf-8", errors="replace") as log:
        for line in log:
            mark = MARK.search(line)
            if mark:
                windows.append((mark.group(1), [], [0]))
                continue
            face = FACES.search(line)
            if face:
                faces[windows[-1][0]] = (float(face.group(1)), float(face.group(2)))
                continue
            if SAID.search(line):
                windows[-1][2][0] += 1
                continue
            trace = TRACE.search(line)
            legs = LEGS.search(line)
            yaw = YAW.search(line)
            if not (trace and legs and yaw):
                continue
            g = [float(x) for x in trace.groups()[1:]]
            leg = [float(x) for x in legs.groups()]
            root = tuple(g[3:6])
            windows[-1][1].append({
                "cam": tuple(g[0:3]),
                "root": root,
                "yaw": float(yaw.group(1)),
                "R": tuple(root[i] + leg[i] for i in range(3)),
                "L": tuple(root[i] + leg[3 + i] for i in range(3)),
            })
    return windows


def steps_of(moves, moving):
    runs = 0
    inside = False
    for m in moves:
        if m > moving and not inside:
            runs += 1
        inside = m > moving
    return runs


def measure(rows, said, moving):
    out = {"frames": len(rows), "said": said}
    if len(rows) < 2:
        out.update({"camstep": 0.0, "root": 0.0, "yawspan": 0.0, "Rmove": 0.0, "Lmove": 0.0, "Rsteps": 0,
                    "Lsteps": 0, "steps": 0, "yawmin": 0.0, "yawmax": 0.0})
        return out
    out["camstep"] = max(dist(rows[i]["cam"], rows[i - 1]["cam"]) for i in range(1, len(rows)))
    out["root"] = dist(rows[-1]["root"], rows[0]["root"])
    yaws = [r["yaw"] for r in rows]
    out["yawmin"], out["yawmax"] = min(yaws), max(yaws)
    out["yawspan"] = out["yawmax"] - out["yawmin"]
    for side in ("R", "L"):
        moves = [dist(rows[i][side], rows[i - 1][side]) for i in range(1, len(rows))]
        out[side + "move"] = max(moves)
        out[side + "steps"] = steps_of(moves, moving)
    out["steps"] = out["Rsteps"] + out["Lsteps"]
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("run")
    parser.add_argument("--moving", type=float, default=0.3, help="units a frame a foot counts as moving")
    parser.add_argument("--gate", action="append", default=[])
    parser.add_argument("--facing", action="append", default=[])
    args = parser.parse_args()
    path = os.path.join(args.run, "OBVR.log") if os.path.isdir(args.run) else args.run
    faces = {}
    windows = parse(path, faces)
    print(f"{'window':<16}{'frames':>7}{'camstep':>9}{'root':>8}{'yaw min..max':>16}{'Rmove':>7}{'Lmove':>7}"
          f"{'Rsteps':>7}{'Lsteps':>7}{'said':>6}")
    results = []
    for name, rows, said in windows:
        m = measure(rows, said[0], args.moving)
        results.append((name, m))
        print(f"{name:<16}{m['frames']:>7}{m['camstep']:>9.3f}{m['root']:>8.2f}"
              f"{m['yawmin']:>8.1f}..{m['yawmax']:<6.1f}{m['Rmove']:>7.2f}{m['Lmove']:>7.2f}"
              f"{m['Rsteps']:>7}{m['Lsteps']:>7}{m['said']:>6}")
    for name, (body, head) in faces.items():
        print(f"mark {name}: the body faces {body:.1f}, the head {head:.1f}")
    failed = 0
    for facing in args.facing:
        f = FACING.match(facing)
        if not f:
            print(f"bad facing: {facing}", file=sys.stderr)
            return 2
        names = f.group(1).split(",")
        missing = [n for n in names if n not in faces]
        if missing:
            print(f"FAIL  facing {facing}: no body line at {', '.join(missing)}")
            failed += 1
            continue
        first = faces[names[0]][0]
        offs = [((faces[n][0] - first + 180.0) % 360.0) - 180.0 for n in names]
        span = max(offs) - min(offs)
        ok = span <= float(f.group(2))
        print(f"{'ok  ' if ok else 'FAIL'}  facing {f.group(1)}: the body's yaw spans {span:.1f} degrees")
        failed += 0 if ok else 1
    for gate in args.gate:
        g = GATE.match(gate)
        if not g:
            print(f"bad gate: {gate}", file=sys.stderr)
            return 2
        prefix, metric, op, value = g.group(1), g.group(2), g.group(3), float(g.group(4))
        if prefix.endswith("*"):
            hit = [(n, m) for n, m in results if n.startswith(prefix[:-1]) and m["frames"] > 1]
        else:
            hit = [(n, m) for n, m in results if n == prefix and m["frames"] > 1]
        if not hit:
            print(f"FAIL  {gate}: no window {prefix}* with trace frames")
            failed += 1
            continue
        for name, m in hit:
            got = m[metric]
            ok = got <= value if op == "<=" else got >= value
            print(f"{'ok  ' if ok else 'FAIL'}  {name}: {metric} {got:.3f} {op} {value}")
            failed += 0 if ok else 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
