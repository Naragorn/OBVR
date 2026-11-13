#!/usr/bin/env python3
"""OBVR doctor: checks that OBVR and what it needs are installed right.

Run it after installing, before the first start or any time later:

    python3 obvr-doctor.py                     finds the game by itself
    python3 obvr-doctor.py /path/to/Oblivion   or tell it where the game is
    python3 obvr-doctor.py --mo2 /path/to/MO2  Mod Organizer 2: its folder or ModOrganizer.ini

It only reads files and never changes anything. Every check prints one line,
OK or PROBLEM, and under a problem one line that says how to fix it. INFO
lines are things it could not check, or notes. The Windows version is
OBVR-doctor.bat (OBVR-doctor.ps1); both follow docs/self-check-spec.md.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import struct
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

# ------------------------------------------------------------------ facts
# Each value below has its evidence in docs/self-check-spec.md.

# Oblivion's Steam app ID (steamapps/libraryfolders.vdf, appmanifest_22330.acf).
OBLIVION_APP_ID = "22330"
# SteamVR's Steam app ID.
STEAMVR_APP_ID = "250820"
# MenuQue v16b's Submodule.Game.dll, SHA-256 of the file OBVR's in-game check
# verified as v16b (OBVR.log, 2026-11-10, both test installs).
MENUQUE_V16B_SHA256 = "b1f904137a1cdf888f1e4a2f24bd293cb17dd9effb1502021256c25fd5cfbc91"
# The README: xOBSE 22.13 or newer.
MIN_XOBSE = (22, 13)

# The files OBVR reads, as core/SelfCheckLogic.h's kExpectedFiles (the test
# tests/SelfCheckAssetsTest.py keeps the three lists the same). Relative to
# Data; the plugin ones under OBSE/Plugins.
PLUGIN_FILES = [
    "OBVR.ini",
    "OBVR_Controls.ini",
    "OBVR_Input/actions.json",
    "OBVR_Input/knuckles.json",
    "OBVR_Input/oculus_touch.json",
    "OBVR_Sounds/slap.wav",
    "OBVR_Stage/loading_dome_dark.obj",
    "OBVR_Stage/loading_dome_dark.mtl",
    "OBVR_Stage/loading_dome_dark.png",
]
DATA_FILES = [
    "Menus/generic/OBVR_AdjustHands.xml",
    "Menus/generic/OBVR_Controls.xml",
    "Menus/generic/OBVR_Onboarding.xml",
    "Menus/generic/OBVR_SelfCheck.xml",
    "Menus/generic/OBVR_Settings.xml",
    "Menus/generic/OBVR_Update.xml",
    "Menus/prefabs/OBVR/button_highlight.xml",
    "Textures/Effects/OBVR/CutCloth.dds",
    "Textures/Effects/OBVR/CutLeather.dds",
    "Textures/Effects/OBVR/CutSkin.dds",
    "Textures/Menus/OBVR/controls_1_left.dds",
    "Textures/Menus/OBVR/controls_1_left_index.dds",
    "Textures/Menus/OBVR/controls_1_right.dds",
    "Textures/Menus/OBVR/controls_1_right_index.dds",
    "Textures/Menus/OBVR/controls_2_left.dds",
    "Textures/Menus/OBVR/controls_2_right.dds",
    "Textures/Menus/OBVR/controls_3_left.dds",
    "Textures/Menus/OBVR/controls_3_left_index.dds",
    "Textures/Menus/OBVR/controls_3_right.dds",
    "Textures/Menus/OBVR/controls_3_right_index.dds",
    "Textures/Menus/OBVR/controls_4_left.dds",
    "Textures/Menus/OBVR/controls_4_right.dds",
    "Textures/Menus/OBVR/controls_5_left.dds",
    "Textures/Menus/OBVR/controls_5_left_index.dds",
    "Textures/Menus/OBVR/controls_5_right.dds",
    "Textures/Menus/OBVR/controls_5_right_index.dds",
    "Textures/Menus/OBVR/tpose.dds",
    "Meshes/characters/_male/specialanims/HandToHandStagger_OBVRMirror.kf",
    "Meshes/characters/_male/specialanims/OneHandStagger_OBVRMirror.kf",
    "Meshes/characters/_male/specialanims/StaffStagger_OBVRMirror.kf",
    "Meshes/characters/_male/specialanims/TwoHandStagger_OBVRMirror.kf",
]

OK, PROBLEM, INFO = "OK", "PROBLEM", "INFO"


@dataclass
class Result:
    state: str
    text: str
    fix: str = ""


# ------------------------------------------------------------------ pure readers


def pe_file_version(data: bytes):
    """The four numbers of a PE file's VS_FIXEDFILEINFO (its FileVersion), or None.
    Oblivion.exe of 1.2.0.416 reads 1.2.0.416, xOBSE 22.13's obse_1_2_416.dll 0.22.13.0."""
    at = data.find(struct.pack("<I", 0xFEEF04BD))
    if at < 0 or at + 16 > len(data):
        return None
    ms, ls = struct.unpack("<II", data[at + 8 : at + 16])
    return (ms >> 16, ms & 0xFFFF, ls >> 16, ls & 0xFFFF)


