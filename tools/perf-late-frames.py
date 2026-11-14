#!/usr/bin/env python3
"""Why the late frames of an OBVR profiler capture were late, and whether
anything in the frame costs on a fixed beat.

A frame here runs from one present's start to the next one's. Its time is
split by the capture's own spans (cpu_events.csv):

  callback before the submit  the present callback up to its first interop
                              flush for the eyes (pass 3): OBVR's per-frame
                              work - the hands, the bodies, the menus
  callback from the submit    the rest of the present span
  wait                        begin_frame (WaitGetPoses)
  scene                       the scene pass(es)
  between                     between_passes
  outside                     what is left: the engine outside its scene
                              pass - AI, physics, scripts, loading

Only frames delivered as world_dual count. A frame is late when it is longer
than --budget ms (12.5: over a 90 Hz period). Each late frame is put down to
the part that ran longest over its own median; "wait only" when none ran
1.5 ms over (the frame was long by its wait alone - a half-rate stretch).

The beat: a frame is long in a part when the part ran over its median by
0.4 ms and by four of the part's own robust deviations. The long frames'
distances from one to the next, in presents, are counted, those of three or
more (long frames one or two apart are a slow stretch, or the frame after a
long one); a beat is a distance that makes up four in ten of them, twenty or
more, and comes three times as often as long frames strewn at their rate
would give it. A step that runs on its own counter keeps its distance when the
counter starts over, where the present id's remainder does not. Built for the
hand fit's read (2026-11-14, docs/performance-spec.md): 13 to 19 ms before
the submit on every 15th frame, which no mean or percentile of the capture
showed as what it was.

Usage: perf-late-frames.py <OBVR-performance-N folder> [more folders]
                           [--budget 12.5]
"""
import bisect
import csv
import json
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path

PARTS = ("scene", "callback before the submit", "callback from the submit", "between", "outside")
WAIT_ONLY = "wait only"
OVER_MEDIAN_MS = 1.5
BEAT_MS = 0.4
BEAT_DEVIATIONS = 4.0
BEAT_LEAST = 20
BEAT_SHARE = 0.4
BEAT_TIMES_CHANCE = 3.0
BEAT_LEAST_DISTANCE = 3


def frame_parts(presents, events, frequency):
    """One row a frame from the spans. presents: (start, end, present id),
    sorted by start; events: (start, end, type, pass index). The last present
    has no next one and gives no row."""
    starts = [p[0] for p in presents]
    scene = defaultdict(float)
    wait = defaultdict(float)
    between = defaultdict(float)
    submit = {}
    for start, end, kind, pass_index in events:
        i = bisect.bisect_right(starts, start) - 1
        if i < 0:
            continue
        ms = (end - start) * 1000.0 / frequency
        if kind == "scene_pass":
            scene[i] += ms
        elif kind == "begin_frame":
            wait[i] += ms
        elif kind == "between_passes":
            between[i] += ms
        elif kind == "interop_flush" and pass_index == 3 and i not in submit and start <= presents[i][1]:
            submit[i] = start
    rows = []
    for i in range(len(presents) - 1):
        start, end, present_id = presents[i]
        callback = (end - start) * 1000.0 / frequency
        before = (submit[i] - start) * 1000.0 / frequency if i in submit else 0.0
        period = (starts[i + 1] - start) * 1000.0 / frequency
        rows.append({
            "present": present_id,
            "period": period,
            "scene": scene[i],
            "callback before the submit": before,
            "callback from the submit": callback - before,
            "between": between[i],
            "wait": wait[i],
            "outside": period - callback - wait[i] - scene[i] - between[i],
        })
    return rows


def classify_late(rows, budget_ms):
    """The late rows by the part that ran longest over its median:
    (medians, {cause: [excess ms, ...]})."""
    medians = {part: statistics.median(r[part] for r in rows) if rows else 0.0 for part in PARTS}
    causes = defaultdict(list)
    for r in rows:
        if r["period"] <= budget_ms:
            continue
        over = {part: r[part] - medians[part] for part in PARTS}
        worst = max(PARTS, key=lambda part: over[part])
        if over[worst] < OVER_MEDIAN_MS:
            causes[WAIT_ONLY].append(0.0)
        else:
            causes[worst].append(over[worst])
    return medians, causes


