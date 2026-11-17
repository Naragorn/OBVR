"""The pure part of the harness's parallel runs (tools/hand-script-run.py --parallel,
docs/hand-script-harness.md "Several runs at once"); tests/HarnessMirrorTest.py.

A parallel run does not use the player's game: it takes a free slot under
~/.local/share/obvr-harness and runs from there, in
  - a mirror of the game folder: links to everything the game only reads, copies of
    the game folder's own files and of the whole plugin folder, and nothing of what
    a game writes (logs, captures) - so nothing a run writes lands in the player's
    game, and two runs never share a file they write;
  - a Proton prefix of its own, copied once from the player's and given the player's
    settings, load order and harness saves again at every run.
With the stand-in runtime (--no-steamvr) nothing is left that two runs share."""

from __future__ import annotations

import fnmatch
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

INSTANCES_FOLDER = "obvr-harness"
SLOTS = 4

_USER_INI = "Documents/My Games/Oblivion/Oblivion.ini"
_USER_PLUGINS = "AppData/Local/Oblivion/Plugins.txt"
_USER_SAVES = "Documents/My Games/Oblivion/Saves"

# What a game writes into its folder: never taken into a mirror.
_WRITTEN_FILES = ("*.log", "*.log.prev", "*.temp", "obvr-samples*.csv")
_WRITTEN_FOLDERS = ("obvr-performance-*", "obvr-backup")
# Folders plugins write their logs into: the mirror gets empty ones of its own.
_LOG_FOLDERS = ("*-logs", "crash logs")
# In the plugin folder: one run's own files, and what plugins wrote there.
_PLUGIN_LEFT_OUT = ("obvr-test.ini", "obvr-handscript.txt", "openvr_mock.dll", "*.log", "obvr-recording-*.txt")


def _matches(name: str, patterns: Sequence[str]) -> bool:
    return any(fnmatch.fnmatchcase(name.lower(), pattern) for pattern in patterns)


def mirror_plan(root: Iterable[Tuple[str, bool]], data: Iterable[str], obse: Iterable[str]) -> Dict[str, str]:
    """Relative path -> what the mirror holds there, from the names in the game
    folder ((name, is a folder)), in its Data and in Data/OBSE:
      'link'  a link to the player's file or folder (only read)
      'copy'  a copy of the file (the game folder's own: executables, INIs)
      'tree'  a copy of the whole folder (the plugins, which write beside themselves)
      'dir'   an empty folder of the mirror's own
      'skip'  nothing (what a game writes)"""
    plan: Dict[str, str] = {}
    data_name: Optional[str] = None
    for name, is_folder in root:
        if is_folder and name.lower() == "data":
            data_name = name
            plan[name] = "dir"
        elif is_folder:
            plan[name] = ("dir" if _matches(name, _LOG_FOLDERS) else
                          "skip" if _matches(name, _WRITTEN_FOLDERS) else "link")
        else:
            plan[name] = "skip" if _matches(name, _WRITTEN_FILES) else "copy"
    if data_name is None:
        return plan
    obse_name: Optional[str] = None
    for name in data:
        if name.lower() == "obse":
            obse_name = name
            plan[f"{data_name}/{name}"] = "dir"
        else:
            plan[f"{data_name}/{name}"] = "link"
    if obse_name is None:
        return plan
    for name in obse:
        plan[f"{data_name}/{obse_name}/{name}"] = "tree" if name.lower() == "plugins" else "link"
    return plan


def plugin_left_out(name: str) -> bool:
    """Whether a file of the player's plugin folder stays out of a mirror's copy."""
    return _matches(name, _PLUGIN_LEFT_OUT)


def _inside(path: str, folder: str) -> bool:
    return path == folder or path.startswith(folder + "/")


def own_game_pid(games: Iterable[Tuple[int, Optional[str]]], game_dir: str, instances_root: str,
                 parallel: bool) -> Optional[int]:
    """The run's own game among the running ones ((pid, working folder), the folder
    None when it cannot be read). A parallel run's is the one running from its
    mirror. A plain run's is any game that is not a slot's - the player's, wherever
    it was started from, as before there were slots."""
    own = game_dir.rstrip("/")
    root = instances_root.rstrip("/")
    for pid, cwd in games:
        folder = cwd.rstrip("/") if cwd else None
        if parallel:
            if folder == own:
                return pid
        elif folder is None or not _inside(folder, root):
            return pid
    return None


def prefix_refresh(saves: Iterable[str], save: str) -> List[str]:
    """The player's files a slot's prefix takes again at every run, relative to the
    prefix's user folder: the game's settings, the load order, the harness's saves and
    the save the run names with its co-save. Everything else in the prefix is as it
    was copied."""
    names = list(saves)
    wanted = [n for n in names if n.startswith("OBVRHarness") and n.endswith((".ess", ".obse"))]
    if save and save != "continue":
        wanted += [n for n in names if n in (f"{save}.ess", f"{save}.obse") and n not in wanted]
    return [_USER_INI, _USER_PLUGINS] + [f"{_USER_SAVES}/{n}" for n in wanted]


def slot_name(number: int) -> str:
    return f"slot-{number}"


def parallel_problem(parallel: bool, no_steamvr: bool, headless: bool, plugins: Sequence[str],
                     game_dir_given: bool) -> Optional[str]:
    """Why the run cannot start as asked, or None."""
    if not parallel:
        if plugins:
            return ("--plugin puts a file into a parallel run's own game folder; without --parallel it would "
                    "overwrite the player's (deploy it instead)")
        return None
    if not no_steamvr:
        return "--parallel needs --no-steamvr: SteamVR takes one game at a time and ends the older one"
    if not headless:
        return "--parallel needs --headless: two games' windows would fight for the desktop's focus"
    if game_dir_given:
        return "--parallel makes its own game folder; --game-dir cannot be given with it"
    return None