def pe_machine(data: bytes):
    """IMAGE_FILE_HEADER.Machine: 0x14C is 32-bit, 0x8664 64-bit; None for no PE file."""
    if len(data) < 0x40 or data[:2] != b"MZ":
        return None
    pe = struct.unpack("<I", data[0x3C:0x40])[0]
    if pe + 6 > len(data) or data[pe : pe + 4] != b"PE\0\0":
        return None
    return struct.unpack("<H", data[pe + 4 : pe + 6])[0]


def parse_dxvk_conf(text: str):
    """(memory defrag off, cached write-only buffers on), DXVK's way: "key = value",
    '#' a comment, booleans true/false in any case, the last line for a key wins."""
    defrag_off = cached_on = False
    for raw in text.splitlines():
        line = raw.split("#", 1)[0]
        if "=" not in line:
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        if key == "dxvk.enableMemoryDefrag":
            defrag_off = value.lower() == "false"
        elif key == "d3d9.cachedWriteOnlyBuffers":
            cached_on = value.lower() == "true"
    return defrag_off, cached_on


def trifle_shadow_on(text: str) -> bool:
    """Trifle.ini's EnableFirstPersonShadow=1 (the README: it hides the body)."""
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith(";"):
            continue
        m = re.match(r"EnableFirstPersonShadow\s*=\s*(\S*)", line)
        if m:
            return m.group(1).startswith("1")
    return False


def ini_flag(text: str, section: str, key: str):
    """A value as GetPrivateProfileIntA finds it: section and key in any case, the
    first line for the key, ';' lines skipped, a trailing comment cut. None for none."""
    current = None
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith(";"):
            continue
        if line.startswith("[") and "]" in line:
            current = line[1 : line.index("]")].strip().lower()
            continue
        if current == section.lower() and "=" in line:
            name, value = line.split("=", 1)
            if name.strip().lower() == key.lower():
                return value.split(";", 1)[0].strip()
    return None


def obng_stereo(text: str):
    """OBNG.ini's (StereoDraw, StereoEyes) under [Route]: 1 and 2 let OBVR draw
    both eyes in one pass (docs/single-pass-stereo-spec.md)."""
    return ini_flag(text, "Route", "StereoDraw"), ini_flag(text, "Route", "StereoEyes")


def parse_vdf(text: str) -> dict:
    """Steam's text VDF (libraryfolders.vdf, localconfig.vdf) into nested dicts,
    keys as they are written. Tolerates escapes and comments."""
    tokens = re.findall(r'"((?:[^"\\]|\\.)*)"|([{}])', text)
    root: dict = {}
    stack = [root]
    key = None
    for quoted, brace in tokens:
        if brace == "{":
            child: dict = {}
            stack[-1][key if key is not None else ""] = child
            stack.append(child)
            key = None
        elif brace == "}":
            if len(stack) > 1:
                stack.pop()
            key = None
        elif key is None:
            key = quoted
        else:
            stack[-1][key] = quoted.replace("\\\\", "\\")
            key = None
    return root


def ci_get(d: dict, name: str):
    """A dict entry by key in any case (Steam writes "Apps" and "apps")."""
    if not isinstance(d, dict):
        return None
    for k, v in d.items():
        if k.lower() == name.lower():
            return v
    return None


def overlay_state(localconfig: dict, app_id: str):
    """Steam's overlay for one game, from userdata/<id>/config/localconfig.vdf:
    off for all games at UserLocalConfigStore/system/EnableGameOverlay "0", off for
    this one at UserLocalConfigStore/apps/<id>/OverlayAppEnable "0", otherwise on
    (Special K's SKIF_Steam_isSteamOverlayEnabled reads it the same way). Returns
    "off-all", "off-game" or "on"."""
    store = ci_get(localconfig, "UserLocalConfigStore") or {}
    system = ci_get(store, "system") or {}
    if ci_get(system, "EnableGameOverlay") == "0":
        return "off-all"
    apps = ci_get(store, "apps") or {}
    app = ci_get(apps, app_id) or {}
    if ci_get(app, "OverlayAppEnable") == "0":
        return "off-game"
    return "on"


