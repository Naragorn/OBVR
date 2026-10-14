"""The pure part of the Linux hand script runner (tools/hand-script-run.py).

What a scenario asks of the runner (its `expect`, `reject`, `count`, `ini`,
`console`, `console-at`, `background-at`, `moved`, `still` and `turned` lines,
src/test/HandScript.h), the OBVR-test.ini written for a run, finding the game
among the processes, following the marks in OBVR.log, and the verdict - the
same checks as the Windows runner, tools/hand-script-run.ps1.

Unlike the Windows runner, this one types nothing into the game: the test INI
switches on [Debug] HandScriptConsole (OBVR hands the scenario's console lines
to the game itself) and names the save in [Debug] HandScriptLoad (OBVR loads it
from the main menu) - src/test/HandScriptConsole.h.

Covered by tests/HandScriptRunnerTest.py.
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass, field
from pathlib import PureWindowsPath
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

SCRIPT_TARGET = "OBVR-HandScript.txt"

_COMMENT = re.compile(r"#.*$")
_WALK = re.compile(r"^(moved|still|turned)\s+(\S+)\s+(\S+)\s+([0-9]+(?:\.[0-9]+)?)$")
_MARK = re.compile(r"HandScript: mark (\S+)")
_PLAYER = re.compile(
    r"HandScript: player at (-?[0-9.]+) (-?[0-9.]+) (-?[0-9.]+), heading (-?[0-9.]+) degrees")


@dataclass
class Scenario:
    expects: List[str] = field(default_factory=list)
    rejects: List[str] = field(default_factory=list)
    counts: List[Tuple[int, str]] = field(default_factory=list)
    ini: Dict[str, List[str]] = field(default_factory=dict)
    console: List[str] = field(default_factory=list)
    console_at: Dict[str, List[str]] = field(default_factory=dict)
    background_at: List[str] = field(default_factory=list)
    walk_checks: List[Tuple[str, str, str, float]] = field(default_factory=list)


def parse_scenario(text: str) -> Scenario:
    """The scenario's lines for the runner; the script's own lines are OBVR's."""
    s = Scenario()
    for raw in text.splitlines():
        line = _COMMENT.sub("", raw).strip()
        if not line:
            continue
        word, _, rest = line.partition(" ")
        rest = rest.strip()
        if word == "expect" and rest:
            s.expects.append(rest)
        elif word == "reject" and rest:
            s.rejects.append(rest)
        elif word == "background-at" and rest and " " not in rest:
            s.background_at.append(rest)
        elif _WALK.match(line):
            m = _WALK.match(line)
            s.walk_checks.append((m.group(1), m.group(2), m.group(3), float(m.group(4))))
        elif word == "console-at":
            mark, _, command = rest.partition(" ")
            if mark and command.strip():
                s.console_at.setdefault(mark, []).append(command.strip())
        elif word == "console" and rest:
            s.console.append(rest)
        elif word == "count":
            n, _, what = rest.partition(" ")
            if n.isdigit() and what.strip():
                s.counts.append((int(n), what.strip()))
        elif word == "ini":
            section, _, setting = rest.partition(" ")
            setting = setting.strip()
            if section and "=" in setting and " " not in setting:
                s.ini.setdefault(section, []).append(setting)
    return s


def build_test_ini(scenario: Scenario, save: str) -> str:
    """OBVR-test.ini for a run: [Debug] with the script, the console lines run from
    inside and the save to load, then the scenario's own lines."""
    sections: Dict[str, List[str]] = {
        "Debug": [f"HandScript={SCRIPT_TARGET}", "HandScriptConsole=1"]}
    if save and save != "continue":
        sections["Debug"].append(f"HandScriptLoad={save}")
    for section, lines in scenario.ini.items():
        sections.setdefault(section, []).extend(lines)
    out = ["; Written by tools/hand-script-run.py for one run and deleted after it."]
    for section, lines in sections.items():
        out.append(f"[{section}]")
        out.extend(lines)
    return "\n".join(out) + "\n"


