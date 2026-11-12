#!/usr/bin/env python3
"""Checks for tools/package-release.py's OBNG part: the bundle of OBNG.dll, the
player's OBNG.ini and OBNG's licence, built from fake OBNG folders - every
reason the archive refuses, and the files and names it takes when nothing is
wrong. Run with: python3 tools/package-release-test.py"""
import hashlib
import importlib.util
import os
import struct
import sys
import tempfile
import time
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

print("OK" if failures == 0 else f"{failures} FAILED")
sys.exit(1 if failures else 0)