def shortcut_overlays(data: bytes):
    """Every non-Steam shortcut in userdata/<id>/config/shortcuts.vdf (binary
    KeyValues: a type byte - 0 map, 1 string, 2 int32, 3 float, 7 uint64,
    8 end of map - a NUL-terminated name, the value) as (name, exe, overlay
    on). AllowOverlay missing counts as on, Steam's default. None when the
    data does not parse."""
    shortcuts = []
    i, depth, cur = 0, 0, {}
    try:
        while i < len(data):
            t = data[i]
            i += 1
            if t == 8:
                if depth == 2:
                    shortcuts.append((cur.get("appname", ""), cur.get("exe", ""), cur.get("allowoverlay", 1) != 0))
                    cur = {}
                depth -= 1
                if depth < 0:
                    return None
                if depth == 0:
                    return shortcuts
                continue
            end = data.index(b"\x00", i)
            name = data[i:end].decode("utf-8", errors="replace").lower()
            i = end + 1
            if t == 0:
                depth += 1
            elif t == 1:
                end = data.index(b"\x00", i)
                if depth == 2:
                    cur[name] = data[i:end].decode("utf-8", errors="replace")
                i = end + 1
            elif t == 2:
                if depth == 2:
                    cur[name] = int.from_bytes(data[i:i + 4], "little")
                i += 4
            elif t in (3, 7):
                i += 4 if t == 3 else 8
            else:
                return None
            if i > len(data):
                return None
    except ValueError:
        return None
    return None


GAME_STARTERS = ("obse_loader.exe", "oblivion.exe")


def starts_the_game(exe: str) -> bool:
    """A shortcut's Exe (often quoted, any slashes and case) names OBSE's loader or Oblivion."""
    name = exe.strip().strip('"').replace("\\", "/").rsplit("/", 1)[-1].lower()
    return name in GAME_STARTERS


def library_paths(libraryfolders: dict):
    """Every library's path in steamapps/libraryfolders.vdf."""
    folders = ci_get(libraryfolders, "libraryfolders") or {}
    paths = []
    for value in folders.values():
        if isinstance(value, dict) and ci_get(value, "path"):
            paths.append(ci_get(value, "path"))
    return paths


def mo2_value(raw: str) -> str:
    """A ModOrganizer.ini value: @ByteArray(...) unwrapped, QSettings' doubled
    backslashes made single, quotes taken off."""
    value = raw.strip()
    m = re.fullmatch(r"@ByteArray\((.*)\)", value)
    if m:
        value = m.group(1)
    if len(value) >= 2 and value[0] == value[-1] == '"':
        value = value[1:-1]
    return value.replace("\\\\", "\\")


def parse_mo2_ini(text: str) -> dict:
    """ModOrganizer.ini's sections and keys: {"General": {...}, "Settings": {...}}."""
    sections: dict = {}
    current = sections.setdefault("", {})
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        m = re.fullmatch(r"\[(.+)\]", line)
        if m:
            current = sections.setdefault(m.group(1), {})
            continue
        if "=" in line:
            key, value = line.split("=", 1)
            current[key.strip()] = mo2_value(value)
    return sections


def enabled_mods(modlist: str):
    """MO2's profiles/<name>/modlist.txt: '+Name' enabled, '-Name' off, '*Name' a
    foreign one, '#' a comment; the top line has the highest priority."""
    mods = []
    for raw in modlist.splitlines():
        line = raw.strip()
        if line.startswith("+"):
            mods.append(line[1:])
    return mods


def windows_to_local(path: str, linux: bool) -> str:
    """A Windows path in a Linux file (MO2 under Wine): Z:\\ is the Linux root;
    another drive letter is left as it is (and then not found)."""
    if linux and re.match(r"^[Zz]:[\\/]", path):
        return "/" + path[3:].replace("\\", "/")
    return path.replace("\\", "/") if linux else path


def summary(results) -> str:
    problems = sum(1 for r in results if r.state == PROBLEM)
    if problems == 0:
        return "All good - start the game!"
    return f"{problems} problem{'s' if problems != 1 else ''} found - fix {'them' if problems != 1 else 'it'}, then run the doctor again."


# ------------------------------------------------------------------ the install


