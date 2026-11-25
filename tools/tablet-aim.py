#!/usr/bin/env python3
"""The right hand's beam angles for a place on the tablet, measured: reads the
OBVR.log of a tablet-aim-grid.txt run (marks named y<yaw>p<pitch>, each
followed by a "HandScript: state" line with "laser hit 1 at X,Y"), fits the
script's pitch and yaw to the laser's hit in the layer's pixels (a quadratic
in x and y, least squares) and answers the angles for the places asked.

  tablet-aim.py OBVR.log X,Y [X,Y ...]   the angles for layer pixels
  tablet-aim.py OBVR.log --stats         tablet-stats-info.txt's lines: the
                                         stats tabs' icons and a column of
                                         rests down each tab's rows, placed
                                         by stats_menu.xml's own numbers
  tablet-aim.py OBVR.log --column X Y0 Y1 STEP WAIT
  tablet-aim.py OBVR.log --row Y X0 X1 STEP WAIT
                                         the beam stepped down a column or
                                         along a row of the page, WAIT seconds
                                         a step - in the page's own units (745
                                         by 792 from its top left corner, as
                                         the menus' XML counts)
  tablet-aim.py OBVR.log --at X Y        one place on the page, in its units

The fit holds for the hands' poses of the grid's run and the page's place in
the hand at that build, and inside the grid's hits only (the page; not what a
page shows beside itself); the worst error of the fit is printed.
"""
import re
import sys

import numpy as np


def read(path):
    """The grid's samples (yaw, pitch, x, y) and the stats page's rectangle in the layer's pixels."""
    samples = []
    page = None
    mark = None
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = re.search(r'Tablet: the Stats page read - the page at (\d+),(\d+)\.\.(\d+),(\d+) layer pixels', line)
            if m:
                page = tuple(int(v) for v in m.groups())
            m = re.search(r'HandScript: mark y(-?\d+)p(-?\d+) ', line)
            if m:
                mark = (int(m.group(1)), int(m.group(2)))
                continue
            m = re.search(r'HandScript: state .*laser hit (\d) at (-?\d+),(-?\d+)', line)
            if m and mark is not None:
                if m.group(1) == '1':
                    samples.append((mark[0], mark[1], int(m.group(2)), int(m.group(3))))
                mark = None
    return samples, page


def terms(x, y):
    return [1.0, x, y, x * y, x * x, y * y]


def fit(samples):
    a = np.array([terms(x, y) for _, _, x, y in samples])
    yaw = np.array([s[0] for s in samples], dtype=float)
    pitch = np.array([s[1] for s in samples], dtype=float)
    for_yaw = np.linalg.lstsq(a, yaw, rcond=None)[0]
    for_pitch = np.linalg.lstsq(a, pitch, rcond=None)[0]
    worst = (float(np.abs(a @ for_pitch - pitch).max()), float(np.abs(a @ for_yaw - yaw).max()))
    return for_pitch, for_yaw, worst


def stats_lines(aim, page):
    """The lines of tablet-stats-info.txt after its opening: each tab chosen by
    its icon, then the beam stepped down the tab's rows. stats_menu.xml: the
    page 745 by 792 units, the icons 135 by 85 at x 28, 165 and 304, y 594 +
    37; the rows between 75 and 580 down the page."""
    left, top, right, bottom = page
    ux = (right - left) / 745.0
    uy = (bottom - top) / 792.0

    def at(x, y):
        return aim(left + x * ux, top + y * uy)

    column = [at(330, y) for y in range(75, 580, 46)]
    out = []
    for name, x in (('general', 28), ('attributes', 165), ('skills', 304)):
        pitch, yaw = at(x + 67.5, 594 + 37 + 42.5)
        # The mark with the beam still on the icon: the film begins without a
        # box, and the first row's slides out half a second in.
        out += [f"# The {name} tab chosen by its icon, then the beam stepped down its rows, resting on each step.",
                f"right rot {pitch} {yaw} 0", "wait 0.8", "right trigger 1", "wait 0.3", "right trigger 0", "wait 1",
                f"mark {name}", "wait 0.5"]
        for pitch, yaw in column:
            out += [f"right rot {pitch} {yaw} 0", "wait 1.1"]
    return out


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    samples, page = read(sys.argv[1])
    if len(samples) < 8:
        sys.exit(f"{len(samples)} marks with a hit in {sys.argv[1]}: too few to fit")
    for_pitch, for_yaw, worst = fit(samples)

    def aim(x, y):
        row = np.array(terms(x, y))
        return round(float(row @ for_pitch), 1), round(float(row @ for_yaw), 1)

    note = f"{len(samples)} marks with a hit; the fit's worst error {worst[0]:.2f} degrees of pitch, {worst[1]:.2f} of yaw"
    mode = sys.argv[2]
    if mode in ('--stats', '--column', '--row', '--at'):
        if page is None:
            sys.exit("no \"Tablet: the Stats page read\" line: the page's rectangle is not known")
        left, top, right, bottom = page
        ux = (right - left) / 745.0
        uy = (bottom - top) / 792.0

        def on_page(x, y):
            return aim(left + x * ux, top + y * uy)

        numbers = [float(v) for v in sys.argv[3:]]
        if mode == '--stats':
            print(f"# {note}; the stats page at {left},{top}..{right},{bottom} layer pixels")
            print("\n".join(stats_lines(aim, page)))
        elif mode == '--at':
            pitch, yaw = on_page(numbers[0], numbers[1])
            print(f"right rot {pitch} {yaw} 0")
        else:
            fixed, start, end, step, wait = numbers
            count = int(abs(end - start) / step) + 1
            sign = 1.0 if end >= start else -1.0
            for i in range(count):
                moving = start + sign * step * i
                pitch, yaw = on_page(fixed, moving) if mode == '--column' else on_page(moving, fixed)
                print(f"right rot {pitch} {yaw} 0")
                print(f"wait {wait:g}")
        return
    print(note)
    for arg in sys.argv[2:]:
        x, y = (float(v) for v in arg.split(','))
        pitch, yaw = aim(x, y)
        print(f"{x:.0f},{y:.0f}: right rot {pitch} {yaw} 0")


if __name__ == "__main__":
    main()
