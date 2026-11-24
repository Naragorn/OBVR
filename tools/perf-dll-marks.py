#!/usr/bin/env python3
"""One DLL's share of the render thread between two marks of a hand-script
run, by that DLL's own functions, for several runs side by side - the
before and after of a change to OBVR.dll (docs/performance-draw-hooks.md).

From each run folder: OBVR.log (the marks' ticks) and the capture the runner
moved in (OBVR-performance-N: cpu_events.csv, samples.csv, samples-modules.csv;
the render sampler, render/RenderSampler.h). A sample belongs to the DLL when
its eip lies on one of the DLL's pages; its function is the last symbol at or
before it in the link map of the very DLL the run loaded (the build writes
OBVR.map beside OBVR.dll - a map of another build names other functions).

Per run: the frames between the marks, their length, the work (length less the
pose wait) and the scene pass in ms, the sampler's spacing, and the DLL's time
a frame inside and outside the scene pass (samples times spacing over frames).
Then, with the runs as columns: every module inside and outside the pass, and
the DLL's functions inside and outside it.

tools/perf-functions.py does the same for the engine's functions and counts
other modules as one line each; this one opens a module up.

Usage: perf-dll-marks.py <from mark> <to mark> <label>=<run folder>=<link map> ...
                         [--module OBVR.dll] [--top 14] [--json FILE]
"""

import bisect
import csv
import json
import os
import re
import statistics
import subprocess
import sys
from collections import Counter

TICKS_PER_MS = 1e4  # the capture's ticks are 100 ns


def parse_args(argv):
    """The two marks, the runs as (label, folder, map), and the options."""
    opts = {"module": "OBVR.dll", "top": 14, "json": ""}
    rest = []
    i = 0
    while i < len(argv):
        name = argv[i][2:]
        if argv[i].startswith("--") and name in opts and i + 1 < len(argv):
            opts[name] = type(opts[name])(argv[i + 1])
            i += 2
        else:
            rest.append(argv[i])
            i += 1
    if len(rest) < 3 or any(spec.count("=") != 2 for spec in rest[2:]):
        sys.exit(__doc__)
    return rest[0], rest[1], [tuple(spec.split("=")) for spec in rest[2:]], opts


def read_map(path):
    """The link map's code symbols as (rva, name), sorted, rvas apart."""
    preferred = 0
    symbols = []
    with open(path, errors="replace") as lines:
        for line in lines:
            found = re.match(r"\s*Preferred load address is ([0-9a-fA-F]+)", line)
            if found:
                preferred = int(found.group(1), 16)
            found = re.match(r"\s*0001:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})\s", line)
            if found:
                symbols.append((int(found.group(2), 16) - preferred, found.group(1)))
    symbols.sort()
    return [rva for rva, _ in symbols], [name for _, name in symbols]


_plain = {}


def plain_name(decorated):
    """A decorated name as its function alone; the name as it is without llvm-undname."""
    if decorated not in _plain:
        text = decorated
        try:
            out = subprocess.run(["llvm-undname", decorated], capture_output=True, text=True).stdout.split("\n")
            if len(out) > 1 and out[1].strip():
                text = out[1].strip()
        except OSError:
            pass
        text = re.sub(r"`anonymous namespace'::", "", text)
        text = re.sub(
            r"\b(void|int|long|bool|float|struct|class|const|unsigned|public:|private:"
            r"|__cdecl|__stdcall|__fastcall|__thiscall) ",
            "",
            text,
        )
        _plain[decorated] = re.sub(r"\(.*", "", text).replace("obvr::render::", "")[:60]
    return _plain[decorated]


def mark_ticks(run):
    """Each mark of the run's OBVR.log with its tick."""
    marks = {}
    with open(os.path.join(run, "OBVR.log"), errors="replace") as lines:
        for line in lines:
            found = re.search(r"HandScript: mark (\S+) \(.*tick (\d+)\)", line)
            if found:
                marks[found.group(1)] = int(found.group(2))
    return marks


def capture_folder(run):
    """The run's capture: its first OBVR-performance-N folder, not the report's."""
    for name in sorted(os.listdir(run)):
        if name.startswith("OBVR-performance") and not name.endswith("report"):
            return os.path.join(run, name)
    sys.exit(f"{run}: no OBVR-performance-N folder")


def frame_times(capture, lo, hi):
    """Between the ticks: the frames' lengths, the scene passes and the pose waits, in ms."""
    with open(os.path.join(capture, "cpu_events.csv"), newline="") as rows:
        events = [row for row in csv.DictReader(rows) if lo <= int(row["start_tick"]) < hi]

    def spans(kind):
        return [(int(e["end_tick"]) - int(e["start_tick"])) / TICKS_PER_MS for e in events if e["event_type"] == kind]

    ends = sorted(int(e["end_tick"]) for e in events if e["event_type"] == "present")
    lengths = [(b - a) / TICKS_PER_MS for a, b in zip(ends, ends[1:])]
    return lengths, spans("scene_pass"), spans("poses_wait")


