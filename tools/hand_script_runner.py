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
from datetime import datetime
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


def run_token(stamp: str, pid: int) -> str:
    """The run's token: its stamp and the runner's pid. The runner's game gets it
    in its environment as OBVR_HARNESS and the overlay names it as HarnessToken,
    and OBVR reads the overlay only when the two agree (core/HarnessToken.h) -
    a game started through Steam carries none, so a file left behind, or a run
    beside the player's game, never reaches it."""
    return f"{stamp}-{pid}"


def token_problem(direct: bool) -> Optional[str]:
    """Why the run cannot start, or None: the token travels in the game's
    environment, which a start through Steam does not take."""
    if direct:
        return None
    return ("the test overlay needs the run's token in the game's environment (OBVR_HARNESS), "
            "which only a --direct or --headless start carries; a start through Steam cannot")


def build_test_ini(scenario: Scenario, save: str, token: str) -> str:
    """OBVR-test.ini for a run: [Debug] with the script, the console lines run from
    inside, the save to load and the run's token, then the scenario's own lines."""
    sections: Dict[str, List[str]] = {
        "Debug": [f"HandScript={SCRIPT_TARGET}", "HandScriptConsole=1"]}
    if save and save != "continue":
        sections["Debug"].append(f"HandScriptLoad={save}")
    sections["Debug"].append(f"HarnessToken={token}")
    # The start guide (OBVR.ini [Onboarding] StartGuide, a chain of pages in the
    # main menu) would stand in front of the main menu's console that loads the
    # run's save: off for every run unless the scenario says otherwise.
    if not any(line.startswith("StartGuide=") for line in scenario.ini.get("Onboarding", [])):
        sections["Onboarding"] = ["StartGuide=0"]
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


_COMPOSITOR_TOTAL = re.compile(
    r"^(\w{3} \w{3} \d{1,2} \d{4} \d{2}:\d{2}:\d{2}\.\d+) \[Info\] - Total\.+\s+(\d+) presents\.\s+"
    r"(\d+) dropped\.\s+(\d+) reprojected")


def compositor_time(stamp: str) -> datetime:
    """A vrcompositor.txt time stamp, e.g. "Wed Oct 14 2026 14:03:51.467338"."""
    return datetime.strptime(stamp, "%a %b %d %Y %H:%M:%S.%f")


def compositor_summary(lines: Sequence[str], since: datetime) -> Optional[Tuple[int, int, int]]:
    """SteamVR's frame summary for the last app that left the compositor since
    `since`: (presented, dropped, reprojected), or None. The compositor writes
    it when the app disconnects."""
    found = None
    for line in lines:
        m = _COMPOSITOR_TOTAL.match(line)
        if m and compositor_time(m.group(1)) >= since:
            found = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
    return found


def compositor_line(totals: Tuple[int, int, int]) -> str:
    presented, dropped, reprojected = totals
    share = 100.0 * reprojected / presented if presented else 0.0
    return (f"SteamVR: {presented} frames presented, {dropped} dropped, {reprojected} reprojected "
            f"({share:.1f} %)")


def env_problem(direct: bool, pairs: Sequence[str]) -> Optional[str]:
    """Why the runner's --env lines cannot be taken, or None: they only reach a
    --direct start (a start through Steam takes Steam's launch options), and
    each is KEY=VALUE with a key."""
    if pairs and not direct:
        return "--env only reaches a --direct start; a start through Steam takes Steam's launch options"
    for pair in pairs:
        key, sep, _ = pair.partition("=")
        if not sep or not key.strip():
            return f"--env {pair!r}: not KEY=VALUE"
    return None


def headless_command(game_command: Sequence[str], width: int, height: int) -> List[str]:
    """The game's launch wrapped in gamescope's headless backend (gamescope 3.16:
    "--backend headless => use headless backend (no window, no DRM output)"):
    nothing of the game on the desktop, no keyboard or mouse grabbed, its window
    inside gamescope's own Xwayland. The nested display is the game's own size
    (Oblivion.ini's iSize, so its fullscreen mode is found) and refreshes at 144,
    well over the headset's 90 - the VR frames are paced by SteamVR, not by the
    nested display."""
    return (["gamescope", "--backend", "headless", "-W", str(width), "-H", str(height),
             "-w", str(width), "-h", str(height), "-r", "144", "--"] + list(game_command))


def headless_problem(headless: bool, background_at: Sequence[str]) -> Optional[str]:
    """Why a scenario cannot run headless, or None: a background-at part puts a
    window of another process in front of the game, which needs the desktop the
    headless game has no window on."""
    if headless and background_at:
        return ("background-at needs the desktop (a window of another process put in front of the game); "
                "run this scenario without --headless")
    return None


def game_size(ini_lines: Iterable[str]) -> Tuple[int, int]:
    """Oblivion.ini's iSize W and H (the game's own fullscreen size, which the
    headless nested display is given); 2560 by 1440 when either is missing or
    not a number."""
    found = {}
    for line in ini_lines:
        key, sep, value = line.partition("=")
        key = key.strip().lower()
        if sep and key in ("isize w", "isize h"):
            try:
                found[key] = int(value.strip())
            except ValueError:
                pass
    if "isize w" in found and "isize h" in found:
        return found["isize w"], found["isize h"]
    return 2560, 1440


def newest_screenshot(files: Iterable[Tuple[str, float]], since: float) -> Optional[str]:
    """Of gamescope's screenshot files ((path, mtime), /tmp/gamescope_*.avif after a
    SIGUSR2), the newest written at or after `since`; None when none was."""
    newest: Optional[Tuple[str, float]] = None
    for path, written in files:
        if written >= since and (newest is None or written > newest[1]):
            newest = (path, written)
    return newest[0] if newest is not None else None


def harness_saves(names: Iterable[str]) -> List[str]:
    """Of the files in the saves folder, the harness's own saves and co-saves - every
    OBVRHarness*.ess and OBVRHarness*.obse (OBVRHarness, OBVRHarnessTall, OBVRHarnessMOO
    ...), in the order given. The runner keeps them the oldest saves there are, so the
    player's Continue never lands in one."""
    return [n for n in names if n.startswith("OBVRHarness") and n.endswith((".ess", ".obse"))]


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