def ci_path(base: Path, relative: str):
    """`relative` under `base`, each part matched in any case (Linux file systems
    are case sensitive, Windows and Wine are not). None when not there."""
    current = base
    for part in re.split(r"[\\/]+", relative):
        if not part:
            continue
        exact = current / part
        if exact.exists():
            current = exact
            continue
        try:
            match = next((c for c in current.iterdir() if c.name.lower() == part.lower()), None)
        except OSError:
            match = None
        if match is None:
            return None
        current = match
    return current


@dataclass
class Install:
    game: Path
    found_by: str
    linux: bool
    mo2_mods: list = field(default_factory=list)  # enabled mods' folders, top priority first
    mo2_note: str = ""
    steam_root: Path | None = None

    def data_file(self, relative: str):
        """A Data file as the game sees it: the real Data folder, or with MO2 one of
        its enabled mods (or its overwrite folder)."""
        real = ci_path(self.game, "Data/" + relative)
        if real is not None:
            return real
        for mod in self.mo2_mods:
            found = ci_path(mod, relative)
            if found is not None:
                return found
        return None

    def game_file(self, relative: str):
        return ci_path(self.game, relative)


def steam_roots(linux: bool):
    if linux:
        home = Path.home()
        candidates = [
            home / ".local/share/Steam",
            home / ".steam/steam",
            home / ".var/app/com.valvesoftware.Steam/.local/share/Steam",
        ]
    else:
        candidates = []
        steam = os.environ.get("OBVR_STEAM_PATH")
        if steam:
            candidates.append(Path(steam))
        for env in ("ProgramFiles(x86)", "ProgramFiles"):
            if os.environ.get(env):
                candidates.append(Path(os.environ[env]) / "Steam")
    seen, roots = set(), []
    for c in candidates:
        try:
            real = c.resolve()
        except OSError:
            continue
        if real.is_dir() and real not in seen:
            seen.add(real)
            roots.append(real)
    return roots


def steam_libraries(root: Path):
    libraries = [root]
    vdf = ci_path(root, "steamapps/libraryfolders.vdf")
    if vdf is not None:
        try:
            for p in library_paths(parse_vdf(vdf.read_text(encoding="utf-8", errors="replace"))):
                path = Path(p)
                if path not in libraries:
                    libraries.append(path)
        except OSError:
            pass
    return libraries


def find_game(argument, script_dir: Path, linux: bool):
    """The Oblivion folder and how it was found, or (None, why)."""
    def is_game(p: Path):
        return p is not None and ci_path(p, "Oblivion.exe") is not None

    if argument:
        p = Path(argument).expanduser()
        if p.is_file():
            p = p.parent
        return (p, "the folder you gave") if is_game(p) else (None, f"no Oblivion.exe in {p}")
    # Next to itself: the zip puts the doctor in Data/OBSE/Plugins.
    for up in (script_dir, *script_dir.parents[:4]):
        if is_game(up):
            return up, "the doctor's own place"
    if is_game(Path.cwd()):
        return Path.cwd(), "the current folder"
    for root in steam_roots(linux):
        for library in steam_libraries(root):
            game = ci_path(library, "steamapps/common/Oblivion")
            if is_game(game):
                return game, f"the Steam library {library}"
    return None, "not found"


def find_mo2(argument, script_dir: Path, linux: bool):
    """ModOrganizer.ini: the one given, one above the doctor (a portable instance,
    the doctor then inside its mods folder), or on Windows a global instance for
    Oblivion under %LOCALAPPDATA%/ModOrganizer."""
    if argument:
        p = Path(argument).expanduser()
        ini = p if p.is_file() else ci_path(p, "ModOrganizer.ini")
        return ini, "the folder you gave"
    for up in script_dir.parents:
        ini = ci_path(up, "ModOrganizer.ini")
        if ini is not None:
            return ini, "a portable MO2 above the doctor"
    local = os.environ.get("LOCALAPPDATA")
    if local and not linux:
        base = Path(local) / "ModOrganizer"
        if base.is_dir():
            for instance in sorted(base.iterdir()):
                ini = ci_path(instance, "ModOrganizer.ini")
                if ini is None:
                    continue
                try:
                    settings = parse_mo2_ini(ini.read_text(encoding="utf-8", errors="replace"))
                except OSError:
                    continue
                if (settings.get("General", {}).get("gameName", "").lower()) == "oblivion":
                    return ini, f"the MO2 instance {instance.name}"
    return None, ""


