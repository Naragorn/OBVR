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
- **Open bug (2026-09-29): every run leaves an `obse_loader` process
  behind.** After about seven runs in a row the next one fails with "The
  game ended before the script started". Ending the leftovers
  (`Stop-Process` on the loaders started by the runs) lets it start again.
  The cause is not looked into.
- **Open bug (2026-09-29): more than two console lines leave OBVR in a
  menu.** With four `console-at` or six `console` lines the console is
  closed on the picture, but every later state line reads "menu 1" (top
  menu unnamed), so nothing that waits for the world runs. Two lines work.
  Not looked into; scenarios keep to two.

## The script

The format is documented at the top of `src/test/HandScript.h`. In short:
- `wait`
- `right|left pos|rot|trigger|grip|stick|a|b|click|trackpad|present|curl`
- `head yaw|pitch|pos|real`
- `mark <name>`
- `dump`
- `log`
- `key <k> <0|1>` (a keyboard key down or up), `action <name>` (one of OBVR's actions as its settings row fires it: `holster_fit`)
- for the runner: `expect`, `reject`, `count <n> <text>` (exactly n times), `ini`, `console`, `console-at <mark> <line>` (typed when that mark is reached; `{near}` becomes the form ID of the item nearest the eyes)
- for the runner, the game behind another window: `background-at <mark>` (a window of another process is put in front at that mark and kept there; the runner stops bringing the game forward and fails the run if the game is in front at a later mark), `moved|still <a> <b> <units>` (the player walked at least / at most this far across the ground between two marks), `turned <a> <b> <degrees>` (and turned at least this much) - measured from the marks' `HandScript: player at` lines and listed under "Measured" in `summary.md`. Background scenarios need OblivionAlwaysActive installed, or the game pauses behind the window.

Rules worth knowing:
- **The head is synthetic by default.** It stands 1.6 m up and looks ahead.
  A sleeping headset gives no valid pose ("head 0" in the state line,
  2026-09-27), and without a head the hands are not placed.
- **Hands are placed from the head's heading**, in metres, in OpenVR's axes:
  x right, y up, z back.
- **A mark ends the script's step.** The picture at a mark shows the state
  the commands before it made. At every mark OBVR writes a `HandScript: state`
  line with the mode, the hands, the camera and the teleport, and a
  `HandScript: items` line: the item near a hand, the item nearest the eyes
  (its form ID and where it is), and what the grab holds.
- **Items placed with `placeatme` can fall through the floor.** The first
  stow run placed a sword 60 units ahead and found it 1900 units below the
  floor. `player.additem` then `player.drop` puts it at the player's feet.
- **Give the runner time for `console` lines.** It types them once the script
  has started, so the script's first `wait` must leave time for that (the
  quick menu scenario waits 9 s).
- **Console text is typed by US scan code.** The game's console reads scan
  codes, not the keyboard layout: typed by this layout's keys, "player" came
  out as "plazer" on a German keyboard.

- **A setting changed during a run goes to `OBVR-test.ini`.** `SaveSetting`
  writes there while the file exists, so the weapon places' fit leaves the
  player's `OBVR.ini` as it was. `holster-fit.txt` checks this; the file's
  hash was the same before and after the run.
- **The harness runs slowly at times.** With the headset asleep a frame can
  take the full 0.25 s the hand clock allows (the teleport's commit frame on
  2026-09-27). A glide can then be over within one frame. Logic that has to
  last must keep its own time rather than rely on seeing several frames: the
  teleport's noise keeps at least 0.5 s for this reason.
