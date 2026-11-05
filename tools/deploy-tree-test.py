#!/usr/bin/env python3
"""Checks for tools/deploy-tree.py: a tree copied into a game folder lands in the
folders the install already has, whatever their case - on a case-sensitive mount
of the Windows install a plain cp -r made Data/Meshes/characters beside the
game's Data/Meshes/Characters, and Windows never saw the files (2026-11-05).
Run with: python3 tools/deploy-tree-test.py"""
import importlib.util
import sys
import tempfile
from pathlib import Path

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("deploy_tree", here / "deploy-tree.py")
dt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dt)

failures = 0


def check(condition, what):
    global failures
    print("  %-5s %s" % ("ok" if condition else "FAIL", what))
    if not condition:
        failures += 1


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def tree(folder: Path) -> list:
    return sorted(p.relative_to(folder).as_posix() for p in folder.rglob("*"))


def refused(call) -> str:
    try:
        call()
    except dt.CaseCollision as error:
        return str(error)
    return ""


print("resolve_name:")
check(dt.resolve_name([], "Meshes") == "Meshes", "nothing there - the wanted name as it is")
check(dt.resolve_name(["Textures", "Meshes"], "Meshes") == "Meshes", "the same name there - taken")
check(dt.resolve_name(["Textures", "Characters"], "characters") == "Characters", "another case there - the one there is taken")
check(dt.resolve_name(["Characters2", "xcharacters"], "characters") == "characters", "only longer names there - the wanted name as it is")
check("Characters" in refused(lambda: dt.resolve_name(["Characters", "characters"], "characters")),
      "two there that differ only in case - refused, even with the wanted one among them")
check("CHARACTERS" in refused(lambda: dt.resolve_name(["Characters", "CHARACTERS"], "characters")),
      "two there that differ only in case - refused, the wanted one not among them")

print("deploy:")
with tempfile.TemporaryDirectory() as scratch:
    scratch = Path(scratch)
    source = scratch / "source"
    write(source / "characters/_male/specialanims/Mirror.kf", "new")
    write(source / "characters/_male/specialanims/other.kf", "other")
    write(source / "top.txt", "top")

    # An empty base: every folder is made in the source's case, the destination's too.
    base = scratch / "empty"
    base.mkdir()
    copied = dt.deploy(source, base, "Meshes")
    check(copied == 3, "an empty base - three files copied")
    check(tree(base) == ["Meshes", "Meshes/characters", "Meshes/characters/_male",
                         "Meshes/characters/_male/specialanims",
                         "Meshes/characters/_male/specialanims/Mirror.kf",
                         "Meshes/characters/_male/specialanims/other.kf", "Meshes/top.txt"],
          "an empty base - the tree in the source's case")

    # The Windows install: its own case all the way down, and a file of another case.
    base = scratch / "windows"
    write(base / "meshes/Characters/_Male/SpecialAnims/mirror.KF", "old")
    write(base / "meshes/Characters/_Male/SpecialAnims/keep.kf", "kept")
    copied = dt.deploy(source, base, "Meshes")
    check(copied == 3, "an install in its own case - three files copied")
    check(tree(base) == ["meshes", "meshes/Characters", "meshes/Characters/_Male",
                         "meshes/Characters/_Male/SpecialAnims",
                         "meshes/Characters/_Male/SpecialAnims/keep.kf",
                         "meshes/Characters/_Male/SpecialAnims/mirror.KF",
                         "meshes/Characters/_Male/SpecialAnims/other.kf", "meshes/top.txt"],
          "an install in its own case - its folders and its file's name kept, no second folder")
    check((base / "meshes/Characters/_Male/SpecialAnims/mirror.KF").read_text() == "new",
          "an install in its own case - the file of another case overwritten where it is")
    check((base / "meshes/Characters/_Male/SpecialAnims/keep.kf").read_text() == "kept",
          "an install in its own case - a file the source lacks is left")

    # A second run over the same install copies again and adds nothing.
    before = tree(base)
    check(dt.deploy(source, base, "Meshes") == 3 and tree(base) == before, "a second run - the same tree")

    # A destination several folders deep, part of it there in another case.
    base = scratch / "deep"
    (base / "DATA").mkdir(parents=True)
    dt.deploy(source, base, "Data/Meshes")
    check((base / "DATA/Meshes/top.txt").is_file() and not (base / "Data").exists(),
          "a destination two folders deep - the part there is taken, the rest made")

    # Two folders that differ only in case: refused before anything is copied.
    base = scratch / "collision"
    write(base / "Meshes/Characters/_male/Specialanims/keep.kf", "kept")
    (base / "Meshes/characters").mkdir()
    before = tree(base)
    message = refused(lambda: dt.deploy(source, base, "Meshes"))
    check("Characters" in message and "characters" in message, "two folders differing in case - refused, both named")
    check(tree(base) == before, "two folders differing in case - nothing copied, not even top.txt")

    # Two files that differ only in case: refused as well.
    base = scratch / "file-collision"
    write(base / "Meshes/TOP.txt", "a")
    write(base / "Meshes/top.TXT", "b")
    before = tree(base)
    check(refused(lambda: dt.deploy(source, base, "Meshes")) != "" and tree(base) == before,
          "two files differing in case - refused, nothing copied")

    # A file where a folder is wanted, and a folder where a file is wanted.
    base = scratch / "file-in-the-way"
    write(base / "Meshes/Characters", "a file")
    try:
        dt.deploy(source, base, "Meshes")
        check(False, "a file where a folder is wanted - refused")
    except SystemExit as error:
        check("Characters" in str(error), "a file where a folder is wanted - refused, named")
    base = scratch / "folder-in-the-way"
    (base / "Meshes/Top.txt").mkdir(parents=True)
    try:
        dt.deploy(source, base, "Meshes")
        check(False, "a folder where a file is wanted - refused")
    except SystemExit as error:
        check("Top.txt" in str(error), "a folder where a file is wanted - refused, named")

    # No source, no base.
    try:
        dt.deploy(scratch / "nowhere", scratch / "empty", "Meshes")
        check(False, "a missing source - refused")
    except SystemExit:
        check(True, "a missing source - refused")
    try:
        dt.deploy(source, scratch / "no-base", "Meshes")
        check(False, "a missing base - refused (a game folder is never made)")
    except SystemExit:
        check(not (scratch / "no-base").exists(), "a missing base - refused (a game folder is never made)")

print("main:")
with tempfile.TemporaryDirectory() as scratch:
    scratch = Path(scratch)
    write(scratch / "source/a.txt", "a")
    (scratch / "base/DATA").mkdir(parents=True)
    check(dt.main([str(scratch / "source"), str(scratch / "base"), "Data/Menus"]) == 0
          and (scratch / "base/DATA/Menus/a.txt").is_file(), "copies and returns 0")
    (scratch / "base/data").mkdir()
    check(dt.main([str(scratch / "source"), str(scratch / "base"), "Data/Menus"]) == 1, "a collision returns 1")
    check(dt.main([str(scratch / "source")]) == 2, "too few arguments return 2")

print("%d failure(s)" % failures)
sys.exit(1 if failures else 0)
