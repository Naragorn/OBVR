---
type: testing
title: Unit tests and logic units
description: How OBVR keeps its decisions testable without the game - pure Logic units over plain values - and how the separate 32-bit test project is configured, built and run on Linux, Windows and CI.
tags: [testing, unit-tests, ctest, logic-units, tdd]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-27T21:08:40.633Z
sources:
  - id: openwiki-source-7a80b79a6fb3618cbfab08a2
    resource: repo://.github/workflows/build.yml
  - id: openwiki-source-23f0c126f78887f3e722dfd0
    resource: repo://src/camera/FrameLogic.h
  - id: openwiki-source-fa68239bf614d837d7e5522c
    resource: repo://tests/CMakeLists.txt
  - id: openwiki-source-3ceeb7b5df15f6df319782cc
    resource: repo://tools/build-linux.sh
generated: { by: "claude-code", at: "2026-10-27T21:08:40.633Z" }
---

# Unit tests and logic units

Most of OBVR runs inside a 32-bit game process and talks to the engine through hard-coded addresses, which cannot run in a test. The code is therefore split: **decisions** live in pure headers over plain values, **engine access** lives in the `.cpp` glue. The decisions are covered by native unit tests; the glue is covered in the running game by the [hand-script harness](hand-script-harness.md).

## The pattern

The first such unit was `src/camera/FrameLogic.h`: the camera hook's callback needs a live camera node and reads the player through a fixed address, but the decisions it makes — has a key just gone down, has the point of view changed, is a periodic action due — are plain state machines, so they were moved out where a test can drive them.

The same split is used throughout:

- `src/**/*Logic.h` (about 40 files), e.g. `game/BladeContactLogic.h`, `game/CapsuleFitLogic.h`, `game/WeaponShapeLogic.h`, `game/MeshGapLogic.h`;
- most of `src/vr/` (`Holster.h`, `Teleport.h`, `QuickMenu.h`, `Archery.h`, …), headers that say "Pure, covered by …_test";
- decision halves of systems such as `core/ReloadGate.h`, `render/SubmitPolicy`, `ui/SettingsMenu`;
- byte generation for patches (`core/EntryDetour`, `core/AroundCall`, `camera/CameraTrampoline`), so the part that crashes the game when wrong is checked without it;
- mirrored Windows, Direct3D, DXVK and OpenVR structs, whose sizes and offsets are pinned by tests.

New logic is expected to come with tests for every meaningful flow, including refusal and fallback paths (see `AGENTS.md`).

## The test project

`tests/CMakeLists.txt` is a standalone CMake project, not part of the DLL build: the DLL is cross-compiled for 32-bit Windows, while the tests run natively. They are still built **32-bit**, because the struct-layout tests must hold for the DLL's pointer size. GCC is refused (it does not accept the `__stdcall`/`__cdecl` keyword spelling of the mirrored interfaces); use clang on Linux or MSVC Win32 on Windows. Tests that go through Win32 itself stay behind `WIN32`, so the rest runs on Linux.

Each suite is one executable plus a CTest name, e.g.:

```cmake
add_executable(holster_test HolsterTest.cpp ../src/vr/Quaternion.cpp)
add_test(NAME holster COMMAND holster_test)
```

A few Python tests (`tests/*Test.py`, for tools such as the harness runner and the performance scripts) are registered as CTest commands too.

Test files use a tiny local `Check(condition, what)` that prints `ok`/`FAIL` per check and returns non-zero from `main` on any failure — no test framework.

## Running

Linux:

```sh
cmake -S tests -B build-linux-tests -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=-m32
cmake --build build-linux-tests
ctest --test-dir build-linux-tests --output-on-failure
# one suite:
cmake --build build-linux-tests --target holster_test && build-linux-tests/holster_test
```

`tools/build-linux.sh --test` builds and runs all tests before building and deploying the DLL; see [Build, deploy and release](../workflows/build-and-deploy.md).

Windows: `cmake -B build-tests tests -A Win32`, then build and `ctest -C Release`.

**CI** (`.github/workflows/build.yml`) builds the DLL and the test project 32-bit on Windows and runs `ctest --output-on-failure` on `windows-latest` for pushes to `main`, `v*` tags and pull requests.
