#!/usr/bin/env python3
"""Checks for tools/perf-late-frames.py: a frame's parts from the capture's
spans, the late frames' causes, and a cost on a fixed beat of frames.
Run with: python3 tools/perf-late-frames-test.py"""
import importlib.util
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("perf_late_frames", here / "perf-late-frames.py")
lf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lf)

failures = 0


def check(condition, what):
    global failures
    print("  %-5s %s" % ("ok" if condition else "FAIL", what))
    if not condition:
        failures += 1


def near(a, b):
    return abs(a - b) < 1e-9


print("a frame's parts")
# Ticks of a tenth of a millisecond: frequency 10000. Three presents, 11 ms
# apart, then 20 ms; the first frame's callback is 3 ms with its submit 2 ms in.
presents = [(0, 30, 7), (110, 120, 8), (310, 320, 9)]
events = [
    (20, 21, "interop_flush", 3),       # the submit of frame 7, 2 ms into the callback
    (25, 26, "interop_flush", 3),       # a later flush of the same pass: not the first
    (31, 51, "begin_frame", 0),         # 2 ms of waiting
    (60, 90, "scene_pass", 0),          # 3 ms
    (90, 92, "between_passes", 0),      # 0.2 ms
    (92, 102, "scene_pass", 1),         # 1 ms, the second pass
    (105, 106, "interop_flush", 3),     # before present 8 began: frame 7's, but after its callback
    (125, 135, "scene_pass", 0),        # frame 8: no submit flush inside its callback
    (-5, -1, "scene_pass", 0),          # before the first present: no frame
]
rows = lf.frame_parts(presents, events, 10000)
check(len(rows) == 2, "a row a present but the last, which has no next one")
r = rows[0]
check(r["present"] == 7 and near(r["period"], 11.0), "the frame runs to the next present's start")
check(near(r["callback before the submit"], 2.0) and near(r["callback from the submit"], 1.0),
      "the callback split at its first eye flush")
check(near(r["scene"], 4.0), "both scene passes summed")
check(near(r["wait"], 2.0) and near(r["between"], 0.2), "the wait and the time between the passes")
check(near(r["outside"], 11.0 - 3.0 - 2.0 - 4.0 - 0.2), "outside: what the spans leave of the frame")
r = rows[1]
check(near(r["callback before the submit"], 0.0) and near(r["callback from the submit"], 1.0),
      "no eye flush inside the callback: all of it counts from the submit")
check(near(r["period"], 20.0) and near(r["scene"], 1.0), "the second frame's own spans")
check(lf.frame_parts([], [], 10000) == [], "no presents: no rows")
check(lf.frame_parts([(0, 10, 1)], [], 10000) == [], "one present: no row")


def row(present, period, **parts):
    base = {"present": present, "period": period, "wait": 5.0}
    for part in lf.PARTS:
        base[part] = parts.get(part.replace(" ", "_"), 1.0)
    return base


print("the late frames' causes")
frames = [row(i, 11.1) for i in range(20)]
frames.append(row(20, 20.0, scene=9.0))
frames.append(row(21, 25.0, callback_before_the_submit=14.0, scene=3.0))
frames.append(row(22, 22.0))                      # nothing ran long: its wait alone
frames.append(row(23, 12.5, outside=30.0))        # exactly on the budget: not late
medians, causes = lf.classify_late(frames, 12.5)
check(near(medians["scene"], 1.0), "the parts' medians over all frames")
check(len(causes["scene"]) == 1 and near(causes["scene"][0], 8.0), "a long scene pass: put down to it, by its excess")
check(len(causes["callback before the submit"]) == 1 and near(causes["callback before the submit"][0], 13.0),
      "two parts long: the one further over its median")
check(len(causes[lf.WAIT_ONLY]) == 1, "no part 1.5 ms over its median: late by its wait alone")
check("outside" not in causes, "a frame on the budget is not late")
check(lf.classify_late([], 12.5)[1] == {}, "no frames: no causes")

print("a cost on a fixed beat")
# 15 frames a beat, 40 beats: the callback before the submit 6 ms on residue 4
beat = []
for i in range(600):
    extra = 6.0 if i % 15 == 4 else 0.0
    beat.append(row(1000 + i, 11.1, callback_before_the_submit=0.5 + extra + (i % 7) * 0.01))
found = lf.beats(beat)
check(len(found) == 1, "one beat found, a part only once")
part, distance, hits, total, long_median, middle = found[0]
check(part == "callback before the submit" and distance == 15, "the part and the frames from one long frame to the next")
check(hits == 39 and total == 39, "forty long frames: thirty-nine distances, all of them the beat's")
check(long_median > 6.0 and middle < 1.0, "the median on the beat against all frames'")
flat = [row(i, 11.1, scene=2.0 + (i % 5) * 0.01) for i in range(600)]
check(lf.beats(flat) == [], "a part that wobbles by hundredths: no beat")
single = [row(i, 11.1, outside=40.0 if i == 300 else 1.0 + (i % 3) * 0.01) for i in range(600)]
check(lf.beats(single) == [], "one long frame is no beat")
check(lf.beats(beat[:150]) == [], "ten long frames: too few to call a beat")
# the step's counter started over halfway: the beat two frames later from there
restarted = []
for i in range(1200):
    on = (i % 15 == 4) if i < 600 else (i % 15 == 6)
    restarted.append(row(1000 + i, 11.1, callback_before_the_submit=0.5 + (6.0 if on else 0.0)))
found = lf.beats(restarted)
check(len(found) == 1 and found[0][1] == 15 and found[0][2] == 78 and found[0][3] == 79,
      "a beat whose counter started over: still every 15, one distance of 17 beside")
# long frames strewn with no beat: about a tenth of all frames
strewn = [row(i, 11.1, scene=9.0 if (i * i * 31 + i * 17) % 97 < 10 else 2.0) for i in range(3000)]
check(lf.beats(strewn) == [], "long frames strewn over the capture: no beat")
# long frames next to each other (a slow stretch): the distance 1 is no beat
stretch = [row(i, 11.1, outside=9.0 if 500 <= i < 700 else 2.0 + (i % 4) * 0.01) for i in range(3000)]
check(lf.beats(stretch) == [], "a slow stretch, frame after frame: no beat")
# a part that scatters widely: only what stands four deviations out is long
wide = [row(i, 11.1, outside=3.0 + ((i * i * 13 + i * 7) % 101) * 0.02) for i in range(3000)]
check(lf.beats(wide) == [], "a part scattering widely: nothing long, no beat")
# the frame after each beat long too: the distance 1 does not hide the beat
echo = []
for i in range(600):
    on = i % 15 in (4, 5)
    echo.append(row(1000 + i, 11.1, callback_before_the_submit=0.5 + (6.0 if on else 0.0)))
found = lf.beats(echo)
check(len(found) == 1 and found[0][1] == 14 and found[0][2] == 39,
      "the frame after a long one long too: the beat shows as the distance to the next pair (14)")
check(lf.beats([]) == [], "no frames: no beat")

if failures:
    print("%d check(s) failed" % failures)
    sys.exit(1)
print("all checks passed")
