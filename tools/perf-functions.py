#!/usr/bin/env python3
"""Which engine FUNCTIONS the render thread spent its time in, from
OBVR-samples.csv (the render sampler, render/RenderSampler.h) and the
engine's function table (tools/oblivion-1.2.0.416-functions.tsv.gz: every
function start Ghidra 12.1.4 found in Oblivion.exe 1.2.0.416, exported with
REA on 2026-10-23; a function is taken to run to the next start).

tools/perf-samples.py counts 256-byte bins; this counts functions, names the
ones that are known, and groups them into the areas a performance plugin
could act on - the engine's allocator, its critical sections, the CRT's
memcpy, the stream packing - so that "where does the engine's 74 % go" has
an answer by function rather than by address.

Per phase (between consecutive marks of the run's OBVR.log):
  exclusive - the function the eip was in (the code that was running),
              with modules outside the exe as one line each
  inclusive - the engine functions whose return addresses were on the stack
              (the callers whose time it was; a function once per sample)
  areas     - the exclusive samples summed by area (annotations below)

Usage: perf-functions.py <samples.csv> <run folder> mark1 mark2 [...]
                         [--functions table.tsv.gz] [--top 30] [--in-pass only|out|all]
"""
import bisect
import csv
import gzip
import re
import sys
from collections import Counter
from pathlib import Path

EXE_CODE = (0x00401000, 0x00A28000)

# Functions read in the disassembly (docs/performance-spec.md section 2,
# docs/performance-profiling.md, this file's first use on 2026-10-23).
KNOWN = {
    0x00763130: ("stream pack: geometry's streams into its render record", "stream packing"),
    0x0076FE20: ("stream pack: the packer", "stream packing"),
    0x0076F590: ("stream pack: the copier loops", "stream packing"),
    0x00776C90: ("stream pack: lock wrapper (staging buffer)", "stream packing"),
    0x00776D80: ("stream pack: unlock (memcpy into the mapped buffer)", "stream packing"),
    0x0047D170: ("TimeInfo::Update (the frame clock)", "frame clock"),
    0x0077A1F0: ("NiD3DShader::SetupShaderPrograms (vtable slot 14)", "shader setup"),
    0x0077A310: ("PrepareGeometryForRendering (shader slot 15): pack + bind streams", "stream packing"),
    0x00767B40: ("shape render: the whole-geometry pack arm under 0x0077A310", "stream packing"),
}

# Function starts the exported table lacks (OBNG, 2026-10-30: none between
# 0x0077A1F0 and 0x0077A4A0, so PrepareGeometryForRendering counted as
# SetupShaderPrograms - 27.3 % that was 13.0 % setup and 14.3 % pack + bind).
EXTRA_STARTS = {
    0x0077A310: "PrepareGeometryForRendering",
    # Right behind 0x007AD270's second jump table (0x007AE020): 3806 of the
    # 4419 samples the table gave 0x007AD270 lie in its body (OBNG, run
    # obng-walkers-original/20261030-172648).
    0x007AE070: "BSShaderAccumulator finish (vtable +0x50)",
}

# Areas by the function's shape, decided in annotate(): the CRT's copy
# routines, the engine's allocator, the critical-section wrappers.
AREA_PATTERNS = [
    (re.compile(r"memcpy|memmove|memset"), "memcpy"),
    (re.compile(r"alloc|free|heap", re.I), "allocator"),
    (re.compile(r"critical", re.I), "critical sections"),
]


def parse_args(argv):
    opts = {"top": 30, "in_pass": "all", "functions": None}
    rest = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("--top", "--in-pass", "--functions") and i + 1 < len(argv):
            key = a[2:].replace("-", "_")
            opts[key] = int(argv[i + 1]) if key == "top" else argv[i + 1]
            i += 2
            continue
        rest.append(a)
        i += 1
    if len(rest) < 2:
        return None, None, [], opts
    return rest[0], rest[1], rest[2:], opts


def load_functions(path):
    """Sorted function starts and {start: name}; the table's rows are
    address, name, thunk."""
    starts, names = [], {}
    opener = gzip.open if str(path).endswith(".gz") else open
    with opener(path, "rt") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 2:
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            starts.append(addr)
            names[addr] = parts[1]
    for addr, name in EXTRA_STARTS.items():
        if addr not in names:
            starts.append(addr)
            names[addr] = name
    starts.sort()
    return starts, names


def function_of(addr, starts):
    """The function start an engine address lies in, or None outside the
    engine's code."""
    if not (EXE_CODE[0] <= addr < EXE_CODE[1]):
        return None
    i = bisect.bisect_right(starts, addr) - 1
    return starts[i] if i >= 0 else None


def mark_ticks(log_lines):
    ticks = {}
    for line in log_lines:
        m = re.search(r"HandScript: mark (\S+) \(.*tick (\d+)\)", line)
        if m:
            ticks[m.group(1)] = int(m.group(2))
    return ticks


