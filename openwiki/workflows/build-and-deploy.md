---
type: workflow
title: Build, deploy and release
description: How the 32-bit OBVR.dll is built with MSVC on Windows or clang-cl on Linux, how tools/build-linux.sh deploys it to the Linux and Windows game installs at once, what CI runs, and how a release archive is packaged.
tags: [build, cmake, clang-cl, deploy, release, ci]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-27T21:08:40.633Z
sources:
  - id: openwiki-source-7a80b79a6fb3618cbfab08a2
    resource: repo://.github/workflows/build.yml
  - id: openwiki-source-b62fa4f033f15c1ef906572f
    resource: repo://cmake/toolchain-linux-clang-cl.cmake
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-3ceeb7b5df15f6df319782cc
    resource: repo://tools/build-linux.sh
  - id: openwiki-source-a1d8c25c5086107f18c63b9c
    resource: repo://tools/package-release.py
generated: { by: "claude-code", at: "2026-10-27T21:08:40.633Z" }
---

# Build, deploy and release

OBVR is one CMake project (`CMakeLists.txt`, version from its `project(OBVR VERSION …)` line, passed to the code as `OBVR_VERSION_STRING`). It must be built **32-bit** because `Oblivion.exe` is 32-bit; the CMake file stops with an error for any other pointer size. The unit tests are a separate project, see [Unit tests](../testing/unit-tests.md).

## Windows (MSVC)

```bat
cmake -B build -A Win32 .
cmake --build build --config Release
```

This is what CI does, and releases are built this way.

## Linux (clang-cl)

`cmake/toolchain-linux-clang-cl.cmake` builds the real DLL with clang-cl and lld-link against Microsoft's C runtime, C++ library and Windows SDK, laid out as a winsysroot by `xwin` (default `~/tools/xwin`, override with `OBVR_WINSYSROOT`). Same ABI and build definition as MSVC: CMake treats clang-cl as MSVC, so the MSVC branch of `CMakeLists.txt` (warnings, static runtime, linker map) applies. The Microsoft files are downloaded once by `xwin` and are not part of the repository. More background: `docs/linux.md`.

## tools/build-linux.sh

The everyday Linux command:

| Option | Effect |
| --- | --- |
| *(none)* | build `build-linux-clangcl`, deploy `OBVR.dll` and `OBVR.map` |
| `--test` | build and run the unit tests (`build-linux-tests`, clang++ -m32) first |
| `--assets` | also deploy menus, their textures, the SteamVR input manifest and sounds from `assets/` |
| `--linux-only` | deploy to the Linux game folder only |
| `--no-deploy` | build only |

Deployment rules:

- **Both installs or none:** by default it deploys to the Linux Steam install (`OBVR_GAME_DIR`) and the Windows install on the NTFS partition (`OBVR_WINDOWS_GAME_DIR`), so the two never run different builds. If either plugin folder is missing nothing is deployed.
- **Never into a running game:** it scans `/proc` for an `Oblivion.exe` process (under Proton its name reads differently, so it checks the command line) and refuses to deploy while one runs.
- **Never half-written:** each file is copied beside the target and renamed over it, then the deployed DLL's SHA-256 prefix is compared with the built one (`DEPLOY MISMATCH` on a difference). The printed hash is the way to check which build an install carries.
- A failed build deploys nothing; the old DLL stays.

## CI

`.github/workflows/build.yml` ("Build and test") runs on `windows-latest` for pushes to `main`, `v*` tags, pull requests and manual dispatch: configure and build the DLL Win32, set up Python for the visual tests, build the test project Win32, run `ctest`. It keeps the DLL, its linker map and the INI as an artifact.

## Release packaging

`tools/package-release.py` (Linux; `package-release.ps1` on Windows) reads the version from `CMakeLists.txt` and writes `dist/OBVR-<version><tag>.zip`, laid out to extract into Oblivion's `Data` folder and to install as a Mod Organizer 2 mod:

- `OBSE/Plugins/`: `OBVR.dll`, `OBVR.ini`, the GPL-3.0 licence, `OBVR_Input/` (SteamVR action manifest and bindings), `OBVR_Sounds/`, Valve's 32-bit `openvr_api.dll` (checked against a pinned SHA-256) with its BSD-3-Clause licence;
- `Menus/` and `Textures/Menus/OBVR/` from `assets/`.

The linker map is written beside the archive for resolving `OBVR.dll+offset` from crash reports. It refuses to package a DLL older than any source file, so a stale build cannot become a release.

## Related

- [Hand-script harness](../testing/hand-script-harness.md) — running a deployed build in the game.
- [Configuration](../operations/configuration.md) — the INI that ships with the DLL.
