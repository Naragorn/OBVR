#!/usr/bin/env python3
"""Checks for tools/package-release.py's OBNG part (the bundle of OBNG.dll, the
player's OBNG.ini and OBNG's licence, built from fake OBNG folders) and its
companion plugins (tools/release-companions.json: from fake zips and folders) -
every reason the archive refuses, and the files and names it takes when nothing
is wrong. Run with: python3 tools/package-release-test.py"""
import hashlib
import importlib.util
import os
import struct
import sys
import tempfile
import time
import zipfile
from pathlib import Path

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("package_release", here / "package-release.py")
pr = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pr)

failures = 0


def check(condition, what):
    global failures
    print("  %-5s %s" % ("ok" if condition else "FAIL", what))
    if not condition:
        failures += 1


def write(path: Path, data) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(data, str):
        path.write_text(data, encoding="utf-8")
    else:
        path.write_bytes(data)


def pe(machine=0x14C) -> bytes:
    data = bytearray(0x100)
    data[0:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", data, 0x84, machine)
    return bytes(data)


PLAYER_INI = "[Shadow]\nEnabled=1\n[Route]\n; one pass\nStereoDraw=1\nStereoEyes=2\n[Fix]\nX=1\n"


def sound_obng(root: Path) -> Path:
    obng = root / "OBNG"
    write(obng / "plugin/src/Plugin.cpp", "x")
    write(obng / "src/engine/A.h", "x")
    write(obng / "plugin/OBNG-player.ini", PLAYER_INI)
    write(obng / "LICENSE", "GPL")
    later = time.time() + 10
    write(obng / "build-plugin/OBNG.dll", pe())
    os.utime(obng / "build-plugin/OBNG.dll", (later, later))
    return obng


def has(problems, word):
    return any(word in p for p in problems)


print("The INI reader")
check(pr.ini_value(PLAYER_INI, "Route", "StereoDraw") == "1", "a key in its section")
check(pr.ini_value(PLAYER_INI, "Route", "Enabled") is None, "a key of another section: None")
check(pr.ini_value("[route]\r\nstereodraw = 1 ; on\r\n", "Route", "StereoDraw") == "1",
      "case, spaces, CRLF and a trailing comment")
check(pr.ini_value("[Route]\n;StereoDraw=1\n", "Route", "StereoDraw") is None, "a commented line is no value")
check(pr.ini_value("[Route]\nStereoDraw=1\nStereoDraw=0\n", "Route", "StereoDraw") == "1",
      "two lines for a key: the first wins, as OBNG's GetPrivateProfileIntA reads it")

print("A sound OBNG folder")
with tempfile.TemporaryDirectory() as tmp:
    obng = sound_obng(Path(tmp))
    files, problems, sha = pr.obng_bundle(obng)
    check(problems == [], "no problem")
    names = [name for _, name in files]
    check(names == ["OBSE/Plugins/OBNG.dll", "OBSE/Plugins/OBNG.ini", "OBSE/Plugins/OBNG-LICENSE.txt"],
          "the DLL, the player's INI as OBNG.ini, the licence")
    check(dict((n, s) for s, n in files)["OBSE/Plugins/OBNG.ini"].name == "OBNG-player.ini",
          "OBNG.ini comes from plugin/OBNG-player.ini, not the developer's OBNG.ini")
    check(sha == hashlib.sha256(pe()).hexdigest(), "the DLL's SHA-256")
    check(dict((n, s) for s, n in files)["OBSE/Plugins/OBNG-LICENSE.txt"].name == "LICENSE",
          "the licence from OBNG's LICENSE")
    (obng / "LICENSE").unlink()
    write(obng / "LICENSE.txt", "GPL")
    check(has(pr.obng_bundle(obng)[1], "no licence"), "only LICENSE counts (OBNG's GPL-3.0 file), not LICENSE.txt")

print("Refusals, one at a time")
with tempfile.TemporaryDirectory() as tmp:
    files, problems, sha = pr.obng_bundle(Path(tmp) / "nowhere")
    check(has(problems, "no OBNG folder") and files == [] and sha is None, "no OBNG folder")
with tempfile.TemporaryDirectory() as tmp:
    obng = sound_obng(Path(tmp))
    (obng / "LICENSE").unlink()
    check(has(pr.obng_bundle(obng)[1], "no licence"), "no licence file: refused")
with tempfile.TemporaryDirectory() as tmp:
    obng = sound_obng(Path(tmp))
    (obng / "build-plugin/OBNG.dll").unlink()
    check(has(pr.obng_bundle(obng)[1], "no OBNG.dll"), "no DLL")
with tempfile.TemporaryDirectory() as tmp:
    obng = sound_obng(Path(tmp))
    later = time.time() + 10
    write(obng / "build-plugin/OBNG.dll", pe(0x8664))
    os.utime(obng / "build-plugin/OBNG.dll", (later, later))
    check(has(pr.obng_bundle(obng)[1], "not a 32-bit"), "a 64-bit DLL")
    write(obng / "build-plugin/OBNG.dll", b"not a PE file")
    os.utime(obng / "build-plugin/OBNG.dll", (later, later))
    check(has(pr.obng_bundle(obng)[1], "not a 32-bit"), "no PE file at all")
with tempfile.TemporaryDirectory() as tmp:
    obng = sound_obng(Path(tmp))
    later = time.time() + 20
    os.utime(obng / "plugin/src/Plugin.cpp", (later, later))
    check(has(pr.obng_bundle(obng)[1], "older than 1 OBNG source"), "a plugin source newer than the DLL")
    os.utime(obng / "src/engine/A.h", (later, later))
    check(has(pr.obng_bundle(obng)[1], "older than 2 OBNG source"), "an engine header too")
with tempfile.TemporaryDirectory() as tmp:
    obng = sound_obng(Path(tmp))
    (obng / "plugin/OBNG-player.ini").unlink()
    check(has(pr.obng_bundle(obng)[1], "no player INI"), "no player INI")
    write(obng / "plugin/OBNG-player.ini", "[Route]\nStereoDraw=0\nStereoEyes=2\n")
    check(has(pr.obng_bundle(obng)[1], "StereoDraw=0"), "stereo draw off: refused")
    write(obng / "plugin/OBNG-player.ini", "[Route]\nStereoDraw=1\nStereoEyes=1\n")
    check(has(pr.obng_bundle(obng)[1], "StereoEyes=1"), "one eye: refused")
    write(obng / "plugin/OBNG-player.ini", "[Route]\n")
    check(has(pr.obng_bundle(obng)[1], "StereoDraw=(none)"), "neither key: refused, named as missing")
with tempfile.TemporaryDirectory() as tmp:
    obng = sound_obng(Path(tmp))
    (obng / "LICENSE").unlink()
    (obng / "plugin/OBNG-player.ini").unlink()
    check(len(pr.obng_bundle(obng)[1]) == 2, "every problem listed at once, not only the first")

LICENSE_TEXT = "GPL"
COMPANION_INI = "[Main]\r\nEnable=1\r\n[Leaves]\r\n; passes\r\nPasses=4\r\n[Grass]\r\nPasses=4\r\n"


def zip_with(path: Path, members: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w") as z:
        for name, data in members.items():
            z.writestr(name, data)


def sound_companions(parent: Path) -> list:
    """A zip-based companion (with a backslash member, as Sep2026LoadingCrashFix's zip has)
    and a folder-based one, as the real list has both."""
    zip_with(parent / "Zipped/dist/Zipped-1.0.zip", {
        "OBSE\\Plugins\\Zipped.dll": pe(), "OBSE/Plugins/Zipped.ini": COMPANION_INI, "LICENSE": LICENSE_TEXT})
    write(parent / "Folder/build/Folder.dll", pe())
    write(parent / "Folder/Folder.ini", "[Water]\nCalmFrom=200\n")
    write(parent / "Folder/LICENSE", LICENSE_TEXT)
    sha = hashlib.sha256(pe()).hexdigest()
    return [
        {"name": "Zipped", "from": "Zipped/dist/Zipped-1.0.zip",
         "files": {"OBSE/Plugins/Zipped.dll": "OBSE/Plugins/Zipped.dll",
                   "OBSE/Plugins/Zipped.ini": "OBSE/Plugins/Zipped.ini",
                   "OBSE/Plugins/Zipped-LICENSE.txt": "LICENSE"},
         "dll": "OBSE/Plugins/Zipped.dll", "sha256": sha,
         "ini": {"OBSE/Plugins/Zipped.ini": {"Leaves": {"Passes": "2"}, "Grass": {"Passes": "2"}}}},
        {"name": "Folder", "from": "Folder",
         "files": {"OBSE/Plugins/Folder.dll": "build/Folder.dll",
                   "OBSE/Plugins/Folder.ini": "Folder.ini",
                   "OBSE/Plugins/Folder-LICENSE.txt": "LICENSE"},
         "dll": "OBSE/Plugins/Folder.dll", "sha256": sha},
    ]


print("The INI setter")
set_text, missing = pr.set_ini_values(COMPANION_INI, {"Leaves": {"Passes": "2"}, "Grass": {"Passes": "2"}})
check(missing == [], "both keys found")
check(pr.ini_value(set_text, "Leaves", "Passes") == "2" and pr.ini_value(set_text, "Grass", "Passes") == "2",
      "each set in its own section, the other section's key of the same name not touched")
check("; passes\r\n" in set_text and set_text.endswith("\r\n") and "\r\nEnable=1\r\n" in set_text,
      "comments, other keys and CRLF kept")
_, missing = pr.set_ini_values(COMPANION_INI, {"Leaves": {"Sharpen": "1"}, "Water": {"Passes": "2"}})
check(missing == ["[Leaves] Sharpen", "[Water] Passes"], "a key or section the INI does not have: named, not added")
set_text, _ = pr.set_ini_values("[leaves]\nPASSES = 8 ; eight\nPasses=1\n", {"Leaves": {"Passes": "2"}})
check(set_text == "[leaves]\nPASSES=2\nPasses=1\n", "case-blind, the first line for the key (the one the game reads)")

print("The companion plugins: a sound list")
with tempfile.TemporaryDirectory() as tmp:
    parent = Path(tmp)
    entries, problems = pr.companion_bundle(parent, sound_companions(parent))
    check(problems == [], "no problem")
    names = [name for name, _ in entries]
    check(names == ["OBSE/Plugins/Zipped.dll", "OBSE/Plugins/Zipped.ini", "OBSE/Plugins/Zipped-LICENSE.txt",
                    "OBSE/Plugins/Folder.dll", "OBSE/Plugins/Folder.ini", "OBSE/Plugins/Folder-LICENSE.txt"],
          "every file under its archive name, in the list's order")
    data = dict(entries)
    check(data["OBSE/Plugins/Zipped.dll"] == pe(), "a zip member named with a backslash is found")
    check(pr.ini_value(data["OBSE/Plugins/Zipped.ini"].decode(), "Leaves", "Passes") == "2", "the INI's values set")
    check(data["OBSE/Plugins/Zipped-LICENSE.txt"] == LICENSE_TEXT.encode(), "a zip's root LICENSE renamed")
    check(data["OBSE/Plugins/Folder.ini"] == b"[Water]\nCalmFrom=200\n", "an INI without settings kept as it is")
    check(all(n.startswith("OBSE/Plugins/") for n in names), "nothing lands in Data's root")

print("The companion plugins: refusals")
with tempfile.TemporaryDirectory() as tmp:
    parent = Path(tmp)
    spec_list = sound_companions(parent)
    (parent / "Zipped/dist/Zipped-1.0.zip").unlink()
    check(has(pr.companion_bundle(parent, spec_list)[1], "Zipped: no "), "the zip missing")
with tempfile.TemporaryDirectory() as tmp:
    parent = Path(tmp)
    spec_list = sound_companions(parent)
    (parent / "Folder/LICENSE").unlink()
    check(has(pr.companion_bundle(parent, spec_list)[1], "Folder: no "), "a folder's file missing")
with tempfile.TemporaryDirectory() as tmp:
    parent = Path(tmp)
    spec_list = sound_companions(parent)
    spec_list[0]["files"]["OBSE/Plugins/Zipped-README.txt"] = "README.txt"
    check(has(pr.companion_bundle(parent, spec_list)[1], "Zipped: no README.txt in"), "a zip member missing")
with tempfile.TemporaryDirectory() as tmp:
    parent = Path(tmp)
    spec_list = sound_companions(parent)
    spec_list[1]["sha256"] = "0" * 64
    check(has(pr.companion_bundle(parent, spec_list)[1], "Folder: OBSE/Plugins/Folder.dll has SHA-256"),
          "a DLL other than the pinned one")
with tempfile.TemporaryDirectory() as tmp:
    parent = Path(tmp)
    spec_list = sound_companions(parent)
    write(parent / "Folder/build/Folder.dll", pe(0x8664))
    spec_list[1]["sha256"] = hashlib.sha256(pe(0x8664)).hexdigest()
    check(has(pr.companion_bundle(parent, spec_list)[1], "not a 32-bit"), "a 64-bit DLL, even when pinned")
with tempfile.TemporaryDirectory() as tmp:
    parent = Path(tmp)
    spec_list = sound_companions(parent)
    spec_list[0]["ini"]["OBSE/Plugins/Zipped.ini"]["Leaves"]["Method"] = "supersample"
    check(has(pr.companion_bundle(parent, spec_list)[1], "Zipped: OBSE/Plugins/Zipped.ini has no [Leaves] Method"),
          "a setting the shipped INI lacks")
with tempfile.TemporaryDirectory() as tmp:
    parent = Path(tmp)
    spec_list = sound_companions(parent)
    spec_list[1]["dll"] = "OBSE/Plugins/Other.dll"
    check(has(pr.companion_bundle(parent, spec_list)[1], "Folder: its dll"), "the DLL not among its files")
with tempfile.TemporaryDirectory() as tmp:
    parent = Path(tmp)
    spec_list = sound_companions(parent)
    spec_list[1]["files"]["OBSE/Plugins/Zipped.dll"] = "build/Folder.dll"
    check(has(pr.companion_bundle(parent, spec_list)[1], "twice"), "two plugins naming the same archive entry")
with tempfile.TemporaryDirectory() as tmp:
    parent = Path(tmp)
    spec_list = sound_companions(parent)
    (parent / "Zipped/dist/Zipped-1.0.zip").unlink()
    (parent / "Folder/LICENSE").unlink()
    check(len(pr.companion_bundle(parent, spec_list)[1]) == 2, "every problem listed at once")

print("The real list")
real = pr.load_companions(here / "release-companions.json")
check([c["name"] for c in real] == ["FoliageAA", "WaterAA", "OblivionFrameFixes", "Sep2026LoadingCrashFix"],
      "the four plugins of beta 9")
check(real[0]["ini"]["OBSE/Plugins/FoliageAA.ini"] == {"Leaves": {"Passes": "2"}, "Grass": {"Passes": "2"}},
      "FoliageAA at 2 passes for leaves and grass")
check(all(len(c["sha256"]) == 64 and c["dll"] in c["files"] for c in real), "every DLL pinned and shipped")

print("OK" if failures == 0 else f"{failures} FAILED")
sys.exit(1 if failures else 0)
