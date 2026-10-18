#!/usr/bin/env python3
"""Frame times of an OBVR profiler capture, cut into the phases of a hand
script (the marks), from a harness run folder holding OBVR.log and the
OBVR-performance-N folder the runner moved in.

The log has no clock, so a mark is placed by the "Dual draws at scene call N"
lines around it, every 120 scene calls; the capture's events carry a scene id
that counts the same scenes at its own rate (one a frame against the log's
two passes), so the log's calls are scaled onto the capture's ids by their
overall ratio. Phases of tens of seconds are bracketed within a second or two.

Usage: perf-phases.py <run folder> mark1 mark2 [mark3 ...]
       each phase runs from one mark to the next.
"""
import csv
import math
import re
import sys
from pathlib import Path


def nearest_rank(values, percentile):
    if not values:
        return None
    ordered = sorted(values)
    rank = max(1, math.ceil(percentile / 100.0 * len(ordered)))
    return ordered[rank - 1]


def stats(values):
    if not values:
        return "-", "-", "-", "-", "-"
    mean = sum(values) / len(values)
    return ("%.2f" % mean, "%.2f" % nearest_rank(values, 50), "%.2f" % nearest_rank(values, 95),
            "%.2f" % nearest_rank(values, 99), "%.2f" % max(values))


def main():
    run = Path(sys.argv[1])
    marks = sys.argv[2:]
    captures = sorted(run.glob("OBVR-performance-*"))
    captures = [c for c in captures if c.is_dir()]
    if not captures:
        raise SystemExit("no OBVR-performance-N folder in %s" % run)
    capture = captures[0]
    log = (run / "OBVR.log").read_text(encoding="latin-1").splitlines()

    # The log's scene call around each mark.
    last_call = 0
    mark_call = {}
    pending = []
    call_at_stop = None
    for line in log:
        m = re.search(r"Dual draws at scene call (\d+)", line)
        if m:
            last_call = int(m.group(1))
            for name in pending:
                mark_call[name] = (mark_call[name][0], last_call)
            pending = []
            continue
        if "Performance: capture" in line and "stopped" in line and call_at_stop is None:
            call_at_stop = last_call  # the log's call when the capture ended
        m = re.search(r"HandScript: mark (\S+) ", line)
        if m:
            mark_call[m.group(1)] = (last_call, None)
            pending.append(m.group(1))
    for name in pending:
        mark_call[name] = (mark_call[name][0], last_call)
    # The log's calls scaled onto the capture's scene ids by where the
    # capture ended (a capture cut short by its record limit ends early).
    max_log_call = call_at_stop if call_at_stop else last_call

    events = list(csv.DictReader(open(capture / "cpu_events.csv", newline="")))
    frames = list(csv.DictReader(open(capture / "frames.csv", newline="")))
    manifest = (capture / "manifest.json").read_text()
    freq = int(re.search(r'"qpc_frequency":(\d+)', manifest).group(1))
    scene_ids = [int(e["scene_id"]) for e in events if e["scene_id"] not in ("", "0")]
    max_scene = max(scene_ids) if scene_ids else 1
    ratio = max_log_call / float(max_scene) if max_scene else 1.0

    # scene id -> the presents it was seen in.
    scene_present = {}
    for e in events:
        s = e["scene_id"]
        if s in ("", "0"):
            continue
        scene_present.setdefault(int(s), set()).add(int(e["present_id"]))

    def present_of_call(call):
        want = call / ratio
        best = None
        for s, presents in scene_present.items():
            d = abs(s - want)
            if best is None or d < best[0]:
                best = (d, min(presents))
        return best[1] if best else None

    frame_by_present = {int(f["present_id"]): f for f in frames}
    events_by_present = {}
    for e in events:
        events_by_present.setdefault(int(e["present_id"]), []).append(e)

    print("| Phase | Frames | Frame ms mean | median | p95 | p99 | max | over 20 ms | frame_end_callback mean / p95 | poses_wait mean / p95 | present mean / p95 | scene_pass mean / p95 |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for a, b in zip(marks, marks[1:]):
        if a not in mark_call or b not in mark_call:
            print("| %s to %s | mark missing in the log |" % (a, b))
            continue
        ca = mark_call[a][1] if mark_call[a][1] is not None else mark_call[a][0]
        cb = mark_call[b][0]
        pa = present_of_call(ca)
        pb = present_of_call(cb)
        if pa is None or pb is None or pb <= pa:
            print("| %s to %s | no frames found (calls %s to %s) |" % (a, b, ca, cb))
            continue
        ids = [p for p in sorted(frame_by_present) if pa <= p < pb]
        intervals = []
        for p0, p1 in zip(ids, ids[1:]):
            t0 = int(frame_by_present[p0]["start_tick"])
            t1 = int(frame_by_present[p1]["start_tick"])
            intervals.append((t1 - t0) * 1000.0 / freq)
        spans = {}
        for p in ids:
            for e in events_by_present.get(p, []):
                ms = (int(e["end_tick"]) - int(e["start_tick"])) * 1000.0 / freq
                spans.setdefault(e["event_type"], []).append(ms)
        over = sum(1 for v in intervals if v > 20.0)
        fm = stats(intervals)

        def two(name):
            v = spans.get(name, [])
            if not v:
                return "-"
            return "%.2f / %.2f" % (sum(v) / len(v), nearest_rank(v, 95))

        print("| %s to %s | %d | %s | %s | %s | %s | %s | %d (%.1f%%) | %s | %s | %s | %s |" % (
            a, b, len(ids), fm[0], fm[1], fm[2], fm[3], fm[4], over,
            100.0 * over / max(1, len(intervals)), two("frame_end_callback"), two("poses_wait"),
            two("present"), two("scene_pass")))


if __name__ == "__main__":
    main()