def read_run(run, map_path, first, last, module):
    """One run's numbers between the two marks."""
    marks = mark_ticks(run)
    for mark in (first, last):
        if mark not in marks:
            sys.exit(f"{run}: no mark {mark} in OBVR.log (it has {', '.join(marks) or 'none'})")
    lo, hi = marks[first], marks[last]
    capture = capture_folder(run)
    lengths, scene, wait = frame_times(capture, lo, hi)
    frames = len(lengths)
    if frames == 0:
        sys.exit(f"{run}: no frames between {first} and {last}")

    pages = {}
    base = None
    with open(os.path.join(capture, "samples-modules.csv"), newline="") as rows:
        for row in csv.DictReader(rows):
            pages[row["page"].upper()] = row["module"]
            if row["module"].lower() == module.lower():
                at = int(row["base"], 16)
                base = at if base is None else min(base, at)
    if base is None:
        sys.exit(f"{run}: {module} is not among the capture's modules")
    rvas, names = read_map(map_path)

    ticks = []
    modules = {"in": Counter(), "out": Counter()}
    functions = {"in": Counter(), "out": Counter()}
    with open(os.path.join(capture, "samples.csv"), newline="") as rows:
        for row in csv.DictReader(rows):
            tick = int(row["tick"])
            if not lo <= tick < hi:
                continue
            ticks.append(tick)
            side = "in" if row["in_pass"] == "1" else "out"
            owner = pages.get(row["eip"][:4].upper(), "(no module)")
            modules[side][owner] += 1
            if owner.lower() == module.lower():
                at = bisect.bisect_right(rvas, int(row["eip"], 16) - base) - 1
                functions[side][names[at] if at >= 0 else "(no symbol)"] += 1
    ticks.sort()
    spacing = statistics.median(b - a for a, b in zip(ticks, ticks[1:])) / TICKS_PER_MS

    def a_frame(samples):
        return samples * spacing / frames

    def own(side):
        return sum(count for name, count in modules[side].items() if name.lower() == module.lower())

    return {
        "run": os.path.basename(run.rstrip("/")),
        "frames": frames,
        "seconds": (hi - lo) / TICKS_PER_MS / 1e3,
        "frame_ms": statistics.mean(lengths),
        "work_ms": statistics.mean(lengths) - sum(wait) / frames,
        "scene_ms": statistics.mean(scene),
        "scene_median_ms": statistics.median(scene),
        "spacing_ms": spacing,
        "samples": len(ticks),
        "dll_in_samples": own("in"),
        "dll_out_samples": own("out"),
        "dll_in_ms": a_frame(own("in")),
        "dll_out_ms": a_frame(own("out")),
        "modules_in": {name: a_frame(count) for name, count in modules["in"].items()},
        "modules_out": {name: a_frame(count) for name, count in modules["out"].items()},
        "functions_in": {plain_name(name): a_frame(count) for name, count in functions["in"].items()},
        "functions_out": {plain_name(name): a_frame(count) for name, count in functions["out"].items()},
    }


def print_table(title, results, key, top):
    """One table with the runs as columns, the largest rows first."""
    print(f"== {title}, ms a frame: " + " | ".join(result["label"] for result in results))
    rows = {name for result in results for name in result[key]}
    for name in sorted(rows, key=lambda row: -max(result[key].get(row, 0) for result in results))[:top]:
        print("   " + " | ".join(f"{result[key].get(name, 0):5.3f}" for result in results) + "  " + name)


def main():
    first, last, runs, opts = parse_args(sys.argv[1:])
    module = opts["module"]
    results = []
    for label, run, map_path in runs:
        result = read_run(run, map_path, first, last, module)
        result["label"] = label
        results.append(result)
        print(
            f"{label:9} {result['run']}: {result['frames']} frames in {result['seconds']:.1f} s, "
            f"frame {result['frame_ms']:.2f} ms, work {result['work_ms']:.2f}, "
            f"scene pass {result['scene_ms']:.2f} (median {result['scene_median_ms']:.2f}); "
            f"sampler {result['spacing_ms']:.2f} ms apart; {module} in the pass "
            f"{result['dll_in_ms']:.3f} ms a frame ({result['dll_in_samples']} samples), "
            f"outside {result['dll_out_ms']:.3f} ({result['dll_out_samples']})"
        )
    print_table("modules inside the scene pass", results, "modules_in", opts["top"])
    print_table("modules outside the scene pass", results, "modules_out", opts["top"])
    print_table(f"{module} inside the scene pass", results, "functions_in", opts["top"])
    print_table(f"{module} outside the scene pass", results, "functions_out", opts["top"])
    if opts["json"]:
        with open(opts["json"], "w") as out:
            json.dump(results, out, indent=1)


if __name__ == "__main__":
    main()
