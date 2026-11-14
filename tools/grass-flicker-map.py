#!/usr/bin/env python3
"""Where the grass flickers: a per-pixel map of the grass-flicker runs.

For each run and mark it takes one eye's frames in a row ([Debug]
EyeDumpFrames) and computes, per pixel, the mean second difference
|f[i+1] - 2 f[i] + f[i-1]| in grey 0..255: a steady drift of the view
cancels in it, a value that jumps back and forth does not. It prints the
picture's mean, the mean of each box, and how concentrated the flicker is
(the share of pixels above the threshold and the share of all flicker they
carry), and writes the map as a picture beside the frames
(flicker-<eye>-<mark>.png, still pixels dark).

The question it was written for (Nadi, 2026-11-14): does the grass flicker
where it crosses other grass, stones and the ground, or at every blade's
edge that the wind moves? A run with the wind and one without it, read side
by side, part the two.

With --overlap it reads a grass-overlap run (tools/hand-scripts/
grass-overlap.txt): the series wind (the head still, the wind blowing), calm
(the wind at 0) and bare (the grass switched off). The pixels the picture
loses with the grass gone are the grass; they are parted into the grass with
only grass around it (inside: a dense tuft, blade before blade) and the grass
within --reach pixels of what is no grass (edge: blades before the ground, a
stone, a trunk), and the flicker of each is printed with and without the wind,
with a verdict. It exits 1 when the run measured nothing: too little grass
in view, a frozen picture, a series missing.

Usage: tools/grass-flicker-map.py <run folder> [<run folder> ...]
       [--marks still,turn] [--eye Right] [--boxes name:left:top:right:bottom,...]
       [--threshold 10] [--full 40]
       tools/grass-flicker-map.py --overlap <run folder> [<run folder> ...]
       [--eye Right] [--mask-threshold 8] [--reach 3] [--full 40]
"""

import argparse
import os
import sys

import numpy as np


def flicker_map(frames):
    """The mean second difference of each pixel over a series
    [frame, row, column]; None with fewer than three frames."""
    if frames is None or len(frames) < 3:
        return None
    return np.abs(frames[2:] - 2 * frames[1:-1] + frames[:-2]).mean(axis=0)


def concentration(flicker, threshold):
    """(share of the pixels above the threshold, share of all the flicker
    those pixels carry); (0, 0) for a still picture."""
    total = float(flicker.sum())
    if total <= 0.0:
        return (0.0, 0.0)
    hot = flicker > threshold
    return (float(hot.mean()), float(flicker[hot].sum()) / total)


def parse_boxes(text):
    """name:left:top:right:bottom,... as a list of tuples; ValueError for a
    box that lacks an edge or holds no pixel."""
    boxes = []
    for item in filter(None, text.split(",")):
        parts = item.split(":")
        if len(parts) != 5:
            raise ValueError(f"box '{item}': want name:left:top:right:bottom")
        left, top, right, bottom = (int(p) for p in parts[1:])
        if right <= left or bottom <= top:
            raise ValueError(f"box '{item}': holds no pixel")
        boxes.append((parts[0], left, top, right, bottom))
    return boxes


def box_mean(flicker, box):
    """The mean of the map inside a box, None when the box misses the picture."""
    _, left, top, right, bottom = box
    cut = flicker[top:bottom, left:right]
    return float(cut.mean()) if cut.size else None


def heat(flicker, full):
    """The map as 8-bit grey: 0 still, 255 at the full value and above."""
    return np.clip(flicker / full * 255.0, 0, 255).astype(np.uint8)


def grass_mask(with_grass, bare, threshold):
    """The pixels whose mean over a series with the grass differs from the
    mean over one without it by more than the threshold and three times
    what the pixel moves by itself without grass (a leaf in the wind differs
    between any two series); None when a series is missing or the two are
    of different sizes."""
    if with_grass is None or bare is None or with_grass.shape[1:] != bare.shape[1:]:
        return None
    apart = np.abs(with_grass.mean(axis=0) - bare.mean(axis=0))
    return apart > threshold + 3.0 * bare.std(axis=0)


def _spread(mask, reach):
    """True where a True pixel lies within `reach` pixels (a square); the
    picture's rim continues the pixel at it."""
    padded = np.pad(mask, reach, mode="edge")
    rows, columns = mask.shape
    out = np.zeros_like(mask)
    for dy in range(2 * reach + 1):
        for dx in range(2 * reach + 1):
            out |= padded[dy:dy + rows, dx:dx + columns]
    return out


