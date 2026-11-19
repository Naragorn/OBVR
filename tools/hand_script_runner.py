"""The pure part of the Linux hand script runner (tools/hand-script-run.py).

What a scenario asks of the runner (its `expect`, `reject`, `count`, `ini`,
`console`, `console-at`, `background-at`, `moved`, `still`, `turned` and `gazed` lines,
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
_WALK = re.compile(r"^(moved|still|turned|gazed)\s+(\S+)\s+(\S+)\s+([0-9]+(?:\.[0-9]+)?)$")
_MARK = re.compile(r"HandScript: mark (\S+)")
# The eyes' heading at a mark (camera/MarkEyes.cpp), the first after the mark.
_EYES = re.compile(r"Camera: at the mark - the eyes at .* heading (-?[0-9.]+)\)")
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
    # ini-at <mark> <Section> <Key=Value>: written into OBVR-test.ini when the
    # mark is reached; OBVR's hot reload takes it up in the running game.
    ini_at: Dict[str, List[Tuple[str, str]]] = field(default_factory=dict)
    background_at: List[str] = field(default_factory=list)
    walk_checks: List[Tuple[str, str, str, float]] = field(default_factory=list)
    # "ends": the game ends by itself during the script (SteamVR's quit,
    # vr/VrEvents.h); its ending is the pass.
    ends: bool = False


def parse_scenario(text: str) -> Scenario:
    """The scenario's lines for the runner; the script's own lines are OBVR's."""
    s = Scenario()
    for raw in text.splitlines():
        line = _COMMENT.sub("", raw).strip()
        if not line:
            continue
        word, _, rest = line.partition(" ")
        rest = rest.strip()
        if line == "ends":
            s.ends = True
        elif word == "expect" and rest:
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
        elif word == "ini-at":
            parts = rest.split()
            if len(parts) == 3 and "=" in parts[2] and not parts[2].startswith("="):
                s.ini_at.setdefault(parts[0], []).append((parts[1], parts[2]))
    return s


def apply_ini_change(text: str, section: str, setting: str) -> str:
    """The INI text with Key=Value set in [section]: the key's line there
    replaced, or the line added at the section's end, or the section added
    with it. Section and key are found without case, as the INI is read."""
    key = setting.partition("=")[0].lower()
    lines = text.splitlines()
    inside = False
    end = None  # the index after the section's last line
    for at, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            if inside:
                break
            inside = stripped[1:-1].lower() == section.lower()
            if inside:
                end = at + 1
            continue
        if inside:
            end = at + 1
            if stripped.partition("=")[0].strip().lower() == key and "=" in stripped:
                lines[at] = setting
                return "\n".join(lines) + "\n"
    if end is None:
        lines += [f"[{section}]", setting]
    else:
        lines.insert(end, setting)
    return "\n".join(lines) + "\n"


def ending_problem(ends: bool, game_ended: bool) -> Optional[str]:
    """Whether the game's ending (or its not ending) is a problem: an "ends"
    scenario wants it to end by itself, any other wants it running until the
    runner closes it."""
    if ends and not game_ended:
        return "the game did not end by itself"
    if game_ended and not ends:
        return "the game ended during the script"
    return None


# A run that measures has the machine to itself (Nadi, 2026-11-17): beside it
# no other harness run and no other game. Every run holds the runs' lock
# shared, an exclusive one alone; an exclusive run that waits closes a gate
# first, so that new runs queue behind it instead of starving it.
def measures(scenario: Scenario) -> bool:
    """Whether the scenario turns OBVR's profiler on - at the start or at a mark."""
    lines = list(scenario.ini.get("Performance", []))
    for changes in scenario.ini_at.values():
        lines += [setting for section, setting in changes if section == "Performance"]
    return any(line.replace(" ", "") == "Enabled=1" for line in lines)


def exclusive_reason(asked: bool, scenario: Scenario) -> Optional[str]:
    """Why the run is exclusive, or None when it is not."""
    if asked:
        return "asked for with --exclusive"
    if measures(scenario):
        return "the scenario turns the profiler on ([Performance] Enabled=1)"
    return None


