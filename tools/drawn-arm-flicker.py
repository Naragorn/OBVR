#!/usr/bin/env python3
"""The body's arms as drawn, frame by frame, from Debug.BodyTrace's "Body drawn"
lines (game/BodyIk.cpp LogDrawnArms): how far each arm bone turned in the
root's frame since the frame drawn before - the body's upper arm, its twist,
the forearm, its twist, the hand, and the first-person hand.

Per window between a hand script's marks: the largest turn in a frame of each
bone. With the controllers held still in a window, a drawn bone turning is the
flicker (Nadi 2026-11-16: the arms on an equip or a pick-up).

  tools/drawn-arm-flicker.py <run folder or OBVR.log> [--gate WINDOW ...] [--most DEGREES]

A gate fails when any bone of either arm turned more than --most (2.0) degrees
in a frame of that window, skipping the window's first frame (the mark's own
move of the controllers lands there).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

MARK = re.compile(r"HandScript: mark (\S+)")
DRAWN = re.compile(r"Body drawn: f=(\d+) \| R (.*?) \| L (.*)$")
BONES = ("upper", "utwist", "fore", "ftwist", "hand", "fphand")


def parse_side(text: str) -> list[float]:
    values = []
    for token in text.split():
        values.append(float(token.rstrip("h")) if token != "-" else 0.0)
    return values


def windows(lines):
    name, rows = "start", []
    for line in lines:
        m = MARK.search(line)
        if m:
            yield name, rows
            name, rows = m.group(1), []
            continue
        d = DRAWN.search(line)
        if d:
            rows.append((int(d.group(1)), parse_side(d.group(2)), parse_side(d.group(3))))
    yield name, rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("run")
    ap.add_argument("--gate", action="append", default=[])
    ap.add_argument("--most", type=float, default=2.0)
    a = ap.parse_args()
    path = Path(a.run)
    log = path / "OBVR.log" if path.is_dir() else path
    lines = log.read_text(errors="replace").splitlines()
    failed = False
    seen = set()
    print(f"{'window':14} {'frames':>6}  " + "  ".join(f"R{b:>6}" for b in BONES) + "  " + "  ".join(f"L{b:>6}" for b in BONES))
    for name, rows in windows(lines):
        if not rows:
            continue
        seen.add(name)
        body = rows[1:] if len(rows) > 1 else rows
        most_r = [max((r[1][i] if i < len(r[1]) else 0.0) for r in body) for i in range(len(BONES))]
        most_l = [max((r[2][i] if i < len(r[2]) else 0.0) for r in body) for i in range(len(BONES))]
        verdict = ""
        if name in a.gate:
            worst = max(most_r + most_l)
            ok = worst <= a.most
            failed |= not ok
            verdict = f"  gate {'PASS' if ok else 'FAIL'} (worst {worst:.1f})"
        print(f"{name:14} {len(rows):6}  " + "  ".join(f"{v:7.1f}" for v in most_r) + "  "
              + "  ".join(f"{v:7.1f}" for v in most_l) + verdict)
    for g in a.gate:
        if g not in seen:
            print(f"gate {g}: no window of that name with drawn lines - FAIL")
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