def find_game_pid(processes: Iterable[Tuple[int, str, str]]) -> Optional[int]:
    """The game among (pid, argv[0], comm): the process whose program is
    Oblivion.exe. Under Proton its comm reads "Main" and its argv[0] the
    executable's name or Windows path; the launcher and the loader are other
    names."""
    for pid, argv0, _comm in processes:
        if PureWindowsPath(argv0.replace("/", "\\")).name.lower() == "oblivion.exe":
            return pid
    return None


def new_marks(lines: Sequence[str], seen: int) -> Tuple[List[str], bool, int]:
    """The marks written since `seen` mark-or-finished lines, whether the script
    finished among them, and the new count of such lines."""
    relevant = [l for l in lines if "HandScript: mark " in l or "HandScript: finished" in l]
    marks: List[str] = []
    finished = False
    for line in relevant[seen:]:
        m = _MARK.search(line)
        if m:
            marks.append(m.group(1))
        elif "HandScript: finished" in line:
            finished = True
    return marks, finished, len(relevant)


def _standing(lines: Sequence[str]) -> Dict[str, Tuple[float, float, float, float]]:
    """Where the player stood at each mark: the first player line after it."""
    standing: Dict[str, Tuple[float, float, float, float]] = {}
    current: Optional[str] = None
    for line in lines:
        m = _MARK.search(line)
        if m:
            current = m.group(1)
            continue
        p = _PLAYER.search(line)
        if current is not None and p:
            standing.setdefault(current, tuple(float(v) for v in p.groups()))  # type: ignore[arg-type]
            current = None
    return standing


def evaluate(scenario: Scenario, lines: Sequence[str]) -> Tuple[List[str], List[str]]:
    """The problems with a run's log, and what was measured on the way."""
    problems: List[str] = []
    measured: List[str] = []
    for e in scenario.expects:
        if not any(e in l for l in lines):
            problems.append(f"expected in the log, missing: {e}")
    for n, what in scenario.counts:
        found = sum(1 for l in lines if what in l)
        if found != n:
            problems.append(f"expected {n} times in the log, found {found}: {what}")
    for rej in scenario.rejects:
        hits = [l for l in lines if rej in l]
        if hits:
            problems.append(f"rejected, but in the log: {hits[0]}")
    standing = _standing(lines)
    for kind, a, b, limit in scenario.walk_checks:
        if a not in standing or b not in standing:
            missing = a if a not in standing else b
            problems.append(f"{kind} {a} {b}: no player line at mark {missing}")
            continue
        pa, pb = standing[a], standing[b]
        if kind == "turned":
            turn = abs(((pb[3] - pa[3]) % 360 + 540) % 360 - 180)
            measured.append(f"turned from {a} to {b}: {turn:.1f} degrees (at least {limit:.1f})")
            if turn < limit:
                problems.append(f"turned {a} {b}: {turn:.1f} degrees, wanted at least {limit:.1f}")
        else:
            d = math.hypot(pb[0] - pa[0], pb[1] - pa[1])
            bound = "at least" if kind == "moved" else "at most"
            measured.append(f"walked from {a} to {b}: {d:.1f} units across the ground ({bound} {limit:.1f})")
            if kind == "moved" and d < limit:
                problems.append(f"moved {a} {b}: {d:.1f} units, wanted at least {limit:.1f}")
            if kind == "still" and d > limit:
                problems.append(f"still {a} {b}: {d:.1f} units, wanted at most {limit:.1f}")
    return problems, measured


def summary(scenario: str, verdict: str, run_dir: str, marks: Sequence[str],
            measured: Sequence[str], problems: Sequence[str]) -> List[str]:
    """summary.md's lines, as the Windows runner writes them."""
    out = [f"# {scenario} - {verdict}", "", f"Run: {run_dir}", "", f"Marks: {', '.join(marks)}", ""]
    if measured:
        out.append("Measured:")
        out.extend(f"- {m}" for m in measured)
        out.append("")
    if problems:
        out.append("Problems:")
        out.extend(f"- {p}" for p in problems)
    else:
        out.append("Every expect line was found, no reject line was.")
    return out
