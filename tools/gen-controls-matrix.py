#!/usr/bin/env python3
"""Writes the controls matrix's hand scripts from tools/controls_matrix_table.py:

  python3 tools/gen-controls-matrix.py            # tools/hand-scripts/controls-matrix-1.txt, -2.txt
  python3 tools/gen-controls-matrix.py --check    # exit 1 when a written script is not what the table makes

Each run lays its [Buttons] over OBVR_Controls.ini through the run's own
OBVR-test.ini (`ini Buttons ...`, core/Config.cpp ReadButtonBindingsStep), so
the live OBVR_Controls.ini is never touched. Read the runs with
tools/controls-matrix.py.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from controls_matrix_table import PRESS, RUNS, fire_line  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def script_for(run):
    out = []
    w = out.append
    w("# The controls matrix, %s (written by tools/gen-controls-matrix.py from" % run["name"])
    w("# tools/controls_matrix_table.py - edit the table, not this file): %s." % run["about"])
    w("# Every press is checked by OBVR's own line \"Buttons: <Action> from the <input>\":")
    w("# the action on its new input fires, its old input fires it no more, the rest as before.")
    w("# Run with: python3 tools/hand-script-run.py --headless tools/hand-scripts/%s.txt" % run["name"])
    w("# Read with: python3 tools/controls-matrix.py <the run folders>")
    w("")
    w("ini Hands Enabled=1")
    w("ini Hands Kick=1")
    w("ini Hands MenuHoldSeconds=1.25")
    w("ini Hands MenuRingSeconds=0.25")
    w("ini Hands MenuTapSeconds=0.25")
    w("ini Hands StickHoldSeconds=0.4")
    for line in run["hands"]:
        w("ini Hands %s" % line)
    for line in run["buttons"]:
        w("ini Buttons %s" % line)
    w("console-at settle CloseAllMenus")
    closes = [i for i, s in enumerate(run["steps"]) if s.get("close")]
    for i in closes:
        w("console-at closed-%d CloseAllMenus" % (i + 1))
    w("")
    w("expect HandScript: finished")
    w("expect %s" % run["config"])
    for note in run["notes"]:
        w("expect %s" % note)
    fired = set()
    for step in run["steps"]:
        if step["fires"] is not None:
            line = fire_line(step["fires"], step["input"])
            if line not in fired:
                w("expect %s" % line)
                fired.add(line)
        for extra in step.get("extra", []):
            w("expect %s" % extra)
    rejected = set()
    for step in run["steps"]:
        for action in step["not_fires"]:
            line = fire_line(action, step["input"])
            if line not in rejected and line not in fired:
                w("reject %s" % line)
                rejected.add(line)
    w("reject HandScript: WARNING")
    w("")
    w("wait 12")
    w("right pos 0.20 -0.40 -0.30")
    w("left pos -0.20 -0.40 -0.30")
    w("mark settle")
    w("wait 2")
    for i, step in enumerate(run["steps"]):
        n = i + 1
        w("# %d: %s -> %s" % (n, step["input"], step["fires"] or "nothing"))
        for line in PRESS[step["input"]]:
            w(line)
        w("wait 1.5")
        w("mark step-%d" % n)
        if step.get("close"):
            w("mark closed-%d" % n)
            w("wait 1.5")
    w("wait 2")
    w("mark end")
    return "\n".join(out) + "\n"


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--check", action="store_true", help="compare instead of writing")
    p.add_argument("--dir", default=os.path.join(ROOT, "tools", "hand-scripts"))
    args = p.parse_args(argv)
    stale = []
    for run in RUNS:
        path = os.path.join(args.dir, run["name"] + ".txt")
        text = script_for(run)
        if args.check:
            try:
                with open(path, encoding="utf-8") as f:
                    if f.read() != text:
                        stale.append(path)
            except FileNotFoundError:
                stale.append(path)
        else:
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
            print("wrote", path)
    if stale:
        print("not what the table makes (run tools/gen-controls-matrix.py):", *stale)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
