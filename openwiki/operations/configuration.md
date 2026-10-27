---
type: operations
title: Configuration (OBVR.ini)
description: Where OBVR finds OBVR.ini, how it is read at load and hot-reloaded only when the file changed, how settings written from the headset menu reach the file, and how the harness overlays OBVR-test.ini without touching the player's file.
tags: [configuration, ini, hot-reload, settings, harness]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-27T21:08:40.633Z
sources:
  - id: openwiki-source-2b10fb6468f3695dacf90613
    resource: repo://OBVR.ini
  - id: openwiki-source-a66fb70ef9c23adff057a8e2
    resource: repo://src/core/Config.cpp
  - id: openwiki-source-f93e9647d8c17c3de3520aad
    resource: repo://src/core/Config.h
  - id: openwiki-source-a7f9cdef1604a1b055c2fdda
    resource: repo://src/core/ReloadGate.h
generated: { by: "claude-code", at: "2026-10-27T21:08:40.633Z" }
---

# Configuration (OBVR.ini)

All of OBVR's settings live in one INI file, `OBVR.ini`, read into the `obvr::Config` struct (`src/core/Config.h`) and accessed everywhere through `GetConfig()`. The shipped file in the repository root is also the documentation: every key carries a comment block explaining what it does and why its default is what it is.

## Sections

The shipped `OBVR.ini` has these sections: `[Camera]`, `[Head]`, `[Look]`, `[Render]`, `[Hands]`, `[Locomotion]`, `[HandHud]`, `[Body]`, `[Onboarding]`, `[Updates]`, `[Debug]`, `[Performance]`, `[SettingsMenu]`. `[Hands]` is the largest: the Full VR toggle (`Enabled`) and everything about hands, weapons and contact. `[Debug]` holds diagnostics and traces, which stay off by default because diagnostics that run every frame have a measurable cost.

## Where the file is found

`BuildPath` in `src/core/Config.cpp` looks **next to `OBVR.dll` first** (`Data/OBSE/Plugins/`, via `platform::BuildPluginPath`) and falls back to the game root. This keeps OBVR an ordinary Mod Organizer 2 mod: MO2 virtualises only `Data`, and an INI in the game root would need Root Builder. An absolute path is always built, because `GetPrivateProfileString` resolves relative names against the Windows directory.

`Config::Load` checks separately that the file exists — a missing file would otherwise read as all defaults and OBVR would silently do nothing — and logs the decision.

## Reading values

Values are read with `GetPrivateProfileStringA` through small typed readers (`ReadBool`, `ReadFloat`, `ReadUInt`, enum readers such as `ReadStereoMode`). An unknown enum value keeps the previous setting and is logged (`Config: unknown <section>.<key> ...`).

## Hot reload

`[Debug] ReloadEveryFrames` (120 in the shipped file, 0 = off) sets how often `Config::Reload` is asked. A full re-read is a few hundred `GetPrivateProfileString` calls over a file of over 100 KB and was measured at about 75 ms on the frame's own thread, so the decision is gated (`src/core/ReloadGate.h`, pure):

- the file stamps (write time and size of `OBVR.ini` and `OBVR-test.ini`) are compared with what was last read; only a change triggers the read;
- `Load` remembers the stamps so the first reload reads nothing;
- after OBVR wrote a setting itself, `RememberOwnWrite` records the new stamps so its own write does not cause a re-read — but only if nobody else changed the files in between.

A re-read logs `Config: OBVR.ini changed - re-read in N ms`.

## Writing settings

`SaveSetting(section, key, value)` writes one value into the same file `Load` reads (found the same way), so a setting changed in the headset menu is not overwritten by the next reload. It returns false for a read-only file or a mod manager VFS that refuses writes; callers report that once. The settings menu reports changed rows and the caller writes them; see [Menus, HUD and settings UI](../architecture/menus-and-ui.md).

## The test overlay (OBVR-test.ini)

The harness runners (`tools/hand-script-run.py`, `.ps1`) write `OBVR-test.ini` next to the DLL. It is read **over** `OBVR.ini` (only the keys it has change anything) and receives `SaveSetting` writes during a run, so a run never changes the player's file. It only counts when its `[Debug] HarnessToken` equals the `OBVR_HARNESS` environment variable the runner put into the game's environment (`core/HarnessToken.h`); a game started through Steam carries none, so a file left behind by an aborted run has no effect. See [Hand-script harness](../testing/hand-script-harness.md).

## Tests

`tests/ReloadGateTest.cpp`, `IniSettingsTest.cpp` and `HarnessTokenTest.cpp` cover the decisions above without a file system.