def mo2_mod_folders(ini: Path, linux: bool):
    """The enabled mods of the selected profile, top priority first, then the
    overwrite folder; and the game path MO2 has. ([], note) when it cannot be read."""
    try:
        settings = parse_mo2_ini(ini.read_text(encoding="utf-8", errors="replace"))
    except OSError as error:
        return [], None, f"could not read {ini}: {error}"
    general = settings.get("General", {})
    paths = settings.get("Settings", {})
    base = paths.get("base_directory") or str(ini.parent)
    base = windows_to_local(base, linux)

    def expand(key, default):
        value = paths.get(key) or default
        return Path(windows_to_local(value.replace("%BASE_DIR%", base), linux))

    mods_dir = expand("mod_directory", "%BASE_DIR%/mods")
    profiles_dir = expand("profiles_directory", "%BASE_DIR%/profiles")
    overwrite = expand("overwrite_directory", "%BASE_DIR%/overwrite")
    profile = general.get("selected_profile") or "Default"
    game_path = general.get("gamePath")
    modlist = ci_path(profiles_dir, f"{profile}/modlist.txt")
    if modlist is None:
        return [], game_path, f"no modlist.txt for the profile '{profile}' in {profiles_dir}"
    folders = []
    for name in enabled_mods(modlist.read_text(encoding="utf-8", errors="replace")):
        folder = ci_path(mods_dir, name)
        if folder is not None:
            folders.append(folder)
    if overwrite.is_dir():
        folders.insert(0, overwrite)
    return folders, game_path, f"MO2 profile '{profile}', {len(folders)} enabled mod folder(s) searched as Data"


# ------------------------------------------------------------------ the checks


def read(path):
    try:
        return path.read_bytes() if path is not None else None
    except OSError:
        return None