def beats(rows):
    """Parts that cost on a fixed beat of frames: [(part, frames from one
    long frame to the next, how often, of how many distances counted, the
    median of the frames on the beat, the median of all frames)]."""
    found = []
    for part in PARTS:
        if not rows:
            break
        values = [r[part] for r in rows]
        middle = statistics.median(values)
        deviation = statistics.median(abs(v - middle) for v in values) * 1.4826
        over = max(BEAT_MS, BEAT_DEVIATIONS * deviation)
        long_rows = sorted((r["present"], r[part]) for r in rows if r[part] - middle > over)
        pairs = [(b[0] - a[0], b[1]) for a, b in zip(long_rows, long_rows[1:]) if b[0] - a[0] >= BEAT_LEAST_DISTANCE]
        distances = Counter(d for d, _ in pairs)
        if not distances:
            continue
        distance, hits = distances.most_common(1)[0]
        total = len(pairs)
        rate = len(long_rows) / len(rows)
        # strewn at this rate, a distance d has the share rate * (1 - rate)^(d - least) of those counted
        chance = rate * (1.0 - rate) ** (distance - BEAT_LEAST_DISTANCE)
        if hits < BEAT_LEAST or hits < BEAT_SHARE * total or hits < BEAT_TIMES_CHANCE * chance * total:
            continue
        on_beat = statistics.median(v for d, v in pairs if d == distance)
        found.append((part, distance, hits, total, on_beat, middle))
    return found


def read_capture(folder):
    folder = Path(folder)
    frequency = json.loads((folder / "manifest.json").read_text())["qpc_frequency"]
    with open(folder / "frames.csv", newline="") as f:
        world = {int(r["present_id"]) for r in csv.DictReader(f) if r["delivered_mode"] == "world_dual"}
    presents = []
    events = []
    with open(folder / "cpu_events.csv", newline="") as f:
        for r in csv.DictReader(f):
            start, end = int(r["start_tick"]), int(r["end_tick"])
            if r["event_type"] == "present":
                presents.append((start, end, int(r["present_id"])))
            else:
                events.append((start, end, r["event_type"], int(r["pass_index"])))
    presents.sort()
    return [r for r in frame_parts(presents, events, frequency) if r["present"] in world]


def percentile(values, share):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(len(ordered) * share))]


def report(folder, budget_ms):
    rows = read_capture(folder)
    print("%s: %d world frames" % (Path(folder).name, len(rows)))
    if not rows:
        return
    medians, causes = classify_late(rows, budget_ms)
    late = sum(len(v) for v in causes.values())
    print("  medians ms: %s, wait %.2f" % (", ".join("%s %.2f" % (p, medians[p]) for p in PARTS),
                                           statistics.median(r["wait"] for r in rows)))
    print("  late (over %.1f ms): %d, %.1f %%" % (budget_ms, late, 100.0 * late / len(rows)))
    for cause, over in sorted(causes.items(), key=lambda item: -len(item[1])):
        print("    %-28s %5d (%4.1f %% of the world frames), over its median by %.1f ms at the median, "
              "%.1f at the 90th percentile, %.1f at most"
              % (cause, len(over), 100.0 * len(over) / len(rows), statistics.median(over),
                 percentile(over, 0.9), max(over)))
    found = beats(rows)
    if not found:
        print("  no part costs on a fixed beat of frames")
    for part, distance, hits, total, long_median, middle in found:
        print("  a beat: %s, every %d frames (%d of the %d distances of three or more between its long frames) - "
              "%.2f ms at the median on the beat against %.2f over all frames"
              % (part, distance, hits, total, long_median, middle))


def main(argv):
    budget = 12.5
    folders = []
    i = 0
    while i < len(argv):
        if argv[i] == "--budget":
            budget = float(argv[i + 1])
            i += 2
            continue
        folders.append(argv[i])
        i += 1
    if not folders:
        print(__doc__)
        return 2
    for folder in folders:
        report(folder, budget)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
