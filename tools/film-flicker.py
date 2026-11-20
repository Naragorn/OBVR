#!/usr/bin/env python3
"""One-frame flashes in a run's films (tools/hand-script-run.py --record).

A flicker is a picture that shows something for one frame and then goes back:
a body part gone, turned inside out or in the wrong place for a frame. Per
pixel, a frame is taken as flashed where its grey lies more than --threshold
beyond both its neighbours (above the brighter or below the darker of the
frame before and the frame after). A steady move through the frame, or a
step that stays (the arms changing sides, the head turned), lies between its
neighbours or on one of them and does not count. Something moving fast and
thin (a blade swung across the view) does: it is at a pixel for one frame.

Per film (rec-<mark>.mp4, in the run's mark order) it prints the frames, the
largest share of pixels changed from one frame to the next (the film's motion:
a frozen film shows no flash either), the share of flashed pixels in the median frame and in the worst one (when, in
seconds from the mark), and how many frames had more than 0.1 % flashed - for
the whole picture and its lower half (the body when the head looks down).
With --pictures it writes the worst frame of each film beside its neighbours
and the flashed pixels in white (flash-<mark>.png in the run).

  tools/film-flicker.py <run folder> [--gate MARK ...] [--most PERCENT]
                        [--threshold GREY] [--width PIXELS] [--pictures]

A gate fails when a frame of that film had more than --most (0.5) percent of
its pixels flashed. Exits 1 on a failed gate or a gated film missing.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import numpy as np

MARK = re.compile(r"HandScript: mark (\S+)")


def flashed(before: np.ndarray, now: np.ndarray, after: np.ndarray, threshold: int) -> np.ndarray:
    """The pixels of `now` more than `threshold` beyond both neighbours."""
    b = before.astype(np.int16)
    n = now.astype(np.int16)
    a = after.astype(np.int16)
    return (n > np.maximum(b, a) + threshold) | (n < np.minimum(b, a) - threshold)


def film_shares(frames: list[np.ndarray], threshold: int) -> list[float]:
    """For each frame with two neighbours, the share of its pixels flashed."""
    return [float(flashed(frames[i - 1], frames[i], frames[i + 1], threshold).mean())
            for i in range(1, len(frames) - 1)]


def changed_shares(frames: list[np.ndarray], threshold: int) -> list[float]:
    """For each frame after the first, the share of its pixels more than
    `threshold` off the frame before: the film's motion (a frozen film shows
    no flash either)."""
    return [float((np.abs(frames[i].astype(np.int16) - frames[i - 1].astype(np.int16)) > threshold).mean())
            for i in range(1, len(frames))]


def part(picture: np.ndarray, which: str) -> np.ndarray:
    """The whole picture, or its upper or lower half."""
    half = picture.shape[0] // 2
    if which == "lower":
        return picture[half:]
    if which == "upper":
        return picture[:half]
    return picture


def verdict(shares: list[float], most_percent: float) -> tuple[bool, int]:
    """Whether no frame had more than `most_percent` flashed, and the worst
    frame's index among the shares (-1 when there is none)."""
    if not shares:
        return True, -1
    worst = int(np.argmax(shares))
    return shares[worst] * 100.0 <= most_percent, worst


def read_film(path: Path, width: int) -> tuple[list[np.ndarray], list[np.ndarray], float]:
    """The film's frames in grey at `width`, the same in colour, and its rate."""
    import cv2

    film = cv2.VideoCapture(str(path))
    fps = film.get(cv2.CAP_PROP_FPS) or 72.0
    grey, colour = [], []
    while True:
        ok, picture = film.read()
        if not ok:
            break
        height = max(1, round(picture.shape[0] * width / picture.shape[1]))
        small = cv2.resize(picture, (width, height), interpolation=cv2.INTER_AREA)
        colour.append(small)
        grey.append(cv2.cvtColor(small, cv2.COLOR_BGR2GRAY))
    film.release()
    return grey, colour, fps


def mark_order(run: Path) -> list[str]:
    log = run / "OBVR.log"
    if not log.exists():
        return []
    order = []
    for line in log.read_text(errors="replace").splitlines():
        m = MARK.search(line)
        if m and m.group(1) not in order:
            order.append(m.group(1))
    return order


def write_worst(run: Path, mark: str, colour: list[np.ndarray], grey: list[np.ndarray], index: int,
                threshold: int) -> None:
    import cv2

    i = index + 1  # the shares start at the second frame
    mask = flashed(grey[i - 1], grey[i], grey[i + 1], threshold).astype(np.uint8) * 255
    row = np.hstack([colour[i - 1], colour[i], colour[i + 1], cv2.cvtColor(mask, cv2.COLOR_GRAY2BGR)])
    cv2.imwrite(str(run / f"flash-{mark}.png"), row)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("run")
    ap.add_argument("--gate", action="append", default=[])
    ap.add_argument("--most", type=float, default=0.5)
    ap.add_argument("--threshold", type=int, default=24)
    ap.add_argument("--width", type=int, default=320)
    ap.add_argument("--pictures", action="store_true")
    a = ap.parse_args()
    run = Path(a.run)
    films = {p.stem[len("rec-"):]: p for p in run.glob("rec-*.mp4")}
    order = [m for m in mark_order(run) if m in films] + sorted(m for m in films if m not in mark_order(run))
    failed = False
    print(f"{'film':14} {'frames':>6} {'moved %':>8} {'median %':>9} {'worst %':>8} {'at s':>6} {'>0.1%':>6}"
          f" {'lower worst %':>14} {'at s':>6} {'>0.1%':>6}")
    for mark in order:
        grey, colour, fps = read_film(films[mark], a.width)
        shares = film_shares(grey, a.threshold)
        lower = film_shares([part(g, "lower") for g in grey], a.threshold)
        ok, worst = verdict(shares, a.most)
        lower_ok, lower_worst = verdict(lower, a.most)
        moved = changed_shares(grey, a.threshold)
        line = (f"{mark:14} {len(grey):6} {max(moved) * 100 if moved else 0:8.2f} {np.median(shares) * 100 if shares else 0:9.3f}"
                f" {shares[worst] * 100 if shares else 0:8.3f} {(worst + 1) / fps if shares else 0:6.2f}"
                f" {sum(s > 0.001 for s in shares):6}"
                f" {lower[lower_worst] * 100 if lower else 0:14.3f} {(lower_worst + 1) / fps if lower else 0:6.2f}"
                f" {sum(s > 0.001 for s in lower):6}")
        if mark in a.gate:
            passed = ok and lower_ok
            failed |= not passed
            line += f"  gate {'PASS' if passed else 'FAIL'}"
        print(line)
        if a.pictures and shares:
            write_worst(run, mark, colour, grey, worst, a.threshold)
    for g in a.gate:
        if g not in films:
            print(f"gate {g}: no film rec-{g}.mp4 - FAIL")
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