def check_all(inst: Install):
    results = []
    linux = inst.linux

    exe = read(inst.game_file("Oblivion.exe"))
    version = pe_file_version(exe) if exe else None
    if version == (1, 2, 0, 416):
        results.append(Result(OK, "Oblivion 1.2.0.416"))
    elif version is None:
        results.append(Result(INFO, "Oblivion.exe has no version to read - could not verify it is 1.2.0.416"))
    else:
        results.append(Result(PROBLEM, f"Oblivion.exe is version {'.'.join(map(str, version))}, OBVR needs 1.2.0.416",
                              "Install the last official patch (the Steam version already has it)."))

    xobse = read(inst.game_file("obse_1_2_416.dll"))
    loader = inst.game_file("obse_loader.exe")
    if xobse is None or loader is None:
        missing = [n for n, f in (("obse_1_2_416.dll", xobse), ("obse_loader.exe", loader)) if f is None]
        results.append(Result(PROBLEM, f"xOBSE is missing ({', '.join(missing)})",
                              "Copy xOBSE's files into the Oblivion folder: https://github.com/llde/xOBSE/releases/latest"))
    else:
        v = pe_file_version(xobse)
        found = (v[1], v[2]) if v else None
        if found is None:
            results.append(Result(INFO, "xOBSE is there - could not read its version"))
        elif found >= MIN_XOBSE:
            results.append(Result(OK, f"xOBSE {found[0]}.{found[1]}"))
        else:
            results.append(Result(PROBLEM, f"xOBSE {found[0]}.{found[1]} is too old",
                                  "Install xOBSE 22.13 or newer: https://github.com/llde/xOBSE/releases/latest"))

    d3d9 = read(inst.game_file("d3d9.dll"))
    if linux:
        results.append(Result(OK, "DXVK: Proton brings its own"))
        if d3d9 is not None:
            results.append(Result(INFO, "a d3d9.dll is in the Oblivion folder - Proton does not need it"))
    elif d3d9 is None:
        results.append(Result(PROBLEM, "DXVK is missing (no d3d9.dll next to Oblivion.exe)",
                              "From the DXVK gplasync zip's x32 folder, copy d3d9.dll next to Oblivion.exe."))
    elif b"-gplasync" in d3d9:
        results.append(Result(OK, "DXVK gplasync"))
    elif b"DXVK" in d3d9:
        results.append(Result(PROBLEM, "DXVK is there, but not the gplasync build",
                              "Use d3d9.dll from the gplasync zip's x32 folder: https://gitlab.com/Ph42oN/dxvk-gplasync/-/releases"))
    else:
        results.append(Result(PROBLEM, "the d3d9.dll next to Oblivion.exe is not DXVK",
                              "Replace it with d3d9.dll from the gplasync zip's x32 folder."))
    if not linux and d3d9 is not None and pe_machine(d3d9) not in (None, 0x14C):
        results.append(Result(PROBLEM, "d3d9.dll is the 64-bit one", "Take d3d9.dll from the zip's x32 folder."))

    conf_path = inst.game_file("dxvk.conf")
    conf = read(conf_path)
    two_lines = "dxvk.enableMemoryDefrag = False  and  d3d9.cachedWriteOnlyBuffers = True"
    if conf is None:
        results.append(Result(PROBLEM, "dxvk.conf is missing",
                              f"Make a text file dxvk.conf next to Oblivion.exe with two lines: {two_lines}"))
        if inst.game_file("dxvk.conf.txt") is not None:
            results.append(Result(INFO, "found dxvk.conf.txt - rename it to dxvk.conf (Windows hides the .txt ending)"))
    else:
        defrag_off, cached_on = parse_dxvk_conf(conf.decode("utf-8", errors="replace"))
        if defrag_off and cached_on:
            results.append(Result(OK, "dxvk.conf has both lines"))
        else:
            lacking = [l for l, ok in (("dxvk.enableMemoryDefrag = False", defrag_off),
                                       ("d3d9.cachedWriteOnlyBuffers = True", cached_on)) if not ok]
            results.append(Result(PROBLEM, f"dxvk.conf lacks {' and '.join(lacking)}",
                                  f"Add {'these lines' if len(lacking) > 1 else 'this line'} to dxvk.conf."))
        if conf_path is not None and conf_path.name != "dxvk.conf":
            results.append(Result(INFO, f"the file is named {conf_path.name} - DXVK looks for dxvk.conf in lower case on Linux"))

    menuque = inst.data_file("OBSE/Plugins/MenuQue.dll")
    sub = read(inst.data_file("OBSE/Plugins/MenuQue/Submodule.Game.dll"))
    if menuque is None:
        results.append(Result(PROBLEM, "MenuQue is not installed",
                              "Install MenuQue v16b: https://www.nexusmods.com/oblivion/mods/32200"))
    elif sub is None:
        results.append(Result(PROBLEM, "MenuQue is incomplete (no MenuQue/Submodule.Game.dll)",
                              "Install MenuQue v16b again, with its MenuQue folder."))
    elif hashlib.sha256(sub).hexdigest() == MENUQUE_V16B_SHA256:
        results.append(Result(OK, "MenuQue v16b"))
    else:
        results.append(Result(INFO, "MenuQue is there, but not the v16b file OBVR was tested with - could not verify its version"))

    dll = read(inst.data_file("OBSE/Plugins/OBVR.dll"))
    if dll is None:
        results.append(Result(PROBLEM, "OBVR.dll is missing",
                              "Unpack the OBVR zip into Oblivion's Data folder (or install it as a mod in MO2)."))
    else:
        results.append(Result(OK, "OBVR.dll"))
    missing = [f for f in PLUGIN_FILES if inst.data_file("OBSE/Plugins/" + f) is None]
    missing += [f for f in DATA_FILES if inst.data_file(f) is None]
    total = len(PLUGIN_FILES) + len(DATA_FILES)
    if missing:
        results.append(Result(PROBLEM, f"{len(missing)} of OBVR's {total} files missing, e.g. {missing[0]}",
                              "Unpack the OBVR zip into Data again, keeping its folders."))
    else:
        results.append(Result(OK, f"all {total} OBVR files"))
    openvr = read(inst.data_file("OBSE/Plugins/openvr_api.dll"))
    if openvr is None:
        results.append(Result(PROBLEM, "openvr_api.dll is missing next to OBVR.dll", "Unpack the OBVR zip again."))
    elif pe_machine(openvr) != 0x14C:
        results.append(Result(PROBLEM, "openvr_api.dll is not the 32-bit one", "Take openvr_api.dll from the OBVR zip."))
    else:
        results.append(Result(OK, "openvr_api.dll, 32-bit"))

    results.extend(check_obng(inst))

    conflicts = [name for name, rel in (("Enhanced Camera", "OBSE/Plugins/OBSE_EnhancedCamera.dll"),
                                        ("Oblivion Reloaded", "OBSE/Plugins/OblivionReloaded.dll"))
                 if inst.data_file(rel) is not None]
    if conflicts:
        results.append(Result(PROBLEM, f"{' and '.join(conflicts)} {'do' if len(conflicts) > 1 else 'does'} not work with OBVR",
                              "Remove " + " and ".join("OBSE_EnhancedCamera.dll" if c == "Enhanced Camera" else "OblivionReloaded.dll"
                                                        for c in conflicts) + " from Data/OBSE/Plugins."))
    else:
        results.append(Result(OK, "no Enhanced Camera, no Oblivion Reloaded"))
    trifle = read(inst.data_file("OBSE/Plugins/Trifle.ini"))
    if trifle is not None and trifle_shadow_on(trifle.decode("utf-8", errors="replace")):
        results.append(Result(PROBLEM, "Trifle hides your body in VR",
                              "Set EnableFirstPersonShadow=0 in Data/OBSE/Plugins/Trifle.ini."))

    results.extend(check_steam(inst))
    results.extend(check_log(inst, dll))
    return results


