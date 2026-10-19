#!/usr/bin/env python3
"""Frame times of an OBVR profiler capture, cut into the phases of a hand
script (the marks), from a harness run folder holding OBVR.log and the
OBVR-performance-N folder the runner moved in.

The log has no clock; the scene call counter is the clock the capture and the
log share (the capture's scene_id is that counter). A mark line that carries
its scene call ("HandScript: mark people (..., scene call 4700)", since
2026-10-19) is placed exactly, in any stereo mode. An older log's mark is
placed by the "Dual draws at scene call N" heartbeat lines around it, every
120 scene calls - which only the dual pass writes - and the log's calls are
scaled onto the capture's ids by their overall ratio; such phases are
bracketed within a second or two.

Usage: perf-phases.py <run folder> mark1 mark2 [mark3 ...]
       each phase runs from one mark to the next.
       --events a,b,c  appends a "mean / p95" column per named event type
                       (any event of cpu_events.csv, e.g. draw_calls).
"""
import csv
import math
import re
import sys
from pathlib import Path


def mark_calls(log):
    """Each mark's place in the scene calls as (before, after), the scene
    call at which the capture stopped (None when it never did), and whether
    every mark was placed exactly by its own scene call."""
    last_call = 0
    mark_call = {}
    pending = []
    call_at_stop = None
    exact = True

    def close_pending(call):
        for name in pending:
            mark_call[name] = (mark_call[name][0], call)
        pending.clear()

    for line in log:
        m = re.search(r"Dual draws at scene call (\d+)", line)
        if m:
            last_call = int(m.group(1))
            close_pending(last_call)
            continue
        if "Performance: capture" in line and "stopped" in line and call_at_stop is None:
            call_at_stop = last_call  # the log's call when the capture ended
        m = re.search(r"HandScript: mark (\S+) \(.*scene call (\d+)\)", line)
        if m:
            last_call = int(m.group(2))
            close_pending(last_call)
            mark_call[m.group(1)] = (last_call, last_call)
            continue
        m = re.search(r"HandScript: mark (\S+) ", line)
        if m:
            exact = False
            mark_call[m.group(1)] = (last_call, None)
            pending.append(m.group(1))
    close_pending(last_call)
    return mark_call, call_at_stop, exact


def mark_ticks(log):
    """Each mark's tick of the performance counter, from a mark line that
    carries one ("..., tick 153659593659)", since 2026-10-19): the clock the
    capture's frames keep (frames.csv start_tick), so a phase is cut without
    any scene counter - which the alternate eye path does not count."""
    ticks = {}
    for line in log:
        m = re.search(r"HandScript: mark (\S+) \(.*tick (\d+)\)", line)
        if m:
            ticks[m.group(1)] = int(m.group(2))
    return ticks


def presents_between_ticks(frames, tick_from, tick_to):
    """The present ids of the frames whose start lies in [tick_from, tick_to)."""
    return sorted(int(f["present_id"]) for f in frames
                  if tick_from <= int(f["start_tick"]) < tick_to)


def compositor_phase(rows):
    """What the compositor saw of a phase, from its compositor.csv rows (one
    a frame, since 2026-10-19): frames dropped (the previous frame scanned
    out again), mis-presented, reprojected (any reprojection flag), the app's
    GPU time before its second Submit as the compositor measured it, and the
    moment of that Submit relative to the frame's vsync - negative is before
    the vsync. None for no rows."""
    if not rows:
        return None
    dropped = sum(int(r["dropped_frames"]) for r in rows)
    mis = sum(int(r["mis_presented"]) for r in rows)
    # Reprojected for a reason - CPU 0x01, GPU 0x02, motion smoothing 0x08
    # (openvr.h VRCompositor_ReprojectionReason_*). 0x04 only says async
    # reprojection is on and sits on every frame.
    reprojected = sum(1 for r in rows if int(r["reprojection_flags"]) & 0x0B)
    gpu = [float(r["pre_submit_gpu_ms"]) for r in rows]
    submit = [float(r["new_frame_ready_ms"]) for r in rows]
    return {
        "frames": len(rows),
        "dropped": dropped,
        "mis_presented": mis,
        "reprojected": reprojected,
        "gpu_mean": sum(gpu) / len(gpu),
        "gpu_p95": nearest_rank(gpu, 95),
        "submit_mean": sum(submit) / len(submit),
        "submit_p95": nearest_rank(submit, 95),
    }


