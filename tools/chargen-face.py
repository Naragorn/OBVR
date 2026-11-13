#!/usr/bin/env python3
"""Reads where the race menu's character stood in a run's OBVR.log.

The race menu's view logs, at its opening and now and then while it is up,
"Chargen view: ... the face <angle> degrees from the view" (left positive;
camera/ChargenMenuView.cpp). The engine's own aim puts the face 20.1 degrees
to the right (-20.1). This prints every angle and fails when one lies more
than --most degrees off that aim, or when there is none.

  python3 tools/chargen-face.py artifacts/hand-script/race-menu-turned/<run>/OBVR.log
  python3 tools/chargen-face.py OBVR.log --most 3
"""
import argparse
import re
import sys

ENGINE_AIM = -20.1
LINE = re.compile(r"Chargen view: .*?the face (-?\d+(?:\.\d+)?) degrees from the view")


def face_angles(lines):
    return [float(m.group(1)) for m in (LINE.search(line) for line in lines) if m]


def verdict(angles, most):
    if not angles:
        return False, "no chargen face line"
    worst = max(angles, key=lambda a: abs(a - ENGINE_AIM))
    off = abs(worst - ENGINE_AIM)
    if off > most:
        return False, f"the face {worst:.1f} degrees from the view, {off:.1f} off the engine's aim"
    return True, f"{len(angles)} line(s), the worst {worst:.1f} ({off:.1f} off the engine's aim)"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log")
    parser.add_argument("--most", type=float, default=3.0)
    args = parser.parse_args()
    with open(args.log, encoding="utf-8", errors="replace") as f:
        angles = face_angles(f)
    ok, text = verdict(angles, args.most)
    print(("PASS: " if ok else "FAIL: ") + text)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