def check_obng(inst: Install):
    """OBNG.dll comes with OBVR (since 2026-11-12): with it and its stereo on OBVR
    draws both eyes in one pass; without, it falls back to two passes - it still
    works, only slower, so a missing piece is a problem with that said."""
    slower = "VR still works, but draws the world twice (two passes) - slower"
    dll = read(inst.data_file("OBSE/Plugins/OBNG.dll"))
    if dll is None:
        return [Result(PROBLEM, f"OBNG.dll is missing next to OBVR.dll - {slower}",
                       "Unpack the OBVR zip again - OBNG.dll and OBNG.ini come with it.")]
    if pe_machine(dll) != 0x14C:
        return [Result(PROBLEM, f"OBNG.dll is not the 32-bit one - {slower}", "Take OBNG.dll from the OBVR zip.")]
    results = []
    ini = read(inst.data_file("OBSE/Plugins/OBNG.ini"))
    if ini is None:
        results.append(Result(PROBLEM, f"OBNG.ini is missing next to OBNG.dll - {slower}",
                              "Unpack the OBVR zip again (it brings OBNG.ini)."))
    else:
        draw, eyes = obng_stereo(ini.decode("utf-8", errors="replace"))
        if draw == "1" and eyes == "2":
            results.append(Result(OK, "Single pass: OBNG.dll with StereoDraw=1, StereoEyes=2"))
        else:
            results.append(Result(PROBLEM, f"OBNG.ini has StereoDraw={draw or '(none)'}, StereoEyes={eyes or '(none)'} - {slower}",
                                  "In Data/OBSE/Plugins/OBNG.ini under [Route] set StereoDraw=1 and StereoEyes=2 "
                                  "(or take OBNG.ini from the OBVR zip)."))
    obvr_ini = read(inst.data_file("OBSE/Plugins/OBVR.ini"))
    switch = ini_flag(obvr_ini.decode("utf-8", errors="replace"), "Render", "SinglePassStereo") if obvr_ini else None
    if switch is not None and switch != "1":
        results.append(Result(INFO, f"OBVR.ini has SinglePassStereo={switch}: OBVR draws two passes on purpose "
                                    "(1, the default, uses OBNG's single pass)"))
    return results


def check_steam(inst: Install):
    """The Steam overlay for Oblivion, and SteamVR, from the Steam install the game is in."""
    results = []
    root = inst.steam_root
    if root is None:
        results.append(Result(INFO, "the game is not in a Steam library - Steam overlay and SteamVR not checked"))
        return results
    users = []
    userdata = ci_path(root, "userdata")
    if userdata is not None:
        for user in userdata.iterdir():
            config = ci_path(user, "config/localconfig.vdf")
            if config is not None:
                users.append(config)
    if not users:
        results.append(Result(INFO, "could not find Steam's localconfig.vdf - check the Steam overlay by hand"))
    else:
        newest = max(users, key=lambda p: p.stat().st_mtime)  # the user who last ran Steam
        try:
            state = overlay_state(parse_vdf(newest.read_text(encoding="utf-8", errors="replace")), OBLIVION_APP_ID)
        except OSError:
            state = None
        if state is None:
            results.append(Result(INFO, "could not read Steam's settings - check the Steam overlay by hand"))
        elif state == "on":
            results.append(Result(PROBLEM, "the Steam overlay is on for Oblivion",
                                  "In Steam: right-click Oblivion > Properties > General, switch the overlay off."))
        else:
            results.append(Result(OK, "Steam overlay off" + (" (for all games)" if state == "off-all" else "")))
        shortcuts_file = ci_path(newest.parent, "shortcuts.vdf")
        if shortcuts_file is not None:
            try:
                shortcuts = shortcut_overlays(shortcuts_file.read_bytes())
            except OSError:
                shortcuts = None
            for name, exe, on in shortcuts or []:
                if on and starts_the_game(exe):
                    results.append(Result(INFO, f"the non-Steam shortcut \"{name}\" starts the game with Steam's overlay on - "
                                                "if you start from it: in Steam right-click it > Properties, overlay off"))
    steamvr = None
    for library in steam_libraries(root):
        steamvr = steamvr or ci_path(library, "steamapps/common/SteamVR")
    if steamvr is not None:
        results.append(Result(OK, "SteamVR installed"))
    else:
        results.append(Result(PROBLEM, "SteamVR is not installed", "Install SteamVR from Steam."))
    return results


