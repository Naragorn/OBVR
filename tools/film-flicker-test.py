#!/usr/bin/env python3
"""Checks for tools/film-flicker.py: which pixels of a film's frame stand out
of both their neighbours (a one-frame flash), and the film's verdict.
Run with: python3 tools/film-flicker-test.py"""
import importlib.util
import sys
from pathlib import Path

import numpy as np

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("film_flicker", here / "film-flicker.py")
ff = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ff)

failures = 0


def check(condition, what):
    global failures
    print("  %-5s %s" % ("ok" if condition else "FAIL", what))
    if not condition:
        failures += 1


def frame(value, shape=(4, 4)):
    return np.full(shape, value, dtype=np.uint8)


T = 24

print("a frame against its neighbours")
check(ff.flashed(frame(100), frame(100), frame(100), T).sum() == 0, "a still picture: nothing")
check(ff.flashed(frame(100), frame(110), frame(120), T).sum() == 0, "a steady change through the frame: nothing")
check(ff.flashed(frame(100), frame(100), frame(200), T).sum() == 0, "a step after the frame: nothing")
check(ff.flashed(frame(100), frame(200), frame(200), T).sum() == 0, "a step at the frame: nothing")
check(ff.flashed(frame(100), frame(200), frame(100), T).sum() == 16, "there and back: every pixel")
check(ff.flashed(frame(100), frame(20), frame(100), T).sum() == 16, "there and back darker: every pixel")
check(ff.flashed(frame(100), frame(250), frame(150), T).sum() == 16, "beyond both neighbours on the way: every pixel")
check(ff.flashed(frame(100), frame(120), frame(100), T).sum() == 0, "a flash under the threshold: nothing")
half = frame(100)
half[:2] = 200
check(ff.flashed(frame(100), half, frame(100), T).sum() == 8, "half the picture flashed: half the pixels")
check(ff.flashed(frame(250), frame(10), frame(250), T).sum() == 16, "no wrap-around in 8-bit grey")

print("a film")
still = [frame(100) for _ in range(6)]
shares = ff.film_shares(still, T)
check(len(shares) == 4, "a share for every frame with two neighbours")
check(max(shares) == 0.0, "a still film: no flash")
flash = [frame(100), frame(100), frame(200), frame(100), frame(100)]
shares = ff.film_shares(flash, T)
check(shares == [0.0, 1.0, 0.0], "one flashed frame: its share 1, the others 0")
check(ff.film_shares([frame(100), frame(100)], T) == [], "two frames: nothing to judge")

print("the film's motion")
check(ff.changed_shares(still, T) == [0.0] * 5, "a still film: nothing changed")
check(ff.changed_shares([frame(100), frame(200), frame(200)], T) == [1.0, 0.0], "a step: changed once, then still")
check(ff.changed_shares([frame(100)], T) == [], "one frame: no change to judge")

print("the part of the picture")
tall = np.full((4, 2), 100, dtype=np.uint8)
check(ff.part(tall, "all").shape == (4, 2), "all: the whole picture")
check(ff.part(tall, "lower").shape == (2, 2), "lower: the lower half")
check(ff.part(tall, "upper").shape == (2, 2), "upper: the upper half")

print("the verdict")
check(ff.verdict([0.0, 0.001, 0.0], 0.5) == (True, 1), "under the gate: PASS, the worst frame named")
check(ff.verdict([0.0, 0.01, 0.0], 0.5) == (False, 1), "over the gate: FAIL")
check(ff.verdict([], 0.5) == (True, -1), "no frame to judge: PASS, no worst frame")

print("%d failure(s)" % failures)
sys.exit(1 if failures else 0)
