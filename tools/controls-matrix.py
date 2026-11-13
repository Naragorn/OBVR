#!/usr/bin/env python3
"""Reads the controls matrix's runs (tools/hand-scripts/controls-matrix-*.txt)
into one table: per swapped action its new input, whether it fired there,
whether its old input stayed free of it, and whether the rest of the run went
as expected.

  python3 tools/controls-matrix.py artifacts/hand-script/controls-matrix-1/<run> artifacts/hand-script/controls-matrix-2/<run>

A run folder (with its OBVR.log) or an OBVR.log itself; which run it is comes
from the folder's script.txt or the log's binding line. Exit 1 when a row is
not ok or a run is missing.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from controls_matrix_table import PHRASE, RUNS, fire_line  # noqa: E402


def read_log(path):
    log = os.path.join(path, "OBVR.log") if os.path.isdir(path) else path
    with open(log, encoding="utf-8", errors="replace") as f:
        return f.read()


def run_of(path, text):
    script = os.path.join(path, "script.txt") if os.path.isdir(path) else None
    if script and os.path.exists(script):
        with open(script, encoding="utf-8", errors="replace") as f:
            head = f.read(400)
        for run in RUNS:
            if run["name"] in head:
                return run
    for run in RUNS:
        if run["config"] in text:
            return run
    return None


def rows_for(run, text):
    """One row per step: (action or '-', input, fired, old free, note)."""
    rows = []
    for step in run["steps"]:
        src = step.get("as", step["input"])
        if step["fires"] is not None:
            fired = fire_line(step["fires"], src) in text
        else:
            fired = None  # nothing expected
        free = all(fire_line(a, src) not in text for a in step["not_fires"]) if step["not_fires"] else None
        for extra in step.get("extra", []):
            fired = fired and extra in text
        rows.append((step["fires"] or "-", PHRASE[src], fired, free, ",".join(step["not_fires"])))
    return rows


def rest_ok(run, text):
    """The binding line, its notes, the script finished, no warning."""
    missing = [line for line in [run["config"], *run["notes"], "HandScript: finished"] if line not in text]
    if "HandScript: WARNING" in text:
        missing.append("a HandScript WARNING")
    return missing


def word(v):
    return "-" if v is None else ("yes" if v else "NO")


def table(results):
    lines = ["| Run | Action | Input | fired | old input free (of) | rest ok |", "|---|---|---|---|---|---|"]
    ok = True
    for run, rows, missing in results:
        rest = "yes" if not missing else "NO: " + "; ".join(missing)
        ok = ok and not missing
        for action, src, fired, free, of in rows:
            ok = ok and fired is not False and free is not False
            lines.append("| %s | %s | %s | %s | %s%s | %s |" % (
                run["name"][-1], action, src, word(fired), word(free), " (%s)" % of if of else "", rest))
    return "\n".join(lines), ok


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    results = []
    seen = set()
    for path in argv:
        text = read_log(path)
        run = run_of(path, text)
        if run is None:
            print("not a controls-matrix run:", path)
            return 1
        seen.add(run["name"])
        results.append((run, rows_for(run, text), rest_ok(run, text)))
    out, ok = table(results)
    print(out)
    for run in RUNS:
        if run["name"] not in seen:
            print("missing run:", run["name"])
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