def module_table(rows):
    table = {}
    for r in rows:
        try:
            table[int(r["page"], 16)] = (r["module"], int(r["base"], 16))
        except (KeyError, ValueError):
            continue
    return table


def module_of(eip, table):
    if EXE_CODE[0] <= eip < EXE_CODE[1]:
        return "Oblivion.exe"
    if EXE_CODE[1] <= eip < 0x00C00000:
        return "Oblivion.exe data"
    entry = table.get(eip >> 16)
    if entry and entry[0] != "?":
        return entry[0]
    return "outside every module"


def wanted(sample, in_pass):
    if in_pass == "only":
        return sample["in_pass"] == "1"
    if in_pass == "out":
        return sample["in_pass"] != "1"
    return True


def annotate(start, names):
    """The function's label and area."""
    if start in KNOWN:
        return KNOWN[start]
    name = names.get(start, "FUN_%08X" % start)
    for pattern, area in AREA_PATTERNS:
        if pattern.search(name):
            return (name, area)
    return (name, "engine, unsorted")


def aggregate(samples, starts, names, table):
    exclusive = Counter()   # key: ("fn", start) or ("module", name)
    inclusive = Counter()   # key: start
    areas = Counter()
    callers = {}            # exclusive key -> Counter of the innermost engine caller's function
    for s in samples:
        eip = int(s["eip"], 16)
        fn = function_of(eip, starts)
        if fn is not None:
            key = ("fn", fn)
            areas[annotate(fn, names)[1]] += 1
        else:
            module = module_of(eip, table)
            key = ("module", module)
            areas[module] += 1
        exclusive[key] += 1
        seen = set()
        chain = []
        for k in range(8):
            w = s.get("w%d" % k, "")
            if not w:
                continue
            f = function_of(int(w, 16), starts)
            if f is not None:
                seen.add(f)
                chain.append(f)
        for f in seen:
            inclusive[f] += 1
        # The caller: the innermost engine return address that is not the
        # function itself (a return address inside the running function is
        # a call it made that already returned, or recursion).
        caller = next((f for f in chain if key[0] != "fn" or f != key[1]), None)
        if caller is not None:
            callers.setdefault(key, Counter())[caller] += 1
    return exclusive, inclusive, areas, callers


def main():
    samples_path, run, marks, opts = parse_args(sys.argv[1:])
    if samples_path is None:
        raise SystemExit(__doc__)
    here = Path(__file__).resolve().parent
    table_path = Path(opts["functions"]) if opts["functions"] else here / "oblivion-1.2.0.416-functions.tsv.gz"
    starts, names = load_functions(table_path)
    log = (Path(run) / "OBVR.log").read_text(encoding="latin-1").splitlines()
    ticks = mark_ticks(log)
    rows = list(csv.DictReader(open(samples_path, newline="")))
    modules_path = Path(samples_path).with_name(Path(samples_path).stem + "-modules.csv")
    table = module_table(csv.DictReader(open(modules_path, newline=""))) if modules_path.exists() else {}
    print("%d functions in the table" % len(starts))
    for a, b in zip(marks, marks[1:]):
        if a not in ticks or b not in ticks:
            print("## %s to %s: mark missing in the log" % (a, b))
            continue
        phase = [r for r in rows if ticks[a] <= int(r["tick"]) < ticks[b] and wanted(r, opts["in_pass"])]
        n = len(phase)
        print("## %s to %s: %d samples (%s)" % (a, b, n, opts["in_pass"]))
        if not phase:
            continue
        exclusive, inclusive, areas, callers = aggregate(phase, starts, names, table)
        print("   areas (exclusive):")
        for area, c in areas.most_common():
            print("     %5.1f%%  %6d  %s" % (100.0 * c / n, c, area))
        print("   exclusive, by function:")
        for key, c in exclusive.most_common(opts["top"]):
            if key[0] == "fn":
                label, area = annotate(key[1], names)
                print("     %08X  %5.1f%%  %6d  %s  [%s]" % (key[1], 100.0 * c / n, c, label, area))
            else:
                print("     --------  %5.1f%%  %6d  %s" % (100.0 * c / n, c, key[1]))
            # The callers of the ten hottest, three each: who the time belongs to.
            if c >= exclusive.most_common(10)[-1][1] and key in callers:
                for f, cc in callers[key].most_common(3):
                    print("               %5.1f%%  from %08X %s" % (100.0 * cc / c, f, annotate(f, names)[0]))
        print("   inclusive, by engine function on the stack:")
        for start, c in inclusive.most_common(opts["top"]):
            label, area = annotate(start, names)
            print("     %08X  %5.1f%%  %6d  %s" % (start, 100.0 * c / n, c, label))


if __name__ == "__main__":
    main()
