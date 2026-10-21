#!/usr/bin/env python3
"""Builds the release archive on Linux - the same layout as package-release.ps1.

Reads the version from the project() line of CMakeLists.txt, takes the DLL and
its map from the build directory given (the clang-cl build by default) and
OBVR.ini from the repository root, and writes dist/OBVR-<version><tag>.zip laid
out so that it extracts into Oblivion's Data folder and installs as an ordinary
Mod Organizer 2 mod alike:

  OBSE/Plugins/OBVR.dll
  OBSE/Plugins/OBVR.ini
  OBSE/Plugins/OBVR-LICENSE.txt         (the GPL-3.0, which travels with every copy)
  OBSE/Plugins/OBVR_Input/              (the SteamVR action manifest and bindings)
  OBSE/Plugins/OBVR_Sounds/             (the teleport and other sounds)
  OBSE/Plugins/openvr_api.dll           (Valve's 32-bit OpenVR client, third_party/openvr)
  OBSE/Plugins/openvr_api-LICENSE.txt   (its BSD-3-Clause, which binary copies carry)
  Menus/Generic/OBVR_*.xml, Menus/Prefabs/OBVR/   (assets/menus)
  Textures/Menus/OBVR/controls_*.dds              (assets/textures: the controls card's drawings)

The linker map is copied beside the archive as OBVR-<version><tag>.map, for
reading an OBVR.dll+offset from a report back to a function. Refuses to
package a DLL that is older than any source file, so a stale build cannot
become a release by accident; build first (tools/build-linux.sh).

  python3 tools/package-release.py                 # dist/OBVR-0.3.0.zip
  python3 tools/package-release.py --tag -beta1    # dist/OBVR-0.3.0-beta1.zip
"""
import argparse
import hashlib
import re
import shutil
import sys
import zipfile
from pathlib import Path

OPENVR_SHA256 = "ab696e4f218a95b3e396bc310f9fe6485df48c99c0969762083212b1e1f025a6"


def version_of(cmake_lists: Path) -> str:
    for line in cmake_lists.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^\s*project\(OBVR\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", line)
        if m:
            return m.group(1)
    raise SystemExit("CMakeLists.txt has no project(OBVR VERSION x.y.z) line to read the version from")


def newer_sources(root: Path, dll: Path) -> list:
    dll_time = dll.stat().st_mtime
    return [p for p in (root / "src").rglob("*") if p.is_file() and p.stat().st_mtime > dll_time]


def add_tree(archive: zipfile.ZipFile, folder: Path, prefix: str) -> None:
    for path in sorted(folder.rglob("*")):
        if path.is_file():
            archive.write(path, f"{prefix}/{path.relative_to(folder).as_posix()}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default="build-linux-clangcl", help="where OBVR.dll and OBVR.map are")
    parser.add_argument("--out-dir", default="dist")
    parser.add_argument("--tag", default="", help="appended to the version in the file names, e.g. -beta1")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    version = version_of(root / "CMakeLists.txt") + args.tag
    build = root / args.build_dir
    dll, dll_map, ini = build / "OBVR.dll", build / "OBVR.map", root / "OBVR.ini"
    if not dll.is_file():
        raise SystemExit(f"No DLL at {dll} - build first (tools/build-linux.sh)")
    openvr = root / "third_party/openvr/win32/openvr_api.dll"
    openvr_license = root / "third_party/openvr/LICENSE"
    if not openvr.is_file() or not openvr_license.is_file():
        raise SystemExit("third_party/openvr is incomplete - openvr_api.dll and LICENSE both belong there")
    found = hashlib.sha256(openvr.read_bytes()).hexdigest()
    if found != OPENVR_SHA256:
        raise SystemExit(f"third_party/openvr/win32/openvr_api.dll has SHA-256 {found}, not the pinned {OPENVR_SHA256}")
    stale = newer_sources(root, dll)
    if stale:
        names = ", ".join(str(p.relative_to(root)) for p in stale[:5])
        raise SystemExit(f"The DLL is older than {len(stale)} source file(s), e.g. {names} - rebuild before packaging")
    out = root / args.out_dir
    out.mkdir(parents=True, exist_ok=True)
    archive_path = out / f"OBVR-{version}.zip"
    if archive_path.exists():
        archive_path.unlink()
    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(dll, "OBSE/Plugins/OBVR.dll")
        archive.write(ini, "OBSE/Plugins/OBVR.ini")
        archive.write(root / "LICENSE", "OBSE/Plugins/OBVR-LICENSE.txt")
        archive.write(openvr, "OBSE/Plugins/openvr_api.dll")
        archive.write(openvr_license, "OBSE/Plugins/openvr_api-LICENSE.txt")
        add_tree(archive, root / "assets/input", "OBSE/Plugins/OBVR_Input")
        add_tree(archive, root / "assets/sounds", "OBSE/Plugins/OBVR_Sounds")
        add_tree(archive, root / "assets/menus", "Menus")
        add_tree(archive, root / "assets/textures", "Textures")
        count = len(archive.namelist())
    if dll_map.is_file():
        shutil.copyfile(dll_map, out / f"OBVR-{version}.map")
    print(f"{archive_path} ({archive_path.stat().st_size} bytes, {count} files) from {dll} - the map beside it")
    return 0


if __name__ == "__main__":
    sys.exit(main())
