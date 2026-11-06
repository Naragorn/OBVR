#!/usr/bin/env python3
"""Reads a run's "Lockpick trace:" lines ([Debug] LockpickTrace,
game/LockpickTrace.h; tools/hand-scripts/lockpick-trace.txt).

Prints, window by window between the hand script's marks:
- where the pick's tumbler changed: the frame, the tumbler before and after,
  and the cursor's x at the change, in the layer's pixels and in the menu's
  units (the background's x and width give the units: the menu is centred);
- beside each the x where that tumbler is drawn, so the two can be compared;
- how the pick's height followed the cursor's (each cursor y with the pick's
  lowest and highest y seen at it).

Usage: tools/lockpick-trace.py <run folder or OBVR.log> [--layer-width 2568]
"""

import argparse
import os
import re
import sys

MARK = re.compile(r"^HandScript: mark (\S+)")
LINE = re.compile(
    r"^Lockpick trace: f=(\d+) cursor (\d) (\S+) (\S+) engine (\d) (\S+) (\S+) laser (\d) (\S+) (\S+) \| "
    r"pick (\d) x (\S+) y (\S+) place (\S+) tumbler (\d+) \| background (\d) x (\S+) y (\S+) w (\S+) h (\S+)")

# lockpick_menu.xml: each tumbler's cover is 51 units wide from these x, under
# the background.
TUMBLER_LEFT = (292.0, 343.0, 394.0, 444.0, 495.0)
TUMBLER_WIDTH = 51.0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("run")
    parser.add_argument("--layer-width", type=float, default=2568.0)
    args = parser.parse_args()
    path = os.path.join(args.run, "OBVR.log") if os.path.isdir(args.run) else args.run
    window = "start"
    rows = []
    with open(path, encoding="utf-8", errors="replace") as log:
        for line in log:
            mark = MARK.match(line)
            if mark:
                window = mark.group(1)
                continue
            m = LINE.match(line)
            if not m:
                continue
            g = m.groups()
            rows.append({
                "window": window, "f": int(g[0]), "cx": float(g[2]), "cy": float(g[3]),
                "laser": g[7] == "1", "px": float(g[11]), "py": float(g[12]), "place": float(g[13]),
                "tumbler": int(g[14]), "bx": float(g[16]), "bw": float(g[18]),
            })
    if not rows:
        print("no Lockpick trace lines", file=sys.stderr)
        return 1
    bx, bw = rows[-1]["bx"], rows[-1]["bw"]
    units_wide = 2.0 * bx + bw  # the menu is centred on the screen
    scale = args.layer_width / units_wide
    print(f"{len(rows)} lines; the menu's screen is {units_wide:.1f} units wide, {scale:.4f} layer pixels a unit; "
          f"its middle at {units_wide / 2:.1f} units")
    print("where each tumbler is drawn (units, then pixels):")
    for i, left in enumerate(TUMBLER_LEFT):
        a, b = bx + left, bx + left + TUMBLER_WIDTH
        print(f"  tumbler {i + 1}: {a:.0f}..{b:.0f} units, {a * scale:.0f}..{b * scale:.0f} px, middle {(a + b) / 2 * scale:.0f} px")
    print("where the pick's tumbler changed:")
    before = rows[0]
    for row in rows[1:]:
        if row["tumbler"] != before["tumbler"]:
            print(f"  {row['window']:<10} f={row['f']:<5} tumbler {before['tumbler']} -> {row['tumbler']} with the cursor at "
                  f"x {row['cx']:.0f} px ({row['cx'] / scale:.0f} units, {row['cx'] / scale - units_wide / 2:+.0f} from "
                  f"the middle), y {row['cy']:.0f}")
        before = row
    print("the pick's height against the cursor's (cursor y: the pick's y from..to, frames):")
    seen = {}
    order = []
    for row in rows:
        key = (row["window"], round(row["cy"]))
        if key not in seen:
            seen[key] = [row["py"], row["py"], 0]
            order.append(key)
        s = seen[key]
        s[0] = min(s[0], row["py"])
        s[1] = max(s[1], row["py"])
        s[2] += 1
    for key in order:
        if key[0] in ("up", "down"):
            s = seen[key]
            print(f"  {key[0]:<6} cursor y {key[1]:>5}: pick y {s[0]:.0f}..{s[1]:.0f} ({s[2]} frames)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
