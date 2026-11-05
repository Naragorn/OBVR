#!/usr/bin/env python3
"""Copies a tree of files into a game folder, into the folders the install already has.

  python3 tools/deploy-tree.py <source folder> <base folder> <destination below the base>
  python3 tools/deploy-tree.py assets/meshes "$game/Data" Meshes

Windows takes Data\\Meshes\\Characters and data\\meshes\\characters for one folder;
a Linux mount of the same disk does not. A plain cp -r of assets/meshes/characters
onto the mounted Windows install made a second folder beside the game's Characters,
and Windows, which shows one of the two, never saw the files in it (2026-11-05: the
mirrored staggers were "missing" there). So every name below the base - the
destination's and the tree's, folders and files - is looked up among what is there
without regard to case, and what is there is used; only what is not there is made,
in the source's case.

Two names in one folder that differ only in case are refused, with both named:
which of them Windows shows is not this tool's to guess. Everything is resolved
before the first file is copied, so a refusal copies nothing.
"""
import os
import shutil
import sys
from pathlib import Path


class CaseCollision(Exception):
    """Two names in one folder that differ only in case."""


def resolve_name(names: list, wanted: str) -> str:
    """The name to use for `wanted` in a folder holding `names`: the one there that
    matches without regard to case, else `wanted` itself. Two such there are refused."""
    matches = [name for name in names if name.lower() == wanted.lower()]
    if len(matches) > 1:
        raise CaseCollision(" and ".join(sorted(matches)) + " differ only in case")
    return matches[0] if matches else wanted


def resolve_path(start: Path, parts: tuple, ends_in_file: bool) -> Path:
    """`parts` below `start`, each taken as it is there; from the first one that is
    not there on, as given. A file where a folder is wanted (or the reverse) is refused."""
    current, there = start, True
    for index, part in enumerate(parts):
        if not there:
            current = current / part
            continue
        names = os.listdir(current)
        try:
            name = resolve_name(names, part)
        except CaseCollision as error:
            raise CaseCollision(f"in {current}: {error} - Windows shows one of them; merge them first") from None
        current = current / name
        if name not in names:
            there = False
            continue
        wants_file = ends_in_file and index == len(parts) - 1
        if wants_file and current.is_dir():
            raise SystemExit(f"{current} is a folder where the file {part} belongs")
        if not wants_file and not current.is_dir():
            raise SystemExit(f"{current} is a file where the folder {part} belongs")
    return current


def deploy(source: Path, base: Path, destination: str) -> int:
    """Copies every file below `source` to `destination` below `base`; returns how many."""
    source, base = Path(source), Path(base)
    if not source.is_dir():
        raise SystemExit(f"No source folder at {source}")
    if not base.is_dir():
        raise SystemExit(f"No folder at {base} to deploy into - it is never made here")
    into = tuple(part for part in Path(destination).parts if part not in ("", "."))
    files = sorted(path for path in source.rglob("*") if path.is_file())
    plan = [(path, resolve_path(base, into + path.relative_to(source).parts, True)) for path in files]
    for path, target in plan:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
    return len(plan)


def main(arguments: list) -> int:
    if len(arguments) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    source, base, destination = arguments
    try:
        count = deploy(Path(source), Path(base), destination)
    except CaseCollision as error:
        print(f"deploy-tree: nothing copied - {error}", file=sys.stderr)
        return 1
    print(f"{count} file(s) from {source} to {destination} below {base}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