def compositor_cells(summary):
    """The three table cells for a phase's compositor summary."""
    if summary is None:
        return "-", "-", "-"
    return ("%d / %d / %d" % (summary["dropped"], summary["mis_presented"], summary["reprojected"]),
            "%.2f / %.2f" % (summary["gpu_mean"], summary["gpu_p95"]),
            "%.2f / %.2f" % (summary["submit_mean"], summary["submit_p95"]))


def split_args(argv):
    """The run folder, the marks, and the extra event types from the command
    line: "--events a,b" anywhere after the run folder."""
    events = []
    rest = []
    i = 0
    while i < len(argv):
        if argv[i] == "--events" and i + 1 < len(argv):
            events += [e for e in argv[i + 1].split(",") if e]
            i += 2
            continue
        rest.append(argv[i])
        i += 1
    return rest[0] if rest else None, rest[1:], events


def call_scale(max_log_call, max_scene, exact):
    """The factor from the log's scene calls to the capture's scene ids:
    one when the marks are exact, else the two counters' overall ratio."""
    if exact or not max_scene:
        return 1.0
    return max_log_call / float(max_scene)


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
    run_arg, marks, extra_events = split_args(sys.argv[1:])
    if run_arg is None:
        raise SystemExit(__doc__)
    run = Path(run_arg)
    captures = sorted(run.glob("OBVR-performance-*"))
    captures = [c for c in captures if c.is_dir()]
    if not captures:
        raise SystemExit("no OBVR-performance-N folder in %s" % run)
    capture = captures[0]
    log = (run / "OBVR.log").read_text(encoding="latin-1").splitlines()

    # The marks' ticks, when the log has them; else the log's scene call
    # around each mark.
    ticks = mark_ticks(log)
    by_tick = all(m in ticks for m in marks)
    mark_call, call_at_stop, exact = mark_calls(log)
    last_call = max([c for pair in mark_call.values() for c in pair if c is not None] or [0])
    # The log's calls scaled onto the capture's scene ids by where the
    # capture ended (a capture cut short by its record limit ends early) -
    # only for a log whose marks had to be bracketed.
    max_log_call = call_at_stop if call_at_stop else last_call

    events = list(csv.DictReader(open(capture / "cpu_events.csv", newline="")))
    frames = list(csv.DictReader(open(capture / "frames.csv", newline="")))
    manifest = (capture / "manifest.json").read_text()
    freq = int(re.search(r'"qpc_frequency":(\d+)', manifest).group(1))
    scene_ids = [int(e["scene_id"]) for e in events if e["scene_id"] not in ("", "0")]
    max_scene = max(scene_ids) if scene_ids else 1
    ratio = call_scale(max_log_call, max_scene, exact)

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
    # The compositor's rows, when the capture has them (compositor.csv).
    compositor_by_present = {}
    if (capture / "compositor.csv").exists():
        for r in csv.DictReader(open(capture / "compositor.csv", newline="")):
            compositor_by_present.setdefault(int(r["present_id"]), []).append(r)

    print("| Phase | Frames | Frame ms mean | median | p95 | p99 | max | over 20 ms | frame_end_callback mean / p95 | poses_wait mean / p95 | present mean / p95 | scene_pass mean / p95 | compositor: dropped / mis-presented / reprojected | app gpu before submit ms mean / p95 | submit at vsync ms mean / p95 |"
          + "".join(" %s mean / p95 |" % e for e in extra_events))
    print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|" + "---:|" * len(extra_events))
    for a, b in zip(marks, marks[1:]):
        if by_tick:
            ids = presents_between_ticks(frames, ticks[a], ticks[b])
            if not ids:
                print("| %s to %s | no frames found (ticks %d to %d) |" % (a, b, ticks[a], ticks[b]))
                continue
        else:
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

        comp = compositor_cells(compositor_phase(
            [r for p in ids for r in compositor_by_present.get(p, [])]))
        print("| %s to %s | %d | %s | %s | %s | %s | %s | %d (%.1f%%) | %s | %s | %s | %s | %s | %s | %s |" % (
            a, b, len(ids), fm[0], fm[1], fm[2], fm[3], fm[4], over,
            100.0 * over / max(1, len(intervals)), two("frame_end_callback"), two("poses_wait"),
            two("present"), two("scene_pass"), comp[0], comp[1], comp[2])
            + "".join(" %s |" % two(e) for e in extra_events))


if __name__ == "__main__":
    main()