def waiting_for(exclusive: bool, gate_free: bool, lock_free: bool, other_games: int) -> Optional[str]:
    """What keeps the run from starting, or None when nothing does. A run that is
    not exclusive does not mind other games; an exclusive one waits for all of them."""
    if not gate_free:
        return ("another exclusive run is under way or waiting" if exclusive
                else "an exclusive run is under way or waiting")
    if not lock_free:
        return "other harness runs are under way" if exclusive else "an exclusive run is under way"
    if exclusive and other_games > 0:
        return (f"{other_games} game{' is' if other_games == 1 else 's are'} running "
                "(the player's, or a run started some other way)")
    return None


def wait_ran_out(exclusive: bool, reason: str, seconds: float) -> str:
    if exclusive:
        return f"this exclusive run did not get the machine to itself within {seconds:g} s: {reason}"
    return f"this run could not start within {seconds:g} s: {reason}"


def exclusive_note(reason: str, waited: float, other_sessions: int) -> str:
    """The summary's line about an exclusive run: why, how long it waited, and what
    the runner cannot keep away - other gamescope sessions (a BasHarness run)."""
    note = (f"exclusive ({reason}): it started with no other harness run and no other game running, and held "
            "the harness's other runs off until its end")
    if waited >= 1.0:
        note += f"; it waited {waited:.0f} s for that"
    if other_sessions > 0:
        one = other_sessions == 1
        note += (f". {other_sessions} other gamescope session{' was' if one else 's were'} running at its start "
                 f"(not this harness's - {'its' if one else 'their'} load is in the numbers)")
    return note


def intruder_problem(other_games: int, last_mark: Optional[str]) -> Optional[str]:
    """An exclusive run that found other games beside it while it ran - the
    player's own start, or a runner from before the locks, which nothing holds
    off: its measurement is not what it claims, and that is a problem."""
    if other_games <= 0:
        return None
    when = f"after the mark {last_mark}" if last_mark else "before its first mark"
    return (f"this exclusive run was not alone: {other_games} other game{' was' if other_games == 1 else 's were'} "
            f"running beside it {when} (not started through this runner's locks) - its numbers carry that load")


def run_folder(stamp: str, slot: Optional[int]) -> str:
    """The run's folder under its scenario's: its start, and for a parallel run its
    slot as well - two of one scenario started in the same second are two folders."""
    return stamp if slot is None else f"{stamp}-slot{slot}"


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


# A run without SteamVR (--no-steamvr): tools/mock-openvr's stand-in runtime is
# put beside OBVR.dll for the run and named in the overlay's [Debug]
# OpenVRLibrary (vr/OpenVRLibrary.h). It answers as the sleeping headset every
# harness run had, so the script's head and hands are what they always were.
STAND_IN_LIBRARY = "openvr_mock.dll"
_STAND_IN_LOADED = f"OpenVRLibrary={STAND_IN_LIBRARY} loaded"
STAND_IN_NOTE = (f"no SteamVR: OBVR ran against the stand-in runtime ({STAND_IN_LIBRARY}) - a sleeping headset at "
                 "its rate, no controllers (tools/mock-openvr/MockRuntime.h); nothing was composited, so no compositor "
                 "summary, and what SteamVR itself does (overlays shown, the dome, its keyboard, frame timing) was "
                 "not exercised")


# The headsets the stand-in knows, as tools/mock-openvr/MockRuntime.h has them
# (MakeHeadset): an eye's width and height, the display rate, the IPD in mm.
# Kept here as well so that a run can say what it asked for and check OBVR's
# log against it - two tables that disagree fail every run.
STAND_IN_HEADSETS: Dict[str, Tuple[int, int, float, float]] = {
    "dreamair": (4036, 3376, 72.0, 63.0),
    "beyond": (2568, 2568, 75.0, 62.0),
}
STAND_IN_DEFAULT_HEADSET = "dreamair"
_EYE_SIZE = re.compile(r"^([0-9]+)[xX]([0-9]+)$")


def _eye_size(eye: str) -> Optional[Tuple[int, int]]:
    match = _EYE_SIZE.match(eye)
    if match is None:
        return None
    width, height = int(match.group(1)), int(match.group(2))
    if not (256 <= width <= 16384 and 256 <= height <= 16384):
        return None
    return width, height


