#!/usr/bin/env python3
"""How far, and how long, the hands and the weapon trail the controllers.

Reads the "Lag trace:" lines of an OBVR.log ([Debug] HandLagTrace,
camera/hands/HandLagTrace.h) and, for every mark that started them, prints
the frame rate and how far each drawn thing is behind where its controller
put it while the controllers sweep at an even speed: the right hand bone, the
left hand bone and the Weapon node, and the weapon weight's own gap.

Each thing's offset from its controller's place while the hands stood still
(the frames before the sweep began) is taken off first: the pins and the grip
put a bone at its own fixed place against the controller, and only what
changes with the motion is the trail. The trail is measured along the sweep's
direction, in game units, centimetres and milliseconds (the trail over the
controller's speed in that frame).

    python3 tools/hand-lag.py <OBVR.log> [--frames]

--frames prints every frame's numbers as well.
"""

import math
import re
import sys
from collections import OrderedDict

UNITS_PER_METRE = 69.99125
LINE = re.compile(r"Lag trace: (\S+) (\d+) dt ([\d.]+) steps (\d+) ([\d.]+) \| R (.+?) \| L (.+?) \| W (.+?) \| lag ([\d.\-]+) ([\d.\-]+)")
STILL_UNITS = 0.05  # a controller place that moved less than this in a frame stands still


def vec(text):
    return tuple(float(v) for v in text.split())


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def norm(a):
    return math.sqrt(dot(a, a))


def parse(path):
    marks = OrderedDict()
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE.search(line)
            if not m:
                continue
            r = vec(m.group(6))
            frame = {
                "n": int(m.group(2)),
                "dt": float(m.group(3)),
                "steps": int(m.group(4)),
                "step": float(m.group(5)),
                "rc": r[0:3], "rp": r[3:6], "rb": r[6:9],
                "lc": vec(m.group(7))[0:3], "lb": vec(m.group(7))[3:6],
                "w": vec(m.group(8)),
                "gap": float(m.group(9)),
            }
            marks.setdefault(m.group(1), []).append(frame)
    return marks


def trail(frames, ctl_key, drawn_key):
    """Per frame of the sweep: the trail along the sweep (units) and the
    controller's speed then (units a second)."""
    moving = []
    for i in range(1, len(frames)):
        step = norm(sub(frames[i][ctl_key], frames[i - 1][ctl_key]))
        if step > STILL_UNITS:
            moving.append(i)
    if not moving:
        return None
    first = moving[0]
    # The rest offset: the frames before the sweep, the controller still.
    rest = [sub(f[drawn_key], f[ctl_key]) for f in frames[:first]]
    if not rest:
        return None
    rest_offset = tuple(sum(c) / len(rest) for c in zip(*rest))
    direction = sub(frames[moving[-1]][ctl_key], frames[first - 1][ctl_key])
    length = norm(direction)
    if length <= 0.0:
        return None
    direction = tuple(c / length for c in direction)
    out = []
    for i in range(first, len(frames)):
        offset = sub(sub(frames[i][drawn_key], frames[i][ctl_key]), rest_offset)
        behind = -dot(offset, direction)
        step = norm(sub(frames[i][ctl_key], frames[i - 1][ctl_key]))
        speed = step / (frames[i]["dt"] / 1000.0) if frames[i]["dt"] > 0 else 0.0
        out.append((i, behind, speed, i in moving))
    return out


def summary(name, frames, show_frames):
    dts = [f["dt"] for f in frames[1:] if f["dt"] > 0]
    fps = 1000.0 / (sum(dts) / len(dts)) if dts else 0.0
    steps = sum(f["steps"] for f in frames) / len(frames)
    step_ms = sum(f["step"] for f in frames) / len(frames)
    print(f"{name}: {len(frames)} frames, {fps:.1f} fps (dt mean {sum(dts) / len(dts):.1f} ms, "
          f"longest {max(dts):.1f}), physics {steps:.2f} steps a frame of {step_ms:.2f} ms on average")
    for label, ctl, drawn in (("right bone", "rc", "rb"), ("right pin", "rc", "rp"),
                              ("left bone", "lc", "lb"), ("weapon", "rc", "w")):
        if drawn == "w" and all(norm(f["w"]) == 0.0 for f in frames):
            continue
        t = trail(frames, ctl, drawn)
        if t is None:
            print(f"  {label:11s} no sweep")
            continue
        sweeping = [(b, s) for (_, b, s, moving) in t if moving]
        mean_b = sum(b for b, _ in sweeping) / len(sweeping)
        max_b = max(b for b, _ in sweeping)
        speed = sum(s for _, s in sweeping) / len(sweeping)
        ms = 1000.0 * mean_b / speed if speed > 0 else 0.0
        after = [b for (_, b, _, moving) in t if not moving]
        print(f"  {label:11s} behind {mean_b:6.2f} units ({100 * mean_b / UNITS_PER_METRE:5.2f} cm, "
              f"{ms:5.1f} ms at {speed / UNITS_PER_METRE:4.2f} m/s), most {max_b:6.2f}; "
              f"after the stop {after[0] if after else 0:6.2f} -> {after[-1] if after else 0:6.2f}")
        if show_frames:
            for (i, b, s, moving) in t:
                print(f"      frame {frames[i]['n']:3d} dt {frames[i]['dt']:5.1f} steps {frames[i]['steps']} "
                      f"behind {b:6.2f} speed {s / UNITS_PER_METRE:5.2f} m/s {'moving' if moving else 'still'}")
    gaps = [f["gap"] for f in frames]
    print(f"  weight gap  most {max(gaps) * 100:.2f} cm")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    show = "--frames" in sys.argv[2:]
    marks = parse(sys.argv[1])
    if not marks:
        print("no Lag trace lines")
        return 1
    for name, frames in marks.items():
        summary(name, frames, show)
    return 0


if __name__ == "__main__":
    sys.exit(main())