def zones(mask, reach):
    """(inside, edge, outside): grass with only grass within reach, grass
    with something bare within reach, and the bare pixels out of the
    grass's reach."""
    near_bare = _spread(~mask, reach)
    inside = mask & ~near_bare
    edge = mask & near_bare
    outside = ~_spread(mask, reach)
    return inside, edge, outside


def zone_mean(flicker, zone):
    """The mean of the map over a zone, None for a zone without pixels."""
    return float(flicker[zone].mean()) if zone.any() else None


MIN_GRASS_SHARE = 0.02   # of the picture
EDGE_HARDER = 1.5        # the edge against the inside
STAYS_WITHOUT_WIND = 0.5  # of the flicker with the wind
ABOVE_THE_FLOOR = 1.0    # grey a frame the grass may flicker more than what is beside it
ADDS_NOTHING = 0.2       # grey a frame the grass may add to a turn


def verdict(grass_share, wind_inside, wind_edge, calm_inside, calm_edge, outside):
    """(the run measured something, what it says)."""
    if grass_share is None or grass_share < MIN_GRASS_SHARE:
        seen = "no grass mask" if grass_share is None else f"{grass_share * 100:.1f} % of the picture"
        return (False, f"no measurement: too little grass in view ({seen}, "
                       f"{MIN_GRASS_SHARE * 100:.0f} % wanted)")
    if None in (wind_inside, wind_edge, calm_inside, calm_edge, outside):
        return (False, "no measurement: a zone of the picture holds no pixel")
    if wind_inside == 0.0 and wind_edge == 0.0:
        return (False, "no measurement: the grass does not change at all in the wind - a frozen picture "
                       "(a menu up, the world paused)")
    if max(wind_inside, wind_edge) - outside <= ABOVE_THE_FLOOR:
        return (True, f"with the head still the grass is as steady as what is beside it "
                      f"({wind_inside:.2f} inside, {wind_edge:.2f} at its rim, {outside:.2f} beside it): "
                      f"no flicker of its own, neither from the wind nor from what it stands before")
    if wind_edge > EDGE_HARDER * wind_inside:
        where = (f"the flicker sits at the grass's edge against what is no grass "
                 f"({wind_edge:.2f} against {wind_inside:.2f} inside)")
    else:
        where = (f"the flicker is all over the grass, grass before grass as much as at its rim "
                 f"({wind_inside:.2f} inside, {wind_edge:.2f} at the rim)")
    wind, calm = max(wind_inside, wind_edge), max(calm_inside, calm_edge)
    if calm >= STAYS_WITHOUT_WIND * wind:
        why = f"it stays without the wind ({calm:.2f} of {wind:.2f}): the picture itself is unsteady"
    else:
        why = f"it goes with the wind ({calm:.2f} left of {wind:.2f}): blades moving across what is behind them"
    return (True, f"{where}; {why}")


def turn_reading(turn, calmturn, bareturn):
    """(the turns measured something, what they say): the picture's flicker
    under the same slow turn with the wind, without it and without grass."""
    for name, value in (("turn", turn), ("calmturn", calmturn), ("bareturn", bareturn)):
        if value is None:
            return (False, f"no measurement: the series {name} is missing")
    with_wind, without = turn - bareturn, calmturn - bareturn
    if max(with_wind, without) <= ADDS_NOTHING:
        return (True, f"under a turning head the grass adds nothing to the picture's flicker "
                      f"({turn:.2f} with it in the wind, {bareturn:.2f} without grass)")
    return (True, f"under a turning head the grass adds {with_wind:.2f} to the picture's flicker in the wind "
                  f"and {without:.2f} without it (the turn without grass {bareturn:.2f})")