def headset_problem(no_steamvr: bool, headset: Optional[str], hz: Optional[float], eye: Optional[str],
                    ipd: Optional[float]) -> Optional[str]:
    """Why the stand-in's headset cannot be as asked, or None. The same limits as
    the stand-in's own (ConfiguredHeadset), which leaves out what it cannot read."""
    if not no_steamvr:
        if headset is not None or hz is not None or eye is not None or ipd is not None:
            return ("--headset, --hz, --eye and --ipd describe the stand-in runtime's headset; they need "
                    "--no-steamvr (with SteamVR the headset is the real one)")
        return None
    if headset is not None and headset not in STAND_IN_HEADSETS:
        return f"--headset {headset}: the stand-in knows {', '.join(sorted(STAND_IN_HEADSETS))}"
    if hz is not None and not 30.0 <= hz <= 240.0:
        return f"--hz {hz:g}: a display rate from 30 to 240"
    if eye is not None and _eye_size(eye) is None:
        return f"--eye {eye}: an eye's size as <width>x<height>, each side from 256 to 16384"
    if ipd is not None and not 50.0 <= ipd <= 80.0:
        return f"--ipd {ipd:g}: the eyes' distance in millimetres, from 50 to 80"
    return None


def headset_env(headset: Optional[str], hz: Optional[float], eye: Optional[str],
                ipd: Optional[float]) -> Dict[str, str]:
    """The game's environment for the stand-in's headset (read at VR_InitInternal)."""
    env = {"OBVR_MOCK_HEADSET": headset or STAND_IN_DEFAULT_HEADSET}
    if hz is not None:
        env["OBVR_MOCK_HZ"] = f"{hz:g}"
    if eye is not None:
        env["OBVR_MOCK_EYE"] = eye.lower()
    if ipd is not None:
        env["OBVR_MOCK_IPD"] = f"{ipd:g}"
    return env


def expected_headset(headset: Optional[str], hz: Optional[float], eye: Optional[str],
                     ipd: Optional[float]) -> Tuple[int, int, float, float]:
    """What OBVR is to read from the stand-in: an eye's width and height, the rate, the IPD in mm."""
    width, height, own_hz, own_ipd = STAND_IN_HEADSETS[headset or STAND_IN_DEFAULT_HEADSET]
    if eye is not None:
        size = _eye_size(eye)
        if size is not None:
            width, height = size
    return width, height, own_hz if hz is None else hz, own_ipd if ipd is None else ipd


def headset_log_problem(expected: Tuple[int, int, float, float], lines: Sequence[str]) -> Optional[str]:
    """Whether OBVR read the headset the run asked for: its three lines about the
    eye textures, the display rate and the IPD."""
    width, height, hz, ipd = expected
    wanted = [f"two {width}x{height} eye textures ready", f"display runs at {hz:.2f} Hz", f"IPD {ipd:.1f} mm"]
    missing = [w for w in wanted if not any(w in line for line in lines)]
    if not missing:
        return None
    return ("the stand-in's headset is not the one asked for - missing in the log: " +
            ", ".join(f'"{m}"' for m in missing))


def headset_note(headset: Optional[str], expected: Tuple[int, int, float, float]) -> str:
    width, height, hz, ipd = expected
    return (f"stand-in headset: {headset or STAND_IN_DEFAULT_HEADSET} - {width}x{height} an eye, {hz:g} Hz, "
            f"IPD {ipd:g} mm")


def steamvr_problem(no_steamvr: bool, steamvr_runs: bool) -> Optional[str]:
    """Why the run cannot start, or None: a run with SteamVR needs it running; one
    without it does not care (a SteamVR running for someone else is left alone)."""
    if no_steamvr or steamvr_runs:
        return None
    return ("SteamVR is not running; start it first (the headset may sleep), or run with --no-steamvr "
            "(the stand-in runtime, docs/hand-script-harness.md)")


def stand_in_problem(no_steamvr: bool, built: bool, path: str) -> Optional[str]:
    """Why a run without SteamVR cannot start, or None: the stand-in must be built."""
    if not no_steamvr or built:
        return None
    return f"--no-steamvr needs the stand-in runtime, and {path} is not there; tools/build-linux.sh builds it"


