"""The OBVR doctor (tools/obvr-doctor.py): its readers, and every check's OK,
PROBLEM and INFO over fake installs built in a temporary folder - Windows'
rules and Linux's, a Steam library with its overlay settings, Mod Organizer 2
with its mods as Data, and the summary."""

import contextlib
import hashlib
import importlib.util
import io
import struct
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("obvr_doctor", ROOT / "tools" / "obvr-doctor.py")
doctor = importlib.util.module_from_spec(spec)
sys.modules["obvr_doctor"] = doctor
spec.loader.exec_module(doctor)

failures = 0
checks = 0


def check(condition, what):
    global failures, checks
    checks += 1
    print(f"  {'ok  ' if condition else 'FAIL'}  {what}")
    if not condition:
        failures += 1


def pe(machine=0x14C, version=None, extra=b""):
    """A tiny PE file: MZ, the PE header with its machine, a VS_FIXEDFILEINFO."""
    data = bytearray(0x100)
    data[0:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", data, 0x84, machine)
    if version is not None:
        a, b, c, d = version
        data += struct.pack("<IIII", 0xFEEF04BD, 0x10000, (a << 16) | b, (c << 16) | d)
    return bytes(data) + extra


# ------------------------------------------------------------------ readers

print("Readers")
check(doctor.pe_file_version(pe(version=(1, 2, 0, 416))) == (1, 2, 0, 416), "a FileVersion read")
check(doctor.pe_file_version(pe()) is None, "no version resource: None")
check(doctor.pe_machine(pe(0x14C)) == 0x14C and doctor.pe_machine(pe(0x8664)) == 0x8664, "the machine")
check(doctor.pe_machine(b"XX" + bytes(100)) is None and doctor.pe_machine(b"MZ") is None, "no PE: None")
bad = bytearray(pe())
bad[0x80] = ord("X")
check(doctor.pe_machine(bytes(bad)) is None, "no PE signature: None")
check(doctor.parse_dxvk_conf("dxvk.enableMemoryDefrag = False\nd3d9.cachedWriteOnlyBuffers = True") == (True, True),
      "dxvk.conf: the README's lines")
check(doctor.parse_dxvk_conf("dxvk.enableMemoryDefrag=false # x\r\nd3d9.cachedWriteOnlyBuffers= TRUE\r\n") == (True, True),
      "dxvk.conf: spaces, CRLF, comments, case")
check(doctor.parse_dxvk_conf("# dxvk.enableMemoryDefrag = False\nd3d9.cachedWriteOnlyBuffers = 1") == (False, False),
      "dxvk.conf: a comment is no line, 1 is no boolean")
check(doctor.parse_dxvk_conf("dxvk.enableMemoryDefrag = False\ndxvk.enableMemoryDefrag = True") == (False, False),
      "dxvk.conf: the last line wins")
check(doctor.trifle_shadow_on("[x]\nEnableFirstPersonShadow=1\n"), "Trifle on")
check(not doctor.trifle_shadow_on("; EnableFirstPersonShadow=1\nEnableFirstPersonShadow = 0"), "Trifle off, comment skipped")
check(not doctor.trifle_shadow_on(""), "Trifle: nothing set")
vdf = doctor.parse_vdf('"libraryfolders"\n{\n "0"\n {\n  "path"  "C:\\\\Steam"\n }\n "1" { "path" "D:\\\\Games" }\n}\n')
check(doctor.library_paths(vdf) == ["C:\\Steam", "D:\\Games"], "libraryfolders.vdf: both paths, backslashes undoubled")
off_game = doctor.parse_vdf('"UserLocalConfigStore" { "Apps" { "22330" { "OverlayAppEnable" "0" } } }')
check(doctor.overlay_state(off_game, "22330") == "off-game", "overlay off for Oblivion (\"Apps\" in any case)")
check(doctor.overlay_state(off_game, "1") == "on", "another game's entry: on")
off_all = doctor.parse_vdf('"UserLocalConfigStore" { "system" { "EnableGameOverlay" "0" } }')
check(doctor.overlay_state(off_all, "22330") == "off-all", "overlay off for all games")
on = doctor.parse_vdf('"UserLocalConfigStore" { "apps" { "22330" { "OverlayAppEnable" "1" } } }')
check(doctor.overlay_state(on, "22330") == "on" and doctor.overlay_state({}, "22330") == "on", "on, and on by default")
ini = doctor.parse_mo2_ini("[General]\ngameName=Oblivion\ngamePath=@ByteArray(C:\\\\Games\\\\Oblivion)\n"
                           "selected_profile=@ByteArray(Default)\n[Settings]\nmod_directory=\"%BASE_DIR%/mods\"\n; c\n")
check(ini["General"]["gamePath"] == "C:\\Games\\Oblivion" and ini["General"]["selected_profile"] == "Default",
      "ModOrganizer.ini: @ByteArray unwrapped, backslashes undoubled")
check(ini["Settings"]["mod_directory"] == "%BASE_DIR%/mods", "ModOrganizer.ini: quotes off")
check(doctor.enabled_mods("# c\n+OBVR\n-Off\n*Foreign\n+MenuQue\n") == ["OBVR", "MenuQue"], "modlist.txt: + only, in order")
check(doctor.windows_to_local("Z:\\home\\x\\MO2", True) == "/home/x/MO2", "Wine's Z: is the Linux root")
check(doctor.windows_to_local("C:\\MO2", False) == "C:\\MO2", "Windows paths stay")
check(doctor.summary([doctor.Result("OK", "a"), doctor.Result("INFO", "b")]) == "All good - start the game!",
      "summary: all good, INFO does not count")
check(doctor.summary([doctor.Result("PROBLEM", "a")]).startswith("1 problem found - fix it"), "summary: one")
check(doctor.summary([doctor.Result("PROBLEM", "a")] * 3).startswith("3 problems found - fix them"), "summary: three")

# ------------------------------------------------------------------ installs


def write(path: Path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data if isinstance(data, bytes) else data.encode())


def sound_install(root: Path, windows=True):
    """Everything right, the way the README has it."""
    game = root / "Steam/steamapps/common/Oblivion"
    write(game / "Oblivion.exe", pe(version=(1, 2, 0, 416)))
    write(game / "obse_1_2_416.dll", pe(version=(0, 22, 13, 0)))
    write(game / "obse_loader.exe", pe())
    if windows:
        write(game / "d3d9.dll", pe(extra=b"DXVK: v3.1.1-1-gplasync"))
    write(game / "dxvk.conf", "dxvk.enableMemoryDefrag = False\nd3d9.cachedWriteOnlyBuffers = True\n")
    plugins = game / "Data/OBSE/Plugins"
    write(plugins / "MenuQue.dll", pe())
    sub = b"menuque v16b"
    write(plugins / "MenuQue/Submodule.Game.dll", sub)
    doctor.MENUQUE_V16B_SHA256 = hashlib.sha256(sub).hexdigest()
    write(plugins / "OBVR.dll", pe())
    write(plugins / "openvr_api.dll", pe(0x14C))
    write(plugins / "OBNG.dll", pe(0x14C))
    write(plugins / "OBNG.ini", "[Shadow]\nEnabled=1\n[Route]\nStereoDraw=1\nStereoEyes=2\n")
    for f in doctor.PLUGIN_FILES:
        write(plugins / f, "x")
    for f in doctor.DATA_FILES:
        write(game / "Data" / f, "x")
    steam = root / "Steam"
    write(steam / "steamapps/common/SteamVR/bin/x", "x")
    write(steam / "userdata/1/config/localconfig.vdf",
          '"UserLocalConfigStore" { "apps" { "22330" { "OverlayAppEnable" "0" } } }')
    return game, steam



def shortcuts_vdf(entries):
    """A shortcuts.vdf the way Steam writes it: binary KeyValues."""
    out = bytearray(b"\x00shortcuts\x00")
    for i, (name, exe, allow) in enumerate(entries):
        out += b"\x00" + str(i).encode() + b"\x00"
        out += b"\x02appid\x00" + (0x80000000 + i).to_bytes(4, "little")
        out += b"\x01AppName\x00" + name.encode() + b"\x00"
        out += b"\x01Exe\x00" + exe.encode() + b"\x00"
        out += b"\x02AllowOverlay\x00" + allow.to_bytes(4, "little")
        out += b"\x07LastPlayTime64\x00" + bytes(8)
        out += b"\x00tags\x00\x08"
        out += b"\x08"
    out += b"\x08\x08"
    return bytes(out)

def run(game, steam=None, linux=False, mods=()):
    inst = doctor.Install(game=game, found_by="test", linux=linux, mo2_mods=list(mods), steam_root=steam)
    return doctor.check_all(inst)


def states(results):
    return [r.state for r in results]


def has(results, state, word):
    return any(r.state == state and word in r.text for r in results)


print("A sound Windows install")
with tempfile.TemporaryDirectory() as tmp:
    game, steam = sound_install(Path(tmp))
    r = run(game, steam)
    check(doctor.PROBLEM not in states(r), "no problem")
    check(has(r, "OK", "Oblivion 1.2.0.416") and has(r, "OK", "xOBSE 22.13") and has(r, "OK", "DXVK gplasync"),
          "Oblivion, xOBSE and DXVK named")
    check(has(r, "OK", "MenuQue v16b") and has(r, "OK", "all 40 OBVR files") and has(r, "OK", "Steam overlay off"),
          "MenuQue, the files, the overlay")
    check(has(r, "INFO", "has not run yet"), "no OBVR.log: only information")
    check(doctor.summary(r) == "All good - start the game!", "the summary says start")

print("Problems, one at a time")
with tempfile.TemporaryDirectory() as tmp:
    game, steam = sound_install(Path(tmp))
    write(game / "Oblivion.exe", pe(version=(1, 1, 0, 511)))
    check(has(run(game, steam), "PROBLEM", "1.1.0.511"), "another Oblivion version")
    write(game / "Oblivion.exe", pe())
    check(has(run(game, steam), "INFO", "could not verify"), "an exe without a version: could not verify")
    write(game / "Oblivion.exe", pe(version=(1, 2, 0, 416)))
    write(game / "obse_1_2_416.dll", pe(version=(0, 22, 12, 0)))
    check(has(run(game, steam), "PROBLEM", "xOBSE 22.12 is too old"), "xOBSE too old")
    write(game / "obse_1_2_416.dll", pe(version=(0, 23, 0, 0)))
    check(has(run(game, steam), "OK", "xOBSE 23.0"), "xOBSE 23.0 is new enough")
    write(game / "obse_1_2_416.dll", pe())
    check(has(run(game, steam), "INFO", "could not read its version"), "xOBSE without a version")
    (game / "obse_loader.exe").unlink()
    check(has(run(game, steam), "PROBLEM", "obse_loader.exe"), "xOBSE's loader missing")
    write(game / "obse_loader.exe", pe())
    write(game / "d3d9.dll", pe(extra=b"DXVK: v2.7"))
    check(has(run(game, steam), "PROBLEM", "not the gplasync"), "plain DXVK")
    write(game / "d3d9.dll", pe())
    check(has(run(game, steam), "PROBLEM", "is not DXVK"), "a d3d9.dll that is no DXVK")
    write(game / "d3d9.dll", pe(0x8664, extra=b"-gplasync"))
    check(has(run(game, steam), "PROBLEM", "64-bit"), "a 64-bit d3d9.dll")
    (game / "d3d9.dll").unlink()
    check(has(run(game, steam), "PROBLEM", "DXVK is missing"), "no d3d9.dll")
    write(game / "d3d9.dll", pe(extra=b"-gplasync"))
    write(game / "dxvk.conf", "dxvk.enableMemoryDefrag = False\n")
    r = run(game, steam)
    check(has(r, "PROBLEM", "lacks d3d9.cachedWriteOnlyBuffers = True") and not has(r, "PROBLEM", "lacks dxvk.enable"),
          "dxvk.conf without the buffers line")
    write(game / "dxvk.conf", "")
    check(has(run(game, steam), "PROBLEM", "and d3d9.cachedWriteOnlyBuffers"), "an empty dxvk.conf: both lines")
    (game / "dxvk.conf").unlink()
    write(game / "dxvk.conf.txt", "x")
    r = run(game, steam)
    check(has(r, "PROBLEM", "dxvk.conf is missing") and has(r, "INFO", "dxvk.conf.txt"), "no dxvk.conf, a .txt one named")

with tempfile.TemporaryDirectory() as tmp:
    game, steam = sound_install(Path(tmp))
    plugins = game / "Data/OBSE/Plugins"
    write(plugins / "MenuQue/Submodule.Game.dll", b"another")
    check(has(run(game, steam), "INFO", "not the v16b file"), "another MenuQue: could not verify")
    (plugins / "MenuQue/Submodule.Game.dll").unlink()
    check(has(run(game, steam), "PROBLEM", "incomplete"), "MenuQue without its folder")
    (plugins / "MenuQue.dll").unlink()
    check(has(run(game, steam), "PROBLEM", "MenuQue is not installed"), "no MenuQue")
    (plugins / "OBVR.dll").unlink()
    (game / "Data/Menus/generic/OBVR_Update.xml").unlink()
    r = run(game, steam)
    check(has(r, "PROBLEM", "OBVR.dll is missing") and has(r, "PROBLEM", "1 of OBVR's 40 files missing, e.g. Menus/generic/OBVR_Update.xml"),
          "OBVR.dll and one file missing")
    write(plugins / "openvr_api.dll", pe(0x8664))
    check(has(run(game, steam), "PROBLEM", "not the 32-bit"), "a 64-bit openvr_api.dll")
    (plugins / "openvr_api.dll").unlink()
    check(has(run(game, steam), "PROBLEM", "openvr_api.dll is missing"), "no openvr_api.dll")
    write(plugins / "OBSE_EnhancedCamera.dll", "x")
    r = run(game, steam)
    check(has(r, "PROBLEM", "Enhanced Camera does not work") and not has(r, "PROBLEM", "Reloaded"), "Enhanced Camera alone")
    write(plugins / "OblivionReloaded.dll", "x")
    check(has(run(game, steam), "PROBLEM", "Enhanced Camera and Oblivion Reloaded do not"), "both conflicts")
    write(plugins / "Trifle.ini", "EnableFirstPersonShadow=1\n")
    check(has(run(game, steam), "PROBLEM", "Trifle"), "Trifle's shadow on")
    write(plugins / "Trifle.ini", "EnableFirstPersonShadow=0\n")
    check(not has(run(game, steam), "PROBLEM", "Trifle"), "Trifle's shadow off: nothing said")

print("Steam")
with tempfile.TemporaryDirectory() as tmp:
    game, steam = sound_install(Path(tmp))
    write(steam / "userdata/1/config/localconfig.vdf", '"UserLocalConfigStore" { "apps" { } }')
    check(has(run(game, steam), "PROBLEM", "overlay is on"), "no entry for Oblivion: the overlay is on")
    write(steam / "userdata/1/config/localconfig.vdf", '"UserLocalConfigStore" { "system" { "EnableGameOverlay" "0" } }')
    check(has(run(game, steam), "OK", "for all games"), "off for all games")
    # Two users: the one whose file was written last counts.
    import os, time
    write(steam / "userdata/2/config/localconfig.vdf", '"UserLocalConfigStore" { }')
    later = time.time() + 10
    os.utime(steam / "userdata/2/config/localconfig.vdf", (later, later))
    check(has(run(game, steam), "PROBLEM", "overlay is on"), "two users: the newest file's")
    # Non-Steam shortcuts that start the game: their own overlay switch.
    write(steam / "userdata/2/config/localconfig.vdf", '"UserLocalConfigStore" { "apps" { "22330" { "OverlayAppEnable" "0" } } }')
    os.utime(steam / "userdata/2/config/localconfig.vdf", (later, later))
    write(steam / "userdata/2/config/shortcuts.vdf",
          shortcuts_vdf([("OBVR - TES 4 Oblivion VR", '"C:\\Steam\\steamapps\\common\\Oblivion\\obse_loader.exe"', 1),
                         ("Notepad", "notepad.exe", 1),
                         ("Oblivion plain", "C:/Games/Oblivion/OBLIVION.EXE", 0)]))
    r = run(game, steam)
    check(has(r, "OK", "Steam overlay off") and has(r, "INFO", "OBVR - TES 4 Oblivion VR") and
          not has(r, "INFO", "Notepad") and not has(r, "INFO", "Oblivion plain"),
          "a shortcut to obse_loader.exe with the overlay on: named as information; other or off shortcuts: nothing")
    check(doctor.shortcut_overlays(b"\x00shortcuts\x00\x09") is None, "a broken shortcuts.vdf: not read")
    import shutil
    shutil.rmtree(steam / "userdata")
    check(has(run(game, steam), "INFO", "localconfig.vdf"), "no userdata: check by hand")
    shutil.rmtree(steam / "steamapps/common/SteamVR")
    check(has(run(game, steam), "PROBLEM", "SteamVR is not installed"), "no SteamVR")
    check(has(run(game, None), "INFO", "not in a Steam library"), "a game outside Steam (GOG, disc): not checked")
    # SteamVR in a second library.
    write(steam / "steamapps/libraryfolders.vdf", f'"libraryfolders" {{ "0" {{ "path" "{Path(tmp) / "Lib2"}" }} }}')
    write(Path(tmp) / "Lib2/steamapps/common/SteamVR/x", "x")
    check(has(run(game, steam), "OK", "SteamVR installed"), "SteamVR in another library")

print("OBNG and the single pass")
check(doctor.obng_stereo("[Route]\nStereoDraw=1\nStereoEyes=2\n") == ("1", "2"), "the two [Route] keys read")
check(doctor.obng_stereo("[route]\r\n; StereoDraw=0\r\nstereodraw = 1 ; on\r\nStereoDraw=0\r\n") == ("1", None),
      "case, comments, the first line for a key (GetPrivateProfileIntA), a missing key None")
check(doctor.obng_stereo("[Shadow]\nStereoDraw=1\n") == (None, None), "another section's key: not counted")
check(doctor.ini_flag("[Render]\nSinglePassStereo=0\n", "Render", "SinglePassStereo") == "0", "OBVR.ini's switch read")
with tempfile.TemporaryDirectory() as tmp:
    game, steam = sound_install(Path(tmp))
    plugins = game / "Data/OBSE/Plugins"
    r = run(game, steam)
    check(has(r, "OK", "Single pass: OBNG.dll"), "a sound install: the single pass named OK")
    write(plugins / "OBNG.ini", "[Route]\nStereoDraw=0\nStereoEyes=2\n")
    r = run(game, steam)
    check(has(r, "PROBLEM", "StereoDraw=0") and has(r, "PROBLEM", "two passes"), "OBNG's stereo off: the slower two passes")
    write(plugins / "OBNG.ini", "[Route]\n")
    check(has(run(game, steam), "PROBLEM", "StereoDraw=(none), StereoEyes=(none)"), "neither key: both named")
    (plugins / "OBNG.ini").unlink()
    check(has(run(game, steam), "PROBLEM", "OBNG.ini is missing"), "no OBNG.ini")
    write(plugins / "OBNG.ini", "[Route]\nStereoDraw=1\nStereoEyes=2\n")
    write(plugins / "OBNG.dll", pe(0x8664))
    check(has(run(game, steam), "PROBLEM", "OBNG.dll is not the 32-bit"), "a 64-bit OBNG.dll")
    (plugins / "OBNG.dll").unlink()
    r = run(game, steam)
    check(has(r, "PROBLEM", "OBNG.dll is missing") and not any("OBNG.ini" in x.text for x in r), "no OBNG.dll: one line, its INI not judged")
    write(plugins / "OBNG.dll", pe(0x14C))
    write(plugins / "OBVR.ini", "[Render]\nSinglePassStereo=0\n")
    r = run(game, steam)
    check(has(r, "INFO", "SinglePassStereo=0") and not has(r, "PROBLEM", "pass"),
          "the single pass switched off in OBVR.ini: only a note (a choice, not a fault)")
with tempfile.TemporaryDirectory() as tmp:
    game, steam = sound_install(Path(tmp))
    eyes = "Single-pass stereo: OBNG took the eyes {} at scene call 9 - offset 1 2 3, eye 1 at x 4036; 3 set, 0 refused so far - {}, the world into the back buffer\n"
    write(game / "OBVR.log", "OBVR ready\n" + eyes.format("ok (0)", "dual pass") + eyes.format("ok (0)", "single pass through OBNG"))
    check(has(run(game, steam), "INFO", "drew both eyes in one pass"), "the log's last word: the single pass ran")
    write(game / "OBVR.log", "OBVR ready\n" + eyes.format("the route is off or has one eye (6)", "dual pass"))
    check(has(run(game, steam), "INFO", "drew the two passes"), "the log's last word: the dual pass ran")
    write(game / "OBVR.log", "OBVR ready\n")
    r = run(game, steam)
    check(not has(r, "INFO", "one pass") and not has(r, "INFO", "two passes"), "no world drawn in the log: nothing said")

print("OBVR.log")
with tempfile.TemporaryDirectory() as tmp:
    game, steam = sound_install(Path(tmp))
    write(game / "OBVR.log", "OBVR 0.3.0 - Query\nOBVR ready\nSelf-check: dxvk HINT - DXVK is active, but not gplasync\n"
                             "Self-check: files ok - all 40 files\n")
    r = run(game, steam)
    check(has(r, "INFO", "loaded OBVR") and has(r, "INFO", "in the game OBVR found: DXVK is active"),
          "the last start loaded OBVR, its hint passed on, its ok lines not")
    check(doctor.PROBLEM not in states(r), "a log's lines never count as problems")
    write(game / "OBVR.log", "OBVR 0.3.0 - Query\nWrong Oblivion version\n")
    check(has(run(game, steam), "INFO", "did not get OBVR ready"), "a start that did not load OBVR")

print("Linux's rules")
with tempfile.TemporaryDirectory() as tmp:
    game, steam = sound_install(Path(tmp), windows=False)
    r = run(game, steam, linux=True)
    check(doctor.PROBLEM not in states(r) and has(r, "OK", "Proton brings its own"), "no d3d9.dll needed under Proton")
    write(game / "d3d9.dll", pe())
    check(has(run(game, steam, linux=True), "INFO", "does not need it"), "a d3d9.dll there: a note")
    (game / "dxvk.conf").rename(game / "DXVK.conf")
    r = run(game, steam, linux=True)
    check(has(r, "OK", "both lines") and has(r, "INFO", "DXVK.conf"), "found in another case, the name noted")
    (game / "Data/Menus/generic").rename(game / "Data/Menus/Generic")
    check(has(run(game, steam, linux=True), "OK", "all 40"), "folders in another case are found")

print("Mod Organizer 2")
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    game, steam = sound_install(tmp)
    # OBVR and MenuQue live as MO2 mods, not in the real Data.
    mo2 = tmp / "MO2"
    obvr_mod = mo2 / "mods/OBVR"
    menuque_mod = mo2 / "mods/MenuQue"
    for f in doctor.PLUGIN_FILES + ["OBVR.dll", "openvr_api.dll"]:
        src = game / "Data/OBSE/Plugins" / f
        write(obvr_mod / "OBSE/Plugins" / f, src.read_bytes())
        src.unlink()
    for f in doctor.DATA_FILES:
        write(obvr_mod / f, "x")
        (game / "Data" / f).unlink()
    for f in ("MenuQue.dll", "MenuQue/Submodule.Game.dll"):
        write(menuque_mod / "OBSE/Plugins" / f, (game / "Data/OBSE/Plugins" / f).read_bytes())
        (game / "Data/OBSE/Plugins" / f).unlink()
    r = run(game, steam)
    check(has(r, "PROBLEM", "OBVR.dll is missing") and has(r, "PROBLEM", "MenuQue is not installed"),
          "without MO2's mods: missing")
    write(mo2 / "ModOrganizer.ini", f"[General]\ngameName=Oblivion\ngamePath=@ByteArray({game})\n"
                                    "selected_profile=@ByteArray(Default)\n")
    write(mo2 / "profiles/Default/modlist.txt", "# order\n+OBVR\n+MenuQue\n-Old\n")
    folders, game_path, note = doctor.mo2_mod_folders(mo2 / "ModOrganizer.ini", linux=False)
    check([f.name for f in folders] == ["OBVR", "MenuQue"] and game_path == str(game) and "Default" in note,
          "the enabled mods in order, the game path")
    r = run(game, steam, mods=folders)
    check(doctor.PROBLEM not in states(r) and has(r, "OK", "all 40"), "with MO2's mods: everything found")
    write(mo2 / "overwrite/OBSE/Plugins/Trifle.ini", "EnableFirstPersonShadow=1")
    folders, _, _ = doctor.mo2_mod_folders(mo2 / "ModOrganizer.ini", linux=False)
    check(folders[0].name == "overwrite" and has(run(game, steam, mods=folders), "PROBLEM", "Trifle"),
          "MO2's overwrite folder counts first")
    write(mo2 / "ModOrganizer.ini", "[General]\nselected_profile=@ByteArray(Other)\n")
    folders, _, note = doctor.mo2_mod_folders(mo2 / "ModOrganizer.ini", linux=False)
    check(folders == [] and "no modlist.txt" in note, "a profile without its modlist: said")
    write(mo2 / "ModOrganizer.ini", f"[Settings]\nmod_directory=%BASE_DIR%/elsewhere\nbase_directory={mo2}\n")
    folders, _, _ = doctor.mo2_mod_folders(mo2 / "ModOrganizer.ini", linux=False)
    check(all(f.name == "overwrite" for f in folders), "another mods folder (empty): no mods found there")
    found, how = doctor.find_mo2(None, obvr_mod / "OBSE/Plugins", linux=False)
    check(found == mo2 / "ModOrganizer.ini" and "portable" in how, "the doctor inside an MO2 mod finds its MO2")
    found, how = doctor.find_mo2(str(mo2), tmp, linux=False)
    check(found == mo2 / "ModOrganizer.ini" and "you gave" in how, "--mo2 with the folder")

print("Finding the game")
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    game, steam = sound_install(tmp)
    found, how = doctor.find_game(None, game / "Data/OBSE/Plugins", linux=True)
    check(found == game and "own place" in how, "the doctor in Data/OBSE/Plugins finds its game")
    found, how = doctor.find_game(str(game), tmp, linux=True)
    check(found == game and "you gave" in how, "the folder given")
    found, how = doctor.find_game(str(game / "Oblivion.exe"), tmp, linux=True)
    check(found == game, "Oblivion.exe given")
    found, how = doctor.find_game(str(tmp), tmp, linux=True)
    check(found is None and "no Oblivion.exe" in how, "a wrong folder given: said so")

print("The whole run")
with tempfile.TemporaryDirectory() as tmp:
    game, steam = sound_install(Path(tmp))
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = doctor.main([str(game), "--rules", "windows", "--steam", str(steam)])
    text = out.getvalue()
    check(code == 0 and "All good - start the game!" in text and "[ OK      ] Oblivion 1.2.0.416" in text,
          "a sound install: exit 0, one line per check, the summary")
    (game / "dxvk.conf").unlink()
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = doctor.main([str(game), "--rules", "windows", "--steam", str(steam)])
    text = out.getvalue()
    check(code == 1 and "[ PROBLEM ] dxvk.conf is missing\n            Fix: Make a text file dxvk.conf" in text
          and "1 problem found" in text, "a problem: exit 1, the fix under it, counted")
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = doctor.main([str(Path(tmp))])
    check(code == 1 and "Could not find the Oblivion folder" in out.getvalue(), "no game: said how to point at it")

print(f"{checks - failures} of {checks} checks passed")
sys.exit(1 if failures else 0)