def read_overlap(run, eye, mask_threshold, reach, full):
    """Prints one grass-overlap run; True when it measured something."""
    from PIL import Image
    name = "/".join(os.path.normpath(run).split(os.sep)[-2:])
    series = {mark: load_frames(run, eye, mark) for mark in ("wind", "calm", "bare")}
    maps = {mark: flicker_map(frames) for mark, frames in series.items()}
    mask = grass_mask(series["calm"], series["bare"], mask_threshold)
    if mask is None or maps["wind"] is None or maps["calm"] is None or maps["bare"] is None:
        print(f"{name}: no measurement: the series wind, calm and bare are not all there "
              f"({', '.join(f'{m} {0 if f is None else len(f)}' for m, f in series.items())} frames)")
        return False
    inside, edge, outside = zones(mask, reach)
    share = float(mask.mean())
    cells = {(mark, zone_name): zone_mean(maps[mark], zone)
             for mark in ("wind", "calm", "bare")
             for zone_name, zone in (("inside", inside), ("edge", edge), ("outside", outside))}
    show = lambda v: f"{'-':>9}" if v is None else f"{v:9.2f}"
    print(f"{name}: grass {share * 100:.1f} % of the picture "
          f"(inside {inside.mean() * 100:.1f} %, edge {edge.mean() * 100:.1f} %)")
    print(f"  {'series':<8}{'inside':>9}{'edge':>9}{'outside':>9}")
    for mark in ("wind", "calm", "bare"):
        print(f"  {mark:<8}" + "".join(show(cells[(mark, z)]) for z in ("inside", "edge", "outside")))
    ok, text = verdict(share, cells[("wind", "inside")], cells[("wind", "edge")],
                       cells[("calm", "inside")], cells[("calm", "edge")], cells[("wind", "outside")])
    print(f"  {'OK' if ok else 'FAIL'}: {text}")
    turns = {}
    for mark in ("turn", "calmturn", "bareturn"):
        turned = flicker_map(load_frames(run, eye, mark))
        turns[mark] = None if turned is None else float(turned.mean())
        if turned is not None:
            pixels, _ = concentration(turned, 10.0)
            print(f"  {mark:<8} the whole picture {turns[mark]:6.2f}, {pixels * 100:.1f} % of its pixels above 10")
    turn_ok, turn_text = turn_reading(**turns)
    print(f"  {'OK' if turn_ok else 'FAIL'}: {turn_text}")
    ok = ok and turn_ok
    for mark in ("wind", "calm"):
        Image.fromarray(heat(maps[mark], full)).save(os.path.join(run, f"flicker-{eye}-{mark}.png"))
    picture = np.zeros(mask.shape + (3,), dtype=np.uint8)
    picture[inside] = (0, 160, 0)
    picture[edge] = (255, 200, 0)
    Image.fromarray(picture).save(os.path.join(run, f"zones-{eye}.png"))
    return ok


def load_frames(run, eye, mark):
    from PIL import Image
    out = []
    for i in range(1000):
        path = os.path.join(run, f"OBVR-Eye-{eye}-{mark}-f{i:02d}.png")
        if not os.path.exists(path):
            break
        out.append(np.asarray(Image.open(path).convert("L"), dtype=np.float32))
    return np.stack(out) if out else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("runs", nargs="+")
    parser.add_argument("--marks", default="still,turn")
    parser.add_argument("--eye", default="Right")
    parser.add_argument("--boxes", default="")
    parser.add_argument("--threshold", type=float, default=10.0)
    parser.add_argument("--full", type=float, default=40.0)
    parser.add_argument("--overlap", action="store_true")
    parser.add_argument("--mask-threshold", type=float, default=8.0)
    parser.add_argument("--reach", type=int, default=3)
    args = parser.parse_args()
    if args.overlap:
        results = [read_overlap(run, args.eye, args.mask_threshold, args.reach, args.full)
                   for run in args.runs]
        sys.exit(0 if all(results) else 1)
    try:
        boxes = parse_boxes(args.boxes)
    except ValueError as error:
        sys.exit(str(error))
    from PIL import Image
    print("run/mark".ljust(40) + f"{'frames':>7}{'mean':>8}{'hot px':>9}{'carry':>8}"
          + "".join(f"{b[0]:>10}" for b in boxes))
    for run in args.runs:
        name = "/".join(os.path.normpath(run).split(os.sep)[-2:])
        for mark in args.marks.split(","):
            frames = load_frames(run, args.eye, mark)
            flicker = flicker_map(frames)
            if flicker is None:
                print(f"{name}/{mark}: fewer than three frames", file=sys.stderr)
                continue
            pixels, carried = concentration(flicker, args.threshold)
            cells = ""
            for box in boxes:
                mean = box_mean(flicker, box)
                cells += f"{'-':>10}" if mean is None else f"{mean:10.2f}"
            print(f"{name}/{mark}".ljust(40) + f"{len(frames):7d}{flicker.mean():8.2f}"
                  f"{pixels * 100:8.1f}%{carried * 100:7.1f}%" + cells)
            Image.fromarray(heat(flicker, args.full)).save(
                os.path.join(run, f"flicker-{args.eye}-{mark}.png"))


if __name__ == "__main__":
    main()
