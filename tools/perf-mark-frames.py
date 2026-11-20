#!/usr/bin/env python3
"""The longest frame after each of a hand script's marks, and whose it was: for
a hitch tied to a moment the script makes (a menu opened by the left B, a
door), from a harness run folder holding OBVR.log and OBVR-performance-1 with
the render sampler's samples.csv ([Performance] Sampler=1).

For every mark named preN or heldN (tablet-open-hitch.txt's), the longest frame
within 1.5 s after it - its length, how long after the mark it began - and the
engine functions on the sampled stacks inside it (inclusive: a function
counted once a sample; exclusive: where the eip was), named by the function
table (tools/oblivion-1.2.0.416-functions.tsv.gz). The profiler's clock runs
at 10 MHz on Proton (QueryPerformanceFrequency there). Written 2026-11-20 for
the Tab menus' opening hitch (docs/inventory-tablet-spec.md section 13).

Read the inclusive names with care: the stack words are found by a call
before them, not by a frame walk, and stale words come along - in the menu
hitch they named Havok's 0x00893B90 chain while the eip sat in the stand-in's
Sleep inside WaitGetPoses. The exclusive line and the profiler's own spans
(cpu_events.csv) decide.

Usage: perf-mark-frames.py <run folder> tools/oblivion-1.2.0.416-functions.tsv.gz
"""
import csv, gzip, re, sys, bisect, collections
run = sys.argv[1]
cap = run + "/OBVR-performance-1"
FREQ = 1e7
marks = {}
for line in open(run + "/OBVR.log", errors="replace"):
    m = re.search(r"HandScript: mark (\S+) \(.*tick (\d+)\)", line)
    if m: marks[m.group(1)] = int(m.group(2))
frames = []
for r in csv.DictReader(open(cap + "/frames.csv")):
    s, e = int(r["start_tick"]), int(r["end_tick"])
    iv = int(r["interval_tick"])
    frames.append((s, iv, r["delivered_mode"]))
frames.sort()
starts = [f[0] for f in frames]
# functions table
funcs = []
for row in csv.reader(gzip.open(sys.argv[2], "rt"), delimiter="\t"):
    try: funcs.append((int(row[0], 16), row[1]))
    except Exception: pass
funcs.sort(); fa = [f[0] for f in funcs]
def name(a):
    i = bisect.bisect_right(fa, a) - 1
    return funcs[i][1] if i >= 0 else "?"
samples = []
for r in csv.DictReader(open(cap + "/samples.csv")):
    samples.append((int(r["tick"]), r))
samples.sort(); st = [s[0] for s in samples]
res = {}
for mk in [k for k in marks if re.match(r"(pre|held)\d", k)]:
    t0 = marks[mk]; t1 = t0 + int(1.5 * FREQ)
    i0 = bisect.bisect_left(starts, t0); i1 = bisect.bisect_left(starts, t1)
    best = max(range(i0, i1 - 1), key=lambda i: starts[i + 1] - starts[i])
    dur = (starts[best + 1] - starts[best]) / FREQ * 1000
    a, b = starts[best], starts[best + 1]
    j0 = bisect.bisect_left(st, a); j1 = bisect.bisect_left(st, b)
    incl = collections.Counter(); excl = collections.Counter()
    for _, r in samples[j0:j1]:
        seen = set()
        for k in ["eip"] + ["w%d" % n for n in range(8)]:
            v = r[k]
            if not v: continue
            a_ = int(v, 16)
            if not (0x400000 <= a_ < 0xC00000): continue
            n = name(a_)
            if k == "eip": excl[n] += 1
            if n not in seen: incl[n] += 1; seen.add(n)
    res[mk] = (dur, j1 - j0, incl.most_common(8), excl.most_common(4), (a - t0) / FREQ * 1000)
for mk in sorted(res, key=lambda k: (k[:-1], k[-1])):
    dur, n, incl, excl, off = res[mk]
    print("%s: longest frame %.1f ms, %.0f ms after the mark, %d samples" % (mk, dur, off, n))
    print("   inclusive:", ", ".join("%s %d" % x for x in incl))
    print("   exclusive:", ", ".join("%s %d" % x for x in excl))
