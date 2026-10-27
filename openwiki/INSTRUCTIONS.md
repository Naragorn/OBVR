# OpenWiki brief

OBVR puts The Elder Scrolls IV: Oblivion (2006, 1.2.416) into VR: an OBSE plugin DLL for the 32-bit game that hooks the engine, renders through OpenVR and drives a seated mode and a full VR mode with motion controllers (hands, weapons, combat, menus on the hands).

## Scope and priorities

- The architecture of `src/` by area (`camera`, `core`, `game`, `obse`, `perf`, `platform`, `render`, `test`, `ui`, `vr`, `Plugin.cpp`): what each area owns, where it hooks the engine, and how a frame flows through it.
- The pattern that keeps decisions testable: pure `*Logic.h` units over plain values, tested in `tests/` without the game.
- Building, deploying and testing: CMake for Windows and Linux, `tools/build-linux.sh`, the unit tests in `tests/`, and the hand-script harness (`tools/hand-script-run.py`, `tools/hand-scripts/`, `docs/hand-script-harness.md`).
- Configuration: the sections and keys of `OBVR.ini` and where each is read.

## How to treat the existing docs

- `docs/*-spec.md` are design specs and decision records, often with open-bug lists. Link to them for the why instead of restating them; where a spec and the source disagree, the source and tests are authoritative.
- `HANDOFF.md` is a chronological log, not the current state.
- `docs/vr-modding/` is a knowledge base with status labels per file.

## Out of scope

Harness run output, build directories, and reverse-engineering notes kept outside this repository.