def stand_in_loaded_problem(no_steamvr: bool, lines: Sequence[str]) -> Optional[str]:
    """Whether the log agrees with the run about the runtime: without SteamVR OBVR
    must say it loaded the stand-in (an older OBVR.dll does not know the key and
    would go to SteamVR), and with SteamVR it must not have."""
    loaded = any(_STAND_IN_LOADED in line for line in lines)
    if no_steamvr and not loaded:
        return (f"--no-steamvr, but OBVR did not load the stand-in runtime (no \"{_STAND_IN_LOADED}\" line): "
                "an OBVR.dll from before [Debug] OpenVRLibrary?")
    if loaded and not no_steamvr:
        return "OBVR loaded the stand-in runtime in a run that was to use SteamVR"
    return None


# The holster places as OBVR.ini ships them ([Hands] Holster*; vr/Holster.h's
# HolsterSettings): metres right, forward, up from the eyes, and the radii.
SHIPPED_HOLSTER_PLACES: Tuple[Tuple[str, str], ...] = (
    ("HolsterOneHandX", "-0.18"), ("HolsterOneHandForward", "0.02"), ("HolsterOneHandUp", "-0.62"),
    ("HolsterOneHandRadius", "0.22"), ("HolsterTwoHandX", "0.18"), ("HolsterTwoHandForward", "-0.12"),
    ("HolsterTwoHandUp", "-0.15"), ("HolsterTwoHandRadius", "0.18"), ("HolsterBowX", "-0.18"),
    ("HolsterBowForward", "-0.12"), ("HolsterBowUp", "-0.15"), ("HolsterBowRadius", "0.18"))
_HOLSTER_PLACE_KEYS = {key for key, _ in SHIPPED_HOLSTER_PLACES}


def build_test_ini(scenario: Scenario, save: str, token: str, stand_in: bool = False) -> str:
    """OBVR-test.ini for a run: [Debug] with the script, the console lines run from
    inside, the save to load and the run's token, then the scenario's own lines.
    With stand_in, OBVR is pointed at the stand-in runtime (a run without SteamVR)."""
    sections: Dict[str, List[str]] = {
        "Debug": [f"HandScript={SCRIPT_TARGET}", "HandScriptConsole=1"]}
    if save and save != "continue":
        sections["Debug"].append(f"HandScriptLoad={save}")
    sections["Debug"].append(f"HarnessToken={token}")
    if stand_in:
        sections["Debug"].append(f"OpenVRLibrary={STAND_IN_LIBRARY}")
        # The stand-in's headset decides the eye size ([Render]
        # GameResolutionWidth/Height 0, the headset's own): the live INI's own
        # size, set for the player's headset, stood over it - a --headset
        # beyond run rendered at the Dream Air's 4036x3376 (2026-11-19). A
        # scenario that sets either sets the size itself.
        if not any(line.startswith("GameResolution") for line in scenario.ini.get("Render", [])):
            sections["Render"] = ["GameResolutionWidth=0", "GameResolutionHeight=0"]
    # The start guide (OBVR.ini [Onboarding] StartGuide, a chain of pages in the
    # main menu) would stand in front of the main menu's console that loads the
    # run's save: off for every run unless the scenario says otherwise.
    if not any(line.startswith("StartGuide=") for line in scenario.ini.get("Onboarding", [])):
        sections["Onboarding"] = ["StartGuide=0"]
    # The install self-check's panel (OBVR.ini [SelfCheck] ShowPanel) stands
    # there the same way when the test install lacks a file: off unless the
    # scenario asks for it; its log lines stay.
    if not any(line.startswith("ShowPanel=") for line in scenario.ini.get("SelfCheck", [])):
        sections["SelfCheck"] = ["ShowPanel=0"]
    # The holster places as shipped (OBVR.ini), not the player's own: the
    # scripts' hands reach for those. A scenario that sets any place sets them
    # all itself (the INI's first line for a key would win over a later one).
    hands = scenario.ini.get("Hands", [])
    for section, lines in scenario.ini.items():
        sections.setdefault(section, []).extend(lines)
    if not any(line.split("=", 1)[0] in _HOLSTER_PLACE_KEYS for line in hands):
        sections.setdefault("Hands", []).extend(f"{key}={value}" for key, value in SHIPPED_HOLSTER_PLACES)
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


