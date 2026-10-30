#!/usr/bin/env python3
"""Checks for tools/perf-functions.py: the function starts the exported table
lacks (OBNG, 2026-10-30) split the functions they hid inside.
Run with: python3 tools/perf-functions-test.py"""
import importlib.util
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("perf_functions", here / "perf-functions.py")
pf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pf)

failures = 0


def check(condition, what):
    global failures
    print("  %-5s %s" % ("ok" if condition else "FAIL", what))
    if not condition:
        failures += 1


starts, names = pf.load_functions(here / "oblivion-1.2.0.416-functions.tsv.gz")
check(pf.function_of(0x0077A300, starts) == 0x0077A1F0, "SetupShaderPrograms keeps its own body")
check(pf.function_of(0x0077A310, starts) == 0x0077A310, "PrepareGeometryForRendering starts at 0x0077A310")
check(pf.function_of(0x0077A400, starts) == 0x0077A310, "and its body is its own, not SetupShaderPrograms'")
check(0x0077A310 in pf.KNOWN and pf.KNOWN[0x0077A310][1] == "stream packing", "it counts as stream packing")
check(starts == sorted(starts) and len(starts) == len(set(starts)), "the starts stay sorted, each once")
sys.exit(1 if failures else 0)
