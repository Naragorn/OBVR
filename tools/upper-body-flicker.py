#!/usr/bin/env python3
"""The upper body frame by frame, from Debug.BodyTrace (game/BodyIk.cpp LogBodyTrace).

Between each hand script's marks: how often each arm's follow flag (f0/f1)
and the trunk hold (trunk 0/1) changed, and the largest change in a frame of
the chest correction, each clavicle, each elbow's bend and each wrist's
distance off its target. A flicker of the drawn upper body shows as flags
toggling or a value jumping back and forth over a few frames.

  tools/upper-body-flicker.py <run folder or OBVR.log>
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

MARK = re.compile(r"HandScript: mark (\S+)")
TRACE = re.compile(r"Body trace: f=(\d+) .* chest (\S+) .* trunk (\d) .*\| R f(\d) d\S+ bend (\S+) pole \S+ \S+ \S+ mv\d clav (\S+) off (\S+) "
                   r"\| L f(\d) d\S+ bend (\S+) pole \S+ \S+ \S+ mv\d clav (\S+) off (\S+) \|")
FIELDS = ("chest", "Rbend", "Rclav", "Roff", "Lbend", "Lclav", "Loff")


def windows(lines):
    name = "start"
    rows = []
    for line in lines:
        m = MARK.search(line)
        if m:
            yield name, rows
            name, rows = m.group(1), []
            continue
        t = TRACE.search(line)
        if t:
            g = t.groups()
            rows.append({
                "trunk": int(g[2]), "Rf": int(g[3]), "Lf": int(g[7]),
                "chest": float(g[1]), "Rbend": float(g[4]), "Rclav": float(g[5]), "Roff": float(g[6]),
                "Lbend": float(g[8]), "Lclav": float(g[9]), "Loff": float(g[10]),
            })
    yield name, rows


def main() -> int:
    path = Path(sys.argv[1])
    log = path / "OBVR.log" if path.is_dir() else path
    lines = log.read_text(errors="replace").splitlines()
    print("window      frames  toggles(trunk Rf Lf)  " + "  ".join("max d%s" % f for f in FIELDS))
    for name, rows in windows(lines):
        if len(rows) < 2:
            continue
        toggles = {k: sum(1 for a, b in zip(rows, rows[1:]) if a[k] != b[k]) for k in ("trunk", "Rf", "Lf")}
        steps = {k: max(abs(b[k] - a[k]) for a, b in zip(rows, rows[1:])) for k in FIELDS}
        print("%-10s %7d  %5d %3d %3d           " % (name, len(rows), toggles["trunk"], toggles["Rf"], toggles["Lf"])
              + "  ".join("%8.2f" % steps[f] for f in FIELDS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
