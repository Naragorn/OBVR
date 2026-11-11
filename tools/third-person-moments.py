#!/usr/bin/env python3
"""Reads a tp-*.txt harness run's OBVR.log mark by mark: the engine's own
third-person moment (the sit/mount bytes, the sit/sleep state 3 or 5, the
knock state over 0), the camera's mode, the eyes' distance from the body's
head and the animation vignette's alpha; then checks them against the
option's state (Nadi, 2026-11-11):

  on  ([Look] FirstPersonAnimations=0): in a moment the view is in third
      person, the eyes well off the head (over kOffUnits);
  off (FirstPersonAnimations=1): in a moment the eyes are in the head (under
      kInUnits) and the vignette is up;
  both: after the last moment the view is back in first person with the eyes
      in the head.

    python3 tools/third-person-moments.py on artifacts/hand-script/tp-chair-on/<time>
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

kInUnits = 20.0   # the eyes in the head: 5 to 8 units measured (view-offset-events, 2026-11-08)
kOffUnits = 40.0  # the chase camera: 160 to 220 units measured

MARK = re.compile(r"^HandScript: mark (\S+)")
STATES = re.compile(r"^Hands: at the mark - sit/mount bytes ([0-9A-F]{4}), sit/sleep state (\d+), knock state (-?\d+), third person (\d)")
HEAD = re.compile(r"^Camera: at the mark - the body's head ([\d.]+) units level from the eyes \(\S+ \S+\), (-?[\d.]+) up")
ALPHA = re.compile(r"^Camera: at the mark the animation vignette at alpha ([\d.]+)")


@dataclass
class Mark:
    name: str
    bytes_: int = 0
    sit: int = 0
    knock: int = 0
    third: int = 0
    level: float | None = None
    up: float | None = None
    alpha: float = 0.0
    problems: list[str] = field(default_factory=list)

    @property
    def moment(self) -> bool:
        return self.bytes_ != 0 or self.sit in (3, 5) or self.knock > 0

    @property
    def distance(self) -> float | None:
        if self.level is None or self.up is None:
            return None
        return (self.level**2 + self.up**2) ** 0.5


def read_marks(lines: list[str]) -> list[Mark]:
    marks: list[Mark] = []
    for line in lines:
        m = MARK.match(line)
        if m:
            marks.append(Mark(m.group(1)))
            continue
        if not marks:
            continue
        cur = marks[-1]
        if (m := STATES.match(line)) is not None:
            cur.bytes_, cur.sit, cur.knock, cur.third = int(m.group(1), 16), int(m.group(2)), int(m.group(3)), int(m.group(4))
        elif (m := HEAD.match(line)) is not None and cur.level is None:
            cur.level, cur.up = float(m.group(1)), float(m.group(2))
        elif (m := ALPHA.match(line)) is not None:
            cur.alpha = float(m.group(1))
    return marks


def check(marks: list[Mark], state: str) -> list[str]:
    """The problems, empty when the run reads as the option's state wants."""
    problems: list[str] = []
    moments = [k for k in marks if k.moment]
    if not moments:
        return ["no mark fell inside an engine moment"]
    for k in moments:
        d = k.distance
        if d is None:
            problems.append(f"{k.name}: the eyes against the head not read")
            continue
        if state == "on" and not (k.third == 1 and d > kOffUnits):
            problems.append(f"{k.name}: third person {k.third}, the eyes {d:.1f} from the head (want 1, over {kOffUnits:.0f})")
        if state == "off" and not d < kInUnits:
            problems.append(f"{k.name}: the eyes {d:.1f} from the head (want under {kInUnits:.0f})")
        if state == "off" and not k.alpha > 0.0:
            problems.append(f"{k.name}: no vignette")
    last = max(i for i, k in enumerate(marks) if k.moment)
    after = [k for k in marks[last + 1 :] if k.distance is not None and not k.moment]
    if after:
        k = after[-1]
        if k.third != 0 or k.distance >= kInUnits:  # type: ignore[operator]
            problems.append(f"{k.name} (after): third person {k.third}, the eyes {k.distance:.1f} from the head")
    else:
        problems.append("no mark after the last moment")
    return problems


def main(argv: list[str]) -> int:
    if len(argv) != 3 or argv[1] not in ("on", "off"):
        print(__doc__)
        return 2
    log = Path(argv[2]) / "OBVR.log"
    marks = read_marks(log.read_text(errors="replace").splitlines())
    for k in marks:
        d = k.distance
        print(f"{k.name:16} moment {int(k.moment)} third {k.third} eyes {'-' if d is None else f'{d:6.1f}'} vignette {k.alpha:.2f}")
    problems = check(marks, argv[1])
    print("PASS" if not problems else "FAIL\n- " + "\n- ".join(problems))
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
