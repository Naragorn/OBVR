# The hand script harness

Full VR runs in the game with nobody in the headset. The controllers, and the
head, are played from a text file. The runner then:
- starts the game,
- takes pictures at the script's marks, and
- checks the log.

Built 2026-09-27 so features could be implemented and tested while the tester
was away.

## Running it

```
powershell -ExecutionPolicy Bypass -File tools\hand-script-run.ps1 -Script tools\hand-scripts\teleport.txt
```

Needs:
- SteamVR running with the headset connected. It may sleep, and the
  controllers may be off. The runner starts SteamVR when it is not running.
- The desktop unlocked.
- Oblivion not running.

A run takes about a minute. Its results go to
`artifacts\hand-script\<scenario>\<yyyyMMdd-HHmmss>\`:

| File | What it is |
|---|---|
| `summary.md` | PASS or FAIL, the marks, every problem |
| `NN-<mark>-game.png` | the game window at each mark (the monitor's view of the world, HUD included) |
| `OBVR-QuickMenu-<mark>.png` | the quick menu's ring as OBVR painted it, at each mark it was up for |
| `dump-*-headset.png` | SteamVR's composited frame (`dump` in the script), the right eye, brightened and turned upright |
| `dump-*-eyes.png` | the two eyes as OBVR submitted them |
| `OBVR.log` | the run's log |
| `player-OBVR.log`, `player-OBVR.log.prev` | the player's logs as they were before the run |

The runner exits 1 on FAIL.

## What the runner does to the installation

- **It never touches `OBVR.ini`.**
  - It writes `OBVR-test.ini` beside it: `[Debug] HandScript` plus the
    scenario's `ini` lines.
  - OBVR reads that file over `OBVR.ini`; only the keys it has change
    anything.
  - The log says `Config: TEST OVERLAY` when the file is read.
  - The runner deletes the file when the run ends. A file left behind by a
    run that was cut short shows up as that log line.
- **It puts `OBVR.log` and `OBVR.log.prev` back.** The player's last
  session stays readable; the run's own log is kept in the run folder.
- **It continues the latest save and never saves.**
  - `console` lines (e.g. `player.additem`) change only the running game.
  - Nothing is written back to the save.
- **It deletes SteamVR's full-size dumps** from `SteamVR\screenshots` after
  bringing them over small.

## The script

The format is documented at the top of `src/test/HandScript.h`. In short:
- `wait`
- `right|left pos|rot|trigger|grip|stick|a|b|click|trackpad|present|curl`
- `head yaw|pitch|pos|real`
- `mark <name>`
- `dump`
- `log`
- for the runner: `expect`, `reject`, `ini`, `console`

Rules worth knowing:
- **The head is synthetic by default.** It stands 1.6 m up and looks ahead.
  A sleeping headset gives no valid pose ("head 0" in the state line,
  2026-09-27), and without a head the hands are not placed.
- **Hands are placed from the head's heading**, in metres, in OpenVR's axes:
  x right, y up, z back.
- **A mark ends the script's step.** The picture at a mark shows the state
  the commands before it made. At every mark OBVR writes a `HandScript: state`
  line with the mode, the hands, the camera and the teleport.
- **Give the runner time for `console` lines.** It types them once the script
  has started, so the script's first `wait` must leave time for that (the
  quick menu scenario waits 9 s).
- **Console text is typed by US scan code.** The game's console reads scan
  codes, not the keyboard layout: typed by this layout's keys, "player" came
  out as "plazer" on a German keyboard.

## What the pictures cannot show

- **OBVR's overlays are not in the game window.** The teleport arc, the
  quick menu and the settings panel are SteamVR overlays. The game window is
  the world as the monitor gets it. The quick menu is therefore also saved as
  painted.
- **SteamVR's dump is seen from where the real headset lies.**
  - The synthetic head is reprojected to the real headset's orientation.
  - Overlays placed from the synthetic head land away from it.
  - On the desk on 2026-09-27 the headset lay rolled a quarter turn;
    `-DumpRotate` (default 270) turns the picture back.
  - The dump is for the headset's picture of the world, not for overlays.
- **`ShowMirrorWindow` opened no window** that the runner could find
  (2026-09-27). The `mirror` command is kept, but it is not relied on.

## Scenarios

| Scenario | What it proves | Last run |
|---|---|---|
| `tools/hand-scripts/teleport.txt` | holding the right stick aims (arc shown), releasing goes, the player arrives | PASS 2026-09-27: "going - 2.08 m away", "arrived, 0.0 units from the target" |
| `tools/hand-scripts/quick-menu.txt` | the right trackpad opens the ring with the save's hotkeys; the hand chooses slot 1, then slot 3; letting go taps key 3 and the game equips the item | PASS 2026-09-27: "3 Iron Longsword" read from the engine, "hotkey 3 used", the game's own "Iron Longsword equipped." on screen |
| `tools/hand-scripts/holster.txt` | with the bow in hand, a grip at the left hip equips and draws the sword; the same reach sheathes it; a grip behind the left shoulder equips and draws the bow | PASS 2026-09-27: states bow sheathed, then sword drawn, then sword sheathed, then bow drawn (the `HandScript: state` lines) |
