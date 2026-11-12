#!/usr/bin/env python3
"""Builds the release archive on Linux - the same layout as package-release.ps1.

Reads the version from the project() line of CMakeLists.txt, takes the DLL and
its map from the build directory given (the clang-cl build by default) and
OBVR.ini from the repository root, and writes dist/OBVR-<version><tag>.zip laid
out so that it extracts into Oblivion's Data folder and installs as an ordinary
Mod Organizer 2 mod alike:

  OBSE/Plugins/OBVR.dll
  OBSE/Plugins/OBVR.ini
  OBSE/Plugins/OBVR_Controls.ini
  OBSE/Plugins/OBVR-LICENSE.txt         (the GPL-3.0, which travels with every copy)
  OBSE/Plugins/OBVR_Input/              (the SteamVR action manifest and bindings)
  OBSE/Plugins/OBVR_Sounds/             (the teleport and other sounds)
  OBSE/Plugins/OBVR_Stage/              (the loading stage: the dome SteamVR shows while the game loads)
  OBSE/Plugins/openvr_api.dll           (Valve's 32-bit OpenVR client, third_party/openvr)
  OBSE/Plugins/openvr_api-LICENSE.txt   (its BSD-3-Clause, which binary copies carry)
  OBSE/Plugins/OBVR-doctor.bat, OBVR-doctor.ps1, obvr-doctor.py   (the install check)
  OBSE/Plugins/OBNG.dll, OBNG.ini, OBNG-LICENSE.txt   (OBNG: the single-pass stereo, from --obng-dir)
  Menus/Generic/OBVR_*.xml, Menus/Prefabs/OBVR/   (assets/menus)
  Textures/Menus/OBVR/controls_*.dds              (assets/textures: the controls card's drawings)
  Meshes/characters/_male/specialanims/*_OBVRMirror.kf  (assets/meshes: the mirrored staggers, tools/kf-mirror.py)

The linker map is copied beside the archive as OBVR-<version><tag>.map, for
reading an OBVR.dll+offset from a report back to a function, and OBNG.dll's
SHA-256 as OBVR-<version><tag>.obng.sha256. OBNG comes from ../OBNG (or
--obng-dir): build-plugin/OBNG.dll, plugin/OBNG-player.ini (shipped as
OBNG.ini, with StereoDraw=1 and StereoEyes=2) and LICENSE; the archive is
refused while any of them is missing or wrong, unless --without-obng says it
is meant to ship without (then OBVR runs the dual pass). Refuses to
package a DLL that is older than any source file, so a stale build cannot
become a release by accident; build first (tools/build-linux.sh).

  python3 tools/package-release.py                 # dist/OBVR-0.3.0.zip
  python3 tools/package-release.py --tag -beta1    # dist/OBVR-0.3.0-beta1.zip
  python3 tools/package-release.py --obng-dir /path/to/OBNG
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


def missing_stage_files(root: Path) -> list:
    """The loading stage's models the DLL names (src/vr/LoadingStage.h, "OBVR_Stage\\\\<name>.obj"),
    each with its material and its atlas: the .obj files were lost to .gitignore's *.obj once
    (2026-10-30), so an archive without a dome cannot be built by accident."""
    header = (root / "src/vr/LoadingStage.h").read_text(encoding="utf-8")
    names = re.findall(r'OBVR_Stage\\\\([A-Za-z0-9_]+)\.obj', header)
    if not names:
        return ["any model named in src/vr/LoadingStage.h"]
    missing = []
    for name in names:
        for suffix in (".obj", ".mtl", ".png"):
            if not (root / "assets/stage" / (name + suffix)).is_file():
                missing.append(name + suffix)
    return missing


def ini_value(text: str, section: str, key: str):
    """A value the way GetPrivateProfileIntA finds it: the section and the key in
    any case, the first line for the key, ';' lines skipped; a trailing comment
    cut off (the Int reader stops at the first non-digit anyway). None for none."""
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


# OBNG.dll, OBNG's engine plugin: OBVR's single-pass stereo draws the world
# once for both eyes through it (docs/single-pass-stereo-spec.md, A7); without
# it OBVR falls back to the dual pass. Bundled since 2026-11-12. Its archive
# names, beside OBVR's in OBSE/Plugins.
OBNG_DLL = ("build-plugin/OBNG.dll", "OBSE/Plugins/OBNG.dll")  # tools/build-plugin.sh --no-deploy
OBNG_PLAYER_INI = ("plugin/OBNG-player.ini", "OBSE/Plugins/OBNG.ini")  # the player's defaults, not OBNG.ini
OBNG_LICENSE = ("LICENSE", "OBSE/Plugins/OBNG-LICENSE.txt")  # GPL-3.0, as OBVR's
# The two [Route] keys that turn OBNG's stereo draw on (plugin/src/StereoEyesApi.h's
# ObngStereoPlan answers 2 only with both); anything else ships the dual pass.
OBNG_STEREO = (("StereoDraw", "1"), ("StereoEyes", "2"))


def pe_machine(data: bytes):
    if len(data) < 0x40 or data[:2] != b"MZ":
        return None
    at = int.from_bytes(data[0x3C:0x40], "little")
    if at + 6 > len(data) or data[at : at + 4] != b"PE\0\0":
        return None
    return int.from_bytes(data[at + 4 : at + 6], "little")


def obng_bundle(obng: Path):
    """(files as (source, archive name), problems, the DLL's SHA-256 or None).
    Every problem at once, so one run names all that is missing."""
    if not obng.is_dir():
        return [], [f"no OBNG folder at {obng} - give it with --obng-dir, or --without-obng"], None
    problems = []
    dll = obng / OBNG_DLL[0]
    sha = None
    if not dll.is_file():
        problems.append(f"no OBNG.dll at {dll} - build it (OBNG's tools/build-plugin.sh --no-deploy)")
    else:
        data = dll.read_bytes()
        sha = hashlib.sha256(data).hexdigest()
        if pe_machine(data) != 0x14C:
            problems.append(f"{dll} is not a 32-bit Windows DLL")
        sources = [p for folder in (obng / "plugin", obng / "src") if folder.is_dir()
                   for p in folder.rglob("*") if p.is_file() and p.suffix in (".cpp", ".h", ".inc", ".txt")
                   and p.stat().st_mtime > dll.stat().st_mtime]
        if sources:
            problems.append(f"OBNG.dll is older than {len(sources)} OBNG source file(s), e.g. "
                            f"{sources[0].relative_to(obng)} - rebuild OBNG first")
    ini = obng / OBNG_PLAYER_INI[0]
    if not ini.is_file():
        problems.append(f"no player INI at {ini} - it ships as OBSE/Plugins/OBNG.ini")
    else:
        text = ini.read_text(encoding="utf-8", errors="replace")
        wrong = [f"{key}={ini_value(text, 'Route', key) or '(none)'}" for key, want in OBNG_STEREO
                 if ini_value(text, "Route", key) != want]
        if wrong:
            problems.append(f"{ini} has {', '.join(wrong)} under [Route] - the single pass needs "
                            + " and ".join(f"{k}={v}" for k, v in OBNG_STEREO))
    licence = obng / OBNG_LICENSE[0]
    if not licence.is_file():
        problems.append(f"no licence at {licence} - OBNG.dll ships only with its GPL-3.0 text")
    files = [(obng / src, name) for src, name in (OBNG_DLL, OBNG_PLAYER_INI, OBNG_LICENSE)]
    return files, problems, sha


def add_tree(archive: zipfile.ZipFile, folder: Path, prefix: str) -> None:
    for path in sorted(folder.rglob("*")):
        if path.is_file():
            archive.write(path, f"{prefix}/{path.relative_to(folder).as_posix()}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default="build-linux-clangcl", help="where OBVR.dll and OBVR.map are")
    parser.add_argument("--out-dir", default="dist")
    parser.add_argument("--tag", default="", help="appended to the version in the file names, e.g. -beta1")
    parser.add_argument("--obng-dir", help="OBNG's repository (default: ../OBNG beside this one)")
    parser.add_argument("--without-obng", action="store_true",
                        help="ship without OBNG.dll on purpose - OBVR then draws the dual pass")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    obng_files, obng_sha = [], None
    if not args.without_obng:
        obng_dir = Path(args.obng_dir) if args.obng_dir else root.parent / "OBNG"
        obng_files, problems, obng_sha = obng_bundle(obng_dir)
        if problems:
            raise SystemExit("OBNG cannot be bundled:\n  " + "\n  ".join(problems))
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
    missing = missing_stage_files(root)
    if missing:
        raise SystemExit(f"assets/stage lacks {', '.join(missing)} - the loading stage the DLL names "
                         "(src/vr/LoadingStage.h); tools/make-loading-stage.ps1 writes them")
    out = root / args.out_dir
    out.mkdir(parents=True, exist_ok=True)
    archive_path = out / f"OBVR-{version}.zip"
    if archive_path.exists():
        archive_path.unlink()
    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(dll, "OBSE/Plugins/OBVR.dll")
        archive.write(ini, "OBSE/Plugins/OBVR.ini")
        archive.write(root / "OBVR_Controls.ini", "OBSE/Plugins/OBVR_Controls.ini")
        archive.write(root / "LICENSE", "OBSE/Plugins/OBVR-LICENSE.txt")
        archive.write(openvr, "OBSE/Plugins/openvr_api.dll")
        archive.write(openvr_license, "OBSE/Plugins/openvr_api-LICENSE.txt")
        # The doctor: double-click OBVR-doctor.bat (Windows) or run obvr-doctor.py (Linux).
        for doctor in ("OBVR-doctor.bat", "OBVR-doctor.ps1", "obvr-doctor.py"):
            archive.write(root / "tools" / doctor, f"OBSE/Plugins/{doctor}")
        add_tree(archive, root / "assets/input", "OBSE/Plugins/OBVR_Input")
        add_tree(archive, root / "assets/sounds", "OBSE/Plugins/OBVR_Sounds")
        add_tree(archive, root / "assets/stage", "OBSE/Plugins/OBVR_Stage")
        add_tree(archive, root / "assets/menus", "Menus")
        add_tree(archive, root / "assets/textures", "Textures")
        add_tree(archive, root / "assets/meshes", "Meshes")
        for source, name in obng_files:
            archive.write(source, name)
        count = len(archive.namelist())
    if dll_map.is_file():
        shutil.copyfile(dll_map, out / f"OBVR-{version}.map")
    if obng_sha:
        (out / f"OBVR-{version}.obng.sha256").write_text(f"{obng_sha}  OBNG.dll\n", encoding="utf-8")
        print(f"OBNG.dll bundled, SHA-256 {obng_sha}")
    else:
        print("Without OBNG.dll (--without-obng): OBVR draws the dual pass")
    print(f"{archive_path} ({archive_path.stat().st_size} bytes, {count} files) from {dll} - the map beside it")
    return 0


if __name__ == "__main__":
    sys.exit(main())
