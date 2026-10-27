---
type: quickstart
title: OBVR quickstart
description: Entry point to the OBVR wiki - what OBVR is, how the repository is laid out, the rules that apply to changes, and which page answers which task.
tags: [quickstart, overview, navigation]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-27T21:08:40.633Z
sources:
  - id: openwiki-source-8037e2358a2c4f9b2c722a11
    resource: repo://AGENTS.md
  - id: openwiki-source-23775c3de52f3ab95a13cb8b
    resource: repo://README.md
  - id: openwiki-source-0211f2f5f95441e40340330f
    resource: repo://src/Plugin.cpp
generated: { by: "claude-code", at: "2026-10-27T21:08:40.633Z" }
---

# OBVR quickstart

OBVR puts the original **The Elder Scrolls IV: Oblivion** (2006, version 1.2.0.416, not the Remastered edition) into VR. It is a 32-bit xOBSE plugin DLL that hooks the engine in place: the head pose drives the game's own camera, the world is rendered once per eye, the HUD and menus are captured into overlays, and everything is handed to SteamVR through OpenVR. It has a seated mode (headset plus mouse/keyboard/gamepad) and a **Full VR** mode with motion controllers, weapons in hand, holsters, teleport and physical combat. The public README is `README.md`; the licence is GPL-3.0.

## Repository map

| Path | What is there |
| --- | --- |
| `src/` | the DLL; `src/Plugin.cpp` is the xOBSE entry point |
| `tests/` | the native 32-bit unit-test project (C++ suites plus a few Python tests) |
| `tools/` | build/deploy (`build-linux.sh`), release packaging, the hand-script harness runners and scenarios (`hand-scripts/`), analysis scripts |
| `docs/` | design specs (`*-spec.md`, each with open bugs), the harness reference, and `docs/vr-modding/`, a dated knowledge base on VR modding |
| `assets/` | menus, textures, the SteamVR input manifest, sounds shipped with releases |
| `third_party/` | vendored files such as the 32-bit `openvr_api.dll` |
| `OBVR.ini` | the shipped configuration, documented key by key |
| `HANDOFF.md` | a chronological log, not the current state |
| `AGENTS.md` | rules for changes in this repository |

## Rules for changes (from AGENTS.md)

- Code, comments, docs and commit messages are in English.
- Prefer root-cause fixes; state assumptions; do not claim coverage that was not run.
- Exercise every meaningful flow in tests, extracting decisions into testable units when needed.
- Engine findings that matter to the OBNG modernisation project go to `../OBNG/knowledge/`, outside this repo.

## Where to go

| Task | Page |
| --- | --- |
| Understand how the plugin loads, what it patches, how a frame flows | [Architecture overview](architecture/overview.md) |
| Stereo rendering, DXVK interop, submitting to SteamVR | [Rendering and stereo](architecture/rendering-and-stereo.md) |
| HUD, menus, OBVR's settings menu, HUD on the hands | [Menus, HUD and settings UI](architecture/menus-and-ui.md) |
| Motion controllers, gestures, holsters, game input | [Full VR hands and controller input](architecture/full-vr-hands.md) |
| Blades and hands stopping on walls and bodies, fitted capsules | [Weapon and body contact](architecture/weapon-and-body-contact.md) |
| Settings, hot reload, the test overlay | [Configuration (OBVR.ini)](operations/configuration.md) |
| Build on Windows or Linux, deploy, CI, release | [Build, deploy and release](workflows/build-and-deploy.md) |
| Write or run unit tests | [Unit tests and logic units](testing/unit-tests.md) |
| Test a feature in the real game without a headset | [Hand-script harness](testing/hand-script-harness.md) |

## Fast facts

- Every engine address is in `src/game/GameAddresses.h` and valid only for 1.2.0.416; every patch verifies the original bytes first and stays off on a mismatch.
- `src/camera/CameraHook.cpp` is the large glue file; decisions live in pure `*Logic.h` and `src/vr/` headers with unit tests.
- Build on Linux and deploy to both game installs: `tools/build-linux.sh --test`.
- Run a scenario: `python3 tools/hand-script-run.py tools/hand-scripts/<name>.txt --headless`.
