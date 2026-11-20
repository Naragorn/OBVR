#!/usr/bin/env python3
"""Checks for tools/drawn-arm-flicker.py: the columns a gate reads and the
worst turn among them.
Run with: python3 tools/drawn-arm-flicker-test.py"""
import importlib.util
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("drawn_arm_flicker", here / "drawn-arm-flicker.py")
af = importlib.util.module_from_spec(spec)
spec.loader.exec_module(af)

failures = 0


def check(condition, what):
    global failures
    print("  %-5s %s" % ("ok" if condition else "FAIL", what))
    if not condition:
        failures += 1


# Two frames: the right arm's columns, then the left's (upper, utwist, fore,
# ftwist, hand, fphand). The left arm swings; the right hand turns 106.5.
rows = [
    (1, [0.0, 0.0, 0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]),
    (2, [3.6, 3.6, 41.6, 84.7, 106.5, 0.0], [44.6, 44.9, 35.4, 32.3, 0.0, 106.5]),
]

print("the columns")
check(af.columns(None) == list(range(12)), "no --bones: all twelve")
check(af.columns("Rhand,Lfphand") == [4, 11], "Rhand and Lfphand: the right hand and the left first-person hand")
check(af.columns("Lupper") == [6], "Lupper: the left upper arm")
try:
    af.columns("Rknee")
    check(False, "an unknown bone: refused")
except ValueError:
    check(True, "an unknown bone: refused")

print("the worst turn")
check(af.worst(rows, af.columns(None)) == 106.5, "all columns: 106.5")
check(af.worst(rows, af.columns("Lupper,Lutwist")) == 44.9, "the left upper arm and its twist: 44.9")
check(af.worst(rows, af.columns("Rfphand")) == 0.0, "the right first-person hand: 0")
check(af.worst([], af.columns(None)) == 0.0, "no frames: 0")

print("%d failure(s)" % failures)
sys.exit(1 if failures else 0)