- **The laser needs aiming at a menu.** `inventory-drop.txt` points the right
  hand at the first rows with `right rot 62 29 0`. The game's active menu
  follows the cursor, and a cursor on the HUD leaves it "HudMain". The state
  line gives the laser's hit and the cursor, to aim by.

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
| `tools/hand-scripts/fist.txt` | with nothing in the weapon slot, a scripted fist raises the fists, an open hand lowers them | PASS 2026-09-27: "Fist: the weapon hand closed - fists up", then "opened - fists down", weapon state 1 then 2 then 1. FAILS since the tester's save of 2026-09-29 (the runner continues the latest save; "the weapon hand closed - nothing to ready"): checked against a build without that day's changes, it fails the same way |
| `tools/hand-scripts/fist-armed.txt` | sword drawn: a fist does nothing; sword sheathed: a fist takes it off and raises the fists | PASS 2026-09-27 |
| `tools/hand-scripts/holster-fit.txt` | the weapon places' fit takes all three places, saves them to the run's INI only, and a reach to the new place draws | PASS 2026-09-27: "Holster fit: done - one-handed at 0.25 -0.10 -0.55, two-handed at 0.20 -0.10 -0.10, bow at -0.10 -0.20 0.05"; OBVR.ini's hash unchanged |
| `tools/hand-scripts/teleport-noise.txt` | with TeleportMakesNoise=1 the detection reads walk-forward while the teleport moves, and the player does not walk on after the landing | PASS 2026-09-28: 0.0 units across the ground a second after the landing (21.7 with the old setter hook); whether an NPC then hears it: not tested; 2026-09-29 also "the landing sound dodge (000CBA79) played" (TeleportSound, default dodge) |
| `tools/hand-scripts/teleport-sound-landing.txt` | with TeleportMakesNoise=1 and TeleportSound=landing the landing plays the game's own landing sound (0x006B1900 with the controller's ground material) | PASS 2026-09-29: "the landing sound landing (00000000) played"; heard: not checked (the harness listens to nothing) |
| `tools/hand-scripts/walk-direction.txt` | WalkDirection=head: the head turned 90 degrees, the left stick straight ahead walks where the head looks and the view holds still | PASS 2026-09-28: heading 0 to 270 degrees, the feet moved 161 units along it, the compass reads W before and after |
| `tools/hand-scripts/walk-background.txt` | the left stick walks the player in front, then again with another window in front (BackgroundInput=1, OblivionAlwaysActive installed) | PASS 2026-10-09: 182.6 units in front, 184.1 behind, 0.0 while switching; the poll took the controllers' state |
| `tools/hand-scripts/walk-background-off.txt` | the control: BackgroundInput=0 - the stick walks in front and does nothing behind another window | PASS 2026-10-09: 182.7 units in front, 0.0 behind, no poll line |
| `tools/hand-scripts/turn-background.txt` | smooth turning (SnapTurning=0) by the mouse route, in front and behind another window | PASS 2026-10-09: 122.4 degrees in front, 123.7 behind - the injected movement is the size mouse_event gave |
| `tools/hand-scripts/stick-diagonal.txt` | a flick up jumps once; diagonal pushes and flicks neither jump nor aim; a hold aims and goes | PASS 2026-09-27: exactly 1 "jump sent", exactly 1 "Teleport: going" |
| `tools/hand-scripts/inventory-drop.txt` | the left A held 1.5 s over an item in the inventory drops one item | PASS 2026-09-27: exactly 1 click; the weight went 99 to 75, the Steel Longsword's 24 (pictures) |
| `tools/hand-scripts/vanilla-quickkeys*.txt` | experiments: holding a number key opens no ring, in the world or the inventory; the world use equips at once | ran 2026-09-27, see controls-spec 4.4 |
| `tools/hand-scripts/holster-two-handed.txt` | over the right shoulder the claymore is drawn and sheathed; the hip brings the sword; with the sword drawn the shoulder does nothing; then the claymore again | PASS 2026-09-27 |
| `tools/hand-scripts/holster-staff.txt` | the same with a staff (Staff of Burden 000912BE) | PASS 2026-09-27 |
| `tools/hand-scripts/quick-menu-assign.txt` | in the inventory the ring sets slot 5 to the item under the laser; in the world slot 5 then equips it | PASS 2026-09-27: "the hotkeys now: ... 5 Steel Longsword", exactly 1 click, "Steel Longsword equipped." on screen |
| `tools/hand-scripts/vanilla-quickkeys-assign.txt` | experiment: a number key held and a click on an item sets the hotkey | ran 2026-09-27: slot 5 read "Steel Longsword" |
| `tools/hand-scripts/stow.txt` | a dropped sword grabbed, brought to the chest and let go goes into the inventory | PASS 2026-09-27: "taken (no owner ...)", the inventory lists it. FAILS since 2026-09-27 18:44: the save stands the player elsewhere and the hand no longer reaches the sword - new positions needed |
| `tools/hand-scripts/hand-bodies.txt` | the hands and a drawn sword as keyframed Havok bodies: made, in the player's world, driven, out in a menu, a dropped sword pushed by a hand body alone (PushWorld off), the bodies following into the exterior | PASS 2026-09-28: the sword from 252,-1585 to -162,-1581 |
| `tools/hand-scripts/hand-measure.txt` | the hand and weapon bodies against what is drawn: finger bones and the drawn weapon node, `Measure:` lines; the push-people layer switching with a fist | PASS 2026-09-29: the finger joints 4.4-7.5 units behind the grip, the capsule to 11.5 ahead; the drawn blade 11 degrees off the capsule, its far end 14 units aside; "no longer pushes people now (layer 23)" with the fist |
| `tools/hand-scripts/hand-shapes.txt` | small objects placed beside the player: the mesh's world box against the Havok shape's (getAabb) | PASS 2026-09-29: a cup, Havok 1.2-2.7 units beyond the mesh per side (a goblet 1.1-3.0, a tankard 1.7-5.0, the longsword 0.0-0.8, from copies of it with other items) |
| `tools/hand-scripts/stow-owned.txt` | the same sword owned by Baurus first: taken as stolen | PASS 2026-09-27: "taken (owner 00023F2A ...)", the red hand on it in the inventory |
| `tools/hand-scripts/take-only-by-hand.txt` | TakeOnlyByHand=1: A at the sword is kept from the game, the sword stays; stowing still takes it | PASS 2026-09-27. FAILS since the save change of 2026-09-27 18:44, like `stow`: checked 2026-09-29 against a build of 423b77a, it fails there the same way - new hand positions needed |
| `tools/hand-scripts/activate-takes.txt` | the option off: the same A takes the sword | PASS 2026-09-27 |