def _eyes(lines: Sequence[str]) -> Dict[str, float]:
    """The eyes' heading at each mark: the first eyes line after the mark's own."""
    eyes: Dict[str, float] = {}
    current: Optional[str] = None
    for line in lines:
        m = _MARK.search(line)
        if m:
            current = m.group(1)
            continue
        e = _EYES.search(line)
        if current is not None and e:
            eyes.setdefault(current, float(e.group(1)))
            current = None
    return eyes


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
    eyes = _eyes(lines)
    for kind, a, b, limit in scenario.walk_checks:
        if kind == "gazed":
            if a not in eyes or b not in eyes:
                problems.append(f"gazed {a} {b}: no eyes line at mark {a if a not in eyes else b}")
                continue
            turn = abs(((eyes[b] - eyes[a]) % 360 + 540) % 360 - 180)
            measured.append(f"the eyes from {a} to {b}: {turn:.1f} degrees (at most {limit:.1f})")
            if turn > limit:
                problems.append(f"gazed {a} {b}: the eyes turned {turn:.1f} degrees, wanted at most {limit:.1f}")
            continue
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


# --- the recording mode ([Debug] Record, test/RecordingPlan.h) -----------------

# The Black Hand Robe (DBBlackHandRobe 000651D2, the robe of robe-hands.txt): the
# harness save's figure is naked, and a film meant for a person to look at
# shows it dressed (--clothed).
CLOTHED_FORM = "000651D2"
REC_NAME = re.compile(r"^OBVR-Rec-(.+)-(\d{4})\.bmp$")


def with_recording(text: str, seconds: float, every: int, width: int, clothed: bool) -> str:
    """The scenario with the recording switched on (--record) and the figure
    dressed (--clothed): ini and console lines put before its first line that
    is neither a comment nor empty, so a reader of the run's script.txt sees
    them first."""
    added: List[str] = []
    if seconds > 0:
        added += ["ini Debug Record=1", f"ini Debug RecordSeconds={seconds:g}",
                  f"ini Debug RecordEvery={max(1, every)}", f"ini Debug RecordWidth={width}"]
    if clothed:
        added += [f"console player.additem {CLOTHED_FORM} 1", f"console player.equipitem {CLOTHED_FORM}"]
    if not added:
        return text
    lines = text.splitlines()
    at = next((i for i, line in enumerate(lines) if line.strip() and not line.lstrip().startswith("#")),
              len(lines))
    return "\n".join(lines[:at] + added + lines[at:]) + "\n"


def recording_films(names: Iterable[str]) -> Dict[str, List[str]]:
    """The films among a folder's files: each mark's pictures in order. Names
    that are not a film's picture are left out."""
    films: Dict[str, List[Tuple[int, str]]] = {}
    for name in names:
        m = REC_NAME.match(name)
        if m:
            films.setdefault(m.group(1), []).append((int(m.group(2)), name))
    return {mark: [n for _, n in sorted(pics)] for mark, pics in films.items()}


def strip_picks(count: int, most: int = 10) -> List[int]:
    """Which pictures of a film go into its strip: `most` spread evenly from
    the first to the last, all of them when there are fewer."""
    if count <= 0:
        return []
    if count <= most:
        return list(range(count))
    return sorted({round(i * (count - 1) / (most - 1)) for i in range(most)})


def film_fps(hz: float, every: int) -> float:
    """The film's rate: the headset's frames a second over the frames between
    two pictures. The game may run slower than the headset; the film then runs
    a little fast."""
    return max(1.0, (hz if hz > 0 else 72.0) / max(1, every))


def film_commands(mark: str, folder: str, out_dir: str, fps: float, strip_names: Sequence[str]) -> List[List[str]]:
    """The commands that make a mark's film: an MP4 and a GIF from its
    pictures in order, and a strip of the picked pictures side by side."""
    pattern = f"{folder}/OBVR-Rec-{mark}-%04d.bmp"
    mp4 = [
        "ffmpeg", "-y", "-loglevel", "error", "-framerate", f"{fps:g}", "-i", pattern,
        "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", f"{out_dir}/rec-{mark}.mp4",
    ]
    gif = [
        "ffmpeg", "-y", "-loglevel", "error", "-framerate", f"{fps:g}", "-i", pattern, "-vf",
        "scale=480:-2:flags=lanczos,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer",
        f"{out_dir}/rec-{mark}.gif",
    ]
    strip = ["magick"] + [f"{folder}/{name}" for name in strip_names] + [
        "-resize", "320x", "+append", f"{out_dir}/rec-{mark}-strip.png"]
    return [mp4, gif, strip] if strip_names else [mp4, gif]
