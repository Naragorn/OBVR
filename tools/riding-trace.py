#!/usr/bin/env python3
"""The riding trace of a horse-ride run read per phase (the marks): the horse
node's pitch, the engine camera's offset over the horse in the yaw-only frame and in
the node's full frame, the settled saddle of the riding base and how far the base
stands off the engine's camera (the eyes out of the saddle). Written 2026-10-23 for
horse-ride-slope.txt (controls-spec.md, the horse's tilt on a slope).

    python3 tools/riding-trace.py artifacts/hand-script/horse-ride-slope/<run>/OBVR.log
"""
import math
import re
import sys

path = sys.argv[1]
trace_re = re.compile(
    r"Camera: riding trace frame (\d+) - the engine's camera local (\S+) (\S+) (\S+), over the horse (\S+) (\S+) (\S+), "
    r"the horse heading (\S+), the camera's yaw (\S+), its up (\S+) (\S+) (\S+), flags (\S+), "
    r"the horse's node at (\S+) (\S+) (\S+), its pitch (\S+) roll (\S+) deg, the camera in the node's frame "
    r"right (\S+) forward (\S+) up ([-0-9.]+)(?:, the reference's rotX (\S+) rotY (\S+), Bip01 (\S+) its axes' "
    r"up shares (\S+) (\S+) (\S+))?")
base_re = re.compile(
    r"Camera: riding base frame (\d+) - the base (\S+) (\S+) (\S+), the engine's camera (\S+) (\S+) (\S+), "
    r"the saddle forward (\S+) right (\S+) up (\S+), bob (\S+), the horse at (\S+) (\S+) (\S+) heading ([-0-9.]+)")
mark_re = re.compile(r"HandScript: mark (\S+) \(t=(\S+) s")

frames = {}
marks = []
order = []
with open(path, errors="replace") as f:
    for line in f:
        m = trace_re.search(line)
        if m:
            g = m.groups()
            frame = int(g[0])
            dx, dy, dz = float(g[4]), float(g[5]), float(g[6])
            h = float(g[7])
            s, c = math.sin(h), math.cos(h)
            yaw_fwd = dx * s + dy * c
            yaw_right = dx * c - dy * s
            frames[frame] = dict(frame=frame, pitch=float(g[16]), roll=float(g[17]),
                                 yaw_fwd=yaw_fwd, yaw_right=yaw_right, yaw_up=dz,
                                 node_right=float(g[18]), node_fwd=float(g[19]), node_up=float(g[20]),
                                 heading=h, mark=marks[-1][0] if marks else "start",
                                 z=float(g[15]), rotx=float(g[21]) if g[21] else 0.0, roty=float(g[22]) if g[22] else 0.0,
                                 bip=g[23] or "n/a", bx=float(g[24]) if g[24] else 0.0, by=float(g[25]) if g[25] else 0.0,
                                 bz=float(g[26]) if g[26] else 0.0)
            order.append(frame)
            continue
        m = base_re.search(line)
        if m:
            g = m.groups()
            frame = int(g[0])
            if frame in frames:
                bx, by, bz = float(g[1]), float(g[2]), float(g[3])
                cx, cy, cz = float(g[4]), float(g[5]), float(g[6])
                frames[frame].update(thrown=math.sqrt((bx - cx) ** 2 + (by - cy) ** 2 + (bz - cz) ** 2),
                                     saddle_fwd=float(g[7]), saddle_right=float(g[8]), saddle_up=float(g[9]),
                                     base_err_fwd=float(g[7]) - frames[frame]["yaw_fwd"],
                                     base_err_up=float(g[9]) - frames[frame]["yaw_up"])
            continue
        m = mark_re.search(line)
        if m:
            marks.append((m.group(1), float(m.group(2))))

print("trace frames:", len(order), "marks:", [m[0] for m in marks])


def rng(vals):
    if not vals:
        return "n/a"
    return "%.1f..%.1f (span %.1f)" % (min(vals), max(vals), max(vals) - min(vals))


phases = []
for fr in order:
    d = frames[fr]
    if not phases or phases[-1][0] != d["mark"]:
        phases.append([d["mark"], []])
    phases[-1][1].append(d)

for name, rows in phases:
    print("\n== after mark %s: %d frames" % (name, len(rows)))
    print("  horse z          ", rng([r["z"] for r in rows]), " climb %+.1f" % (rows[-1]["z"] - rows[0]["z"]))
    print("  node pitch deg   ", rng([r["pitch"] for r in rows]), " roll", rng([r["roll"] for r in rows]))
    print("  ref rotX         ", rng([r["rotx"] for r in rows]), " rotY", rng([r["roty"] for r in rows]))
    print("  Bip01 %s up shares x" % rows[0]["bip"], rng([r["bx"] for r in rows]), " y", rng([r["by"] for r in rows]),
          " z", rng([r["bz"] for r in rows]))
    print("  yaw-frame  fwd   ", rng([r["yaw_fwd"] for r in rows]), " right", rng([r["yaw_right"] for r in rows]),
          " up", rng([r["yaw_up"] for r in rows]))
    print("  node-frame fwd   ", rng([r["node_fwd"] for r in rows]), " right", rng([r["node_right"] for r in rows]),
          " up", rng([r["node_up"] for r in rows]))
    thrown = [r["thrown"] for r in rows if "thrown" in r]
    if thrown:
        print("  the base off the engine's camera (the eyes out of the saddle): ", rng(thrown),
              " mean %.1f" % (sum(thrown) / len(thrown)))
    errs = [r["base_err_fwd"] for r in rows if "base_err_fwd" in r]
    if errs:
        print("  settled saddle - yaw-frame fwd (the base's lag) ", rng(errs),
              " up", rng([r["base_err_up"] for r in rows if "base_err_up" in r]))
    # the pitch's frame-to-frame step: a stride's rocking would show here
    steps = [abs(rows[i]["pitch"] - rows[i - 1]["pitch"]) for i in range(1, len(rows))]
    if steps:
        print("  pitch step a frame: max %.2f deg, mean %.3f" % (max(steps), sum(steps) / len(steps)))

# the frames with the largest lag of the base behind the camera, with the pitch then
lagged = sorted((r for r in frames.values() if "base_err_fwd" in r), key=lambda r: -abs(r["base_err_fwd"]))[:8]
print("\nthe eight frames with the largest base lag (settled - actual, yaw-frame forward):")
for r in lagged:
    print("  frame %d (%s): lag fwd %.1f up %.1f; pitch %.1f; yaw-frame fwd %.1f, node-frame fwd %.1f" % (
        r["frame"], r["mark"], r["base_err_fwd"], r["base_err_up"], r["pitch"], r["yaw_fwd"], r["node_fwd"]))

# pitch against the yaw-frame forward: the correlation over all frames
rows = [r for r in frames.values()]
if len(rows) > 2:
    import statistics
    p = [r["pitch"] for r in rows]
    y = [r["yaw_fwd"] for r in rows]
    n = [r["node_fwd"] for r in rows]
    try:
        print("\ncorrelation pitch vs yaw-frame forward: %.2f; pitch vs node-frame forward: %.2f" % (
            statistics.correlation(p, y), statistics.correlation(p, n)))
    except Exception as e:
        print("correlation n/a", e)