def check_log(inst: Install, dll):
    """OBVR.log from the last start: there before the first start or not, it
    is only ever information - plus the problems OBVR found itself."""
    log = inst.game_file("OBVR.log")
    if log is None:
        return [Result(INFO, "OBVR has not run yet (no OBVR.log) - that's fine before the first start")]
    text = (read(log) or b"").decode("utf-8", errors="replace")
    age = time.time() - log.stat().st_mtime
    when = "today" if age < 86400 else f"{int(age // 86400)} day(s) ago"
    results = []
    if "OBVR ready" in text:
        results.append(Result(INFO, f"OBVR.log: the last start ({when}) loaded OBVR"))
    else:
        results.append(Result(INFO, f"OBVR.log: the last start ({when}) did not get OBVR ready - see OBVR.log"))
    path = None
    for line in text.splitlines():
        if "Single-pass stereo: OBNG took the eyes" in line:
            path = "single" if "- single pass through OBNG" in line else "dual"
    if path == "single":
        results.append(Result(INFO, f"OBVR.log: the last start ({when}) drew both eyes in one pass (OBNG)"))
    elif path == "dual":
        results.append(Result(INFO, f"OBVR.log: the last start ({when}) drew the two passes (no single pass)"))
    for line in text.splitlines():
        m = re.search(r"Self-check: (\S+) (PROBLEM|HINT) - (.*)", line)
        if m:
            results.append(Result(INFO, f"in the game OBVR found: {m.group(3)}"))
    return results


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Checks an OBVR install. Read-only.")
    parser.add_argument("game", nargs="?", help="the Oblivion folder (found by itself when left out)")
    parser.add_argument("--mo2", help="Mod Organizer 2: its instance folder or ModOrganizer.ini")
    parser.add_argument("--rules", choices=("windows", "linux"), help=argparse.SUPPRESS)  # tests: the other OS's rules
    parser.add_argument("--steam", help=argparse.SUPPRESS)  # tests: the Steam folder the game is in
    args = parser.parse_args(argv)
    linux = (args.rules == "linux") if args.rules else not sys.platform.startswith("win")
    script_dir = Path(__file__).resolve().parent
    print("OBVR doctor - checks your install, changes nothing.\n")

    mo2_ini, mo2_how = find_mo2(args.mo2, script_dir, linux)
    mods, mo2_game, mo2_note = ([], None, "")
    if mo2_ini is not None:
        mods, mo2_game, mo2_note = mo2_mod_folders(mo2_ini, linux)
    game, how = find_game(args.game, script_dir, linux)
    if game is None and mo2_game:
        candidate = Path(windows_to_local(mo2_game, linux))
        if ci_path(candidate, "Oblivion.exe") is not None:
            game, how = candidate, "MO2's game path"
    if game is None:
        print(f"[ PROBLEM ] Could not find the Oblivion folder ({how}).")
        print("            Fix: run it again with the folder: python3 obvr-doctor.py /path/to/Oblivion")
        print("\n1 problem found - fix it, then run the doctor again.")
        return 1
    steam_root = Path(args.steam) if args.steam else None
    for root in ([] if steam_root else steam_roots(linux)):
        for library in steam_libraries(root):
            try:
                if Path(game).resolve().is_relative_to(Path(library).resolve()):
                    steam_root = root
            except OSError:
                pass
    inst = Install(game=Path(game), found_by=how, linux=linux, mo2_mods=mods, mo2_note=mo2_note, steam_root=steam_root)
    print(f"Game folder: {game}  (found by {how})")
    if mo2_ini is not None:
        print(f"Mod Organizer 2: {mo2_ini}  ({mo2_how}) - {mo2_note}")
        if not mods:
            print("[ INFO    ] Could not read MO2's mod list - files inside MO2 mods may show as missing.")
            print("            Run it with --mo2 /path/to/your/MO2/instance")
    print()
    results = check_all(inst)
    for r in results:
        print(f"[ {r.state:<7} ] {r.text}")
        if r.fix:
            print(f"            Fix: {r.fix}")
    print("\n" + summary(results))
    return 1 if any(r.state == PROBLEM for r in results) else 0


if __name__ == "__main__":
    sys.exit(main())
