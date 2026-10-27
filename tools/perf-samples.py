#!/usr/bin/env python3
"""Where the render thread spent its time, from OBVR-samples.csv (the render
sampler, render/RenderSampler.h) cut into the phases of a hand script by the
marks' ticks (OBVR.log of the run).

Each sample is the thread's eip and up to eight return addresses into
Oblivion.exe's code found on its stack, innermost first (since 2026-10-27 only
words with a call before them), and up to four return addresses in other
modules (x0..x3), named by the modules file. Two counts per phase:
  exclusive - where the eip was, by address bin (the code that was running)
  inclusive - which engine return addresses were on the stack (the callers
              whose time it was, a frame counted once per sample)
Addresses are raw; name them with objdump or Ghidra.

Usage: perf-samples.py <samples.csv> <run folder> mark1 mark2 [...]
                       [--bin 256] [--top 25] [--in-pass only|out|all]
"""
import csv
import re
import sys
from collections import Counter
from pathlib import Path


def parse_args(argv):
    """The samples file, the run folder, the marks, and the options."""
    opts = {"bin": 256, "top": 25, "in_pass": "all"}
    rest = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("--bin", "--top", "--in-pass") and i + 1 < len(argv):
            key = a[2:].replace("-", "_")
            opts[key] = int(argv[i + 1]) if key != "in_pass" else argv[i + 1]
            i += 2
            continue
        rest.append(a)
        i += 1
    if len(rest) < 2:
        return None, None, [], opts
    return rest[0], rest[1], rest[2:], opts


def mark_ticks(log_lines):
    ticks = {}
    for line in log_lines:
        m = re.search(r"HandScript: mark (\S+) \(.*tick (\d+)\)", line)
        if m:
            ticks[m.group(1)] = int(m.group(2))
    return ticks


def wanted(sample, in_pass):
    """Whether a sample row counts under the --in-pass choice."""
    if in_pass == "only":
        return sample["in_pass"] == "1"
    if in_pass == "out":
        return sample["in_pass"] != "1"
    return True


def aggregate(samples, bin_size):
    """Exclusive counts by eip bin and inclusive counts by return address
    (each address once per sample), the engine sites that called out of the
    engine (the innermost engine return address of every sample whose eip
    lies outside Oblivion.exe - the D3D9 call sites, mostly), plus the
    sample count and how many had a scene pass running."""
    exclusive = Counter()
    inclusive = Counter()
    callers_out = Counter()
    outside_first = Counter()
    outside_inclusive = Counter()
    in_pass = 0
    for s in samples:
        eip = int(s["eip"], 16)
        exclusive[eip - eip % bin_size] += 1
        if s["in_pass"] == "1":
            in_pass += 1
        seen = set()
        chain = []
        for k in range(8):
            w = s.get("w%d" % k, "")
            if w:
                chain.append(int(w, 16))
                seen.add(int(w, 16))
        for w in seen:
            inclusive[w] += 1
        if chain and not (0x00401000 <= eip < 0x00A28000):
            callers_out[chain[0]] += 1
        # since 2026-10-27: return addresses in other modules (x0..x3, the
        # innermost first; each checked for a call before it by the sampler)
        outside = [int(s["x%d" % k], 16) for k in range(4) if s.get("x%d" % k, "")]
        if outside:
            outside_first[outside[0]] += 1
        for w in set(outside):
            outside_inclusive[w] += 1
    return {"count": len(samples), "in_pass": in_pass, "exclusive": exclusive, "inclusive": inclusive,
            "callers_out": callers_out, "outside_first": outside_first, "outside_inclusive": outside_inclusive}


def module_table(rows):
    """The sampler's module file (OBVR-samples-modules.csv beside the samples:
    page, module, base) as {page: (module, base)}; a module of "?" is a page
    no loaded module claims."""
    table = {}
    for r in rows:
        try:
            table[int(r["page"], 16)] = (r["module"], int(r["base"], 16))
        except (KeyError, ValueError):
            continue
    return table


def module_of(eip, table=None):
    """Where an eip lies: the engine's code, its data, a named module with
    the offset into it (from the module table), or outside everything known
    (the kernel's wait, a mapping no module claims)."""
    if 0x00401000 <= eip < 0x00A28000:
        return "Oblivion.exe"
    if 0x00A28000 <= eip < 0x00C00000:
        return "Oblivion.exe data"
    if table:
        entry = table.get(eip >> 16)
        if entry and entry[0] != "?":
            return entry[0]
    return "outside the exe"


def name_address(addr, table=None):
    """module+offset for an address outside the engine when the table knows
    the module, else the bare address."""
    if table:
        entry = table.get(addr >> 16)
        if entry and entry[0] != "?" and entry[1]:
            return "%s+%X" % (entry[0], addr - entry[1])
    return "%08X" % addr


def main():
    samples_path, run, marks, opts = parse_args(sys.argv[1:])
    if samples_path is None:
        raise SystemExit(__doc__)
    log = (Path(run) / "OBVR.log").read_text(encoding="latin-1").splitlines()
    ticks = mark_ticks(log)
    rows = list(csv.DictReader(open(samples_path, newline="")))
    modules_path = Path(samples_path).with_name(Path(samples_path).stem + "-modules.csv")
    table = module_table(csv.DictReader(open(modules_path, newline=""))) if modules_path.exists() else {}
    for a, b in zip(marks, marks[1:]):
        if a not in ticks or b not in ticks:
            print("## %s to %s: mark missing in the log" % (a, b))
            continue
        phase = [r for r in rows if ticks[a] <= int(r["tick"]) < ticks[b] and wanted(r, opts["in_pass"])]
        agg = aggregate(phase, opts["bin"])
        print("## %s to %s: %d samples, %d with a scene pass running (%.0f%%)" % (
            a, b, agg["count"], agg["in_pass"], 100.0 * agg["in_pass"] / max(1, agg["count"])))
        if not phase:
            continue
        by_module = Counter(module_of(int(r["eip"], 16), table) for r in phase)
        print("   eip by module: " + ", ".join("%s %.0f%%" % (m, 100.0 * c / agg["count"]) for m, c in by_module.most_common()))
        print("   exclusive (eip bins of %d bytes):" % opts["bin"])
        for addr, c in agg["exclusive"].most_common(opts["top"]):
            print("     %08X  %5.1f%%  %d  %s" % (addr, 100.0 * c / agg["count"], c, name_address(addr, table)))
        print("   inclusive (engine return addresses on the stack):")
        for addr, c in agg["inclusive"].most_common(opts["top"]):
            print("     %08X  %5.1f%%  %d" % (addr, 100.0 * c / agg["count"], c))
        print("   engine sites that had called out of the exe (innermost engine return address):")
        for addr, c in agg["callers_out"].most_common(opts["top"]):
            print("     %08X  %5.1f%%  %d" % (addr, 100.0 * c / agg["count"], c))
        if agg["outside_first"]:
            print("   innermost return address outside the exe (who the eip outside it was called by):")
            for addr, c in agg["outside_first"].most_common(opts["top"]):
                print("     %08X  %5.1f%%  %d  %s" % (addr, 100.0 * c / agg["count"], c, name_address(addr, table)))
            print("   return addresses outside the exe (inclusive):")
            for addr, c in agg["outside_inclusive"].most_common(opts["top"]):
                print("     %08X  %5.1f%%  %d  %s" % (addr, 100.0 * c / agg["count"], c, name_address(addr, table)))


if __name__ == "__main__":
    main()
