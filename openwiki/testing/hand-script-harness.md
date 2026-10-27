---
type: testing
title: Hand-script harness
description: How Full VR features are tested in the real game with nobody in the headset - controllers and head played from a text scenario, OBVR-test.ini overlaid with a run token, log expectations checked, pictures and logs collected into a PASS/FAIL run folder.
tags: [testing, harness, full-vr, scenarios, linux, proton]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-27T21:08:40.633Z
sources:
  - id: openwiki-source-50cc916e831201e321516af4
    resource: repo://docs/hand-script-harness.md
  - id: openwiki-source-e0e4098db498b328d6123df6
    resource: repo://src/test/HandScript.h
  - id: openwiki-source-dca3dd6e669100d52b9083b4
    resource: repo://tools/hand-script-run.py
generated: { by: "claude-code", at: "2026-10-27T21:08:40.633Z" }
---

# Hand-script harness

The harness runs Full VR in the actual game with nobody wearing the headset: the head and both controllers are played from a text scenario, OBVR writes state lines into its log, and a runner checks the log against the scenario's expectations and collects pictures. It complements the [unit tests](unit-tests.md), which cover the pure decisions without the game. The full reference, including open bugs of the harness itself, is `docs/hand-script-harness.md`.

## Pieces

| Piece | Where | Role |
| --- | --- | --- |
| Scenario | `tools/hand-scripts/*.txt` (about 300) | one command per line, `#` comments |
| Parser and script clock | `src/test/HandScript.h` (pure, `hand_script_test`) | parses the script, walks it by time, yields the controller state each step leaves |
| In-game runtime | `src/test/HandScriptRuntime.cpp`, `HandScriptConsole.h`, `HandScriptPose.h` | replaces the controllers and head with the script's, writes `HandScript:` state lines at marks, hands console lines to the game |
| Runner (Linux/Proton) | `tools/hand-script-run.py` → `tools/hand_script_runner.py` | starts the game, overlays the INI, waits, checks, collects |
| Runner (Windows) | `tools/hand-script-run.ps1` | same on Windows, types console lines by scan code |

## Running

```sh
python3 tools/hand-script-run.py tools/hand-scripts/blade-wall.txt --headless
```

Needs Steam and SteamVR running (the headset may sleep) and Oblivion not running. `--headless` starts the game inside gamescope's headless backend (nothing on the desktop, no input taken) and is the preferred mode; `--direct` starts it past Steam and its launch options, with the given environment on top. The Linux runner refuses a start through Steam, because the run token only travels with a direct or headless start. Other options: `--save` (default `OBVRHarness`), `--run-timeout`, `--keep-game-open`, `--env KEY=VALUE`. A run takes about a minute; the runner exits non-zero on FAIL.

Results go to `artifacts/hand-script/<scenario>/<yyyyMMdd-HHmmss>/`: `summary.md` (PASS/FAIL, marks, every problem), a game picture per mark, optional headset/eye dumps, and the run's `OBVR.log`. The player's own `OBVR.log` and `OBVR.log.prev` are put back afterwards. `artifacts/` is untracked and can grow very large.

## What a run does to the installation

- It **never touches `OBVR.ini`.** It writes `OBVR-test.ini` beside it (`[Debug] HandScript`, the scenario's `ini` lines, a `HarnessToken`), which OBVR reads over `OBVR.ini` only when the token matches the `OBVR_HARNESS` variable in the game's environment, and deletes it afterwards. See [Configuration](../operations/configuration.md#the-test-overlay-obvr-testini).
- On Linux it types nothing: loading the save and console lines are handed to OBVR through the overlay (`[Debug] HandScriptLoad`, `HandScriptConsole`) and run through xOBSE's console interface.
- It continues the harness save and never saves.

## Scenario language (short)

From the header of `src/test/HandScript.h`:

- `wait <s>` — let the state stand.
- `right|left pos <x> <y> <z>` (metres from the head, OpenVR axes x right, y up, z back, in the head's heading) / `rot <pitch> <yaw> <roll>` / `trigger|grip <0..1>` / `stick <x> <y>` / `a|b|click|trackpad <0|1>` / `present` / `curl` / `thumb`.
- `head yaw|pitch|pos|real|face` — the head is **synthetic by default**, 1.6 m up looking ahead, because a sleeping headset gives no valid pose.
- `mark <name>` — ends a step; OBVR writes `HandScript: state`, `items` and `actors` lines there, and the runner takes a picture.
- `dump` — SteamVR's composited frame and the submitted eyes.
- Runner lines: `ini <Section> <Key=Value>`, `console`, `console-at <mark> <line>` (`{near}`, `{actor}` placeholders), `expect <text>`, `reject <text>`, `count <n> <text>`, `moved|still|turned` between marks.

A typical scenario (`tools/hand-scripts/blade-wall.txt`) sets `ini Hands ...` keys, gives the player a weapon with `console-at ready player.additem ...`, lists `expect Contact: the blade met body` style lines and `reject HandScript: WARNING`, then moves the right hand step by step with `wait` and `mark`.

## Rules worth knowing

- The picture at a mark shows what the commands before it did; the log lines are the evidence, the pictures are for people.
- Frames can be slow with the headset asleep (up to the 0.25 s the hand clock allows), so logic that must last keeps its own time.
- The console interface's return value is not success; what a console line did is read off the world (state and actor lines).
- Runs share one game install and one SteamVR: run them one after another, not in parallel.
