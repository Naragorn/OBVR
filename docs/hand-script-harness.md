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

### On Linux, under Proton (since 2026-10-14)

```
python3 tools/hand-script-run.py tools/hand-scripts/teleport.txt
```

Needs Steam and SteamVR running (the headset may sleep) and Oblivion not
running; it starts the game through Steam (`steam://rungameid/22330`), with
the launch options Steam keeps for the game. The results go to the same
`artifacts/hand-script/<scenario>/<time>/` with the same `summary.md`.

It types nothing into the game. On Windows the runner presses Continue in the
main menu and types the `console` lines by scan code; under Proton that
depends on window focus and the desktop's input tools (`HANDOFF.md` section
12), so the Linux runner hands both to OBVR through `OBVR-test.ini`
(`src/test/HandScriptConsole.h`):
- `console-menu <command>` lines go to the main menu's console before the
  load, four a frame (the console's queue), the load once all are handed:
  for what has to stand before the player's model is built, a race's scale
  for one (`body-ik-tall.txt`, 2026-10-16).
- `[Debug] HandScriptLoad=OBVRHarness`: OBVR asks the main menu's console for
  `load OBVRHarness` once the main menu has stood two seconds ("HandScript:
  asked the main menu's console for ..."), and again after ten seconds if it
  still stands, three times at most.
- `[Debug] HandScriptConsole=1`: OBVR hands the script's `console` lines to
  the game four seconds of game time after the script has started (handed
  over in the first frames after the load, they ran before the world had
  settled; the Windows runner's typing took about that long), and its
  `console-at` lines once their mark is written (two frames later, for
  `{near}`), one line a frame, through xOBSE's console interface
  ("HandScript: console ... handed to the game").
  No console window opens, so the two open bugs above about console lines do
  not arise, and the lines run at once instead of over seconds of typing.

The rest as on Windows, with these differences:
- **Pictures** of the game's window come from its X11 window (Wine's, through
  XWayland) with ImageMagick's `import`. SteamVR on Linux wrote no files for
  `dump` (2026-10-14: three requests in `stereo-view`, no picture anywhere
  under the home folder or `/tmp`, nothing in the compositor's log); the
  summary says so under "Notes".
- **The background part** puts a small Tk window in front with `xdotool`.
- **The `-Save autosave` scenarios** (`load-only`, `race-menu`,
  `race-menu-handsoff`) run with `--save`; the Linux prefix's autosave needs
  four content mods that are not installed there.
- **`make-harness-save.txt`** is not for this runner: it would write the
  harness save from whatever the main menu continues.
- **Measurements.** The summary's "Measured" carries SteamVR's frame summary
  for the run (presented, dropped and reprojected frames, which the compositor
  writes to `vrcompositor.txt` when the game leaves it). An OBVR profiler
  capture written during the run (`[Performance]`) is moved into the run
  folder and analysed with `tools/analyze-performance.py`
  (`OBVR-performance-N-report/report.md`). `perf-walk.txt` is the scenario for
  that.
- **`--direct`** starts the game past Steam: through the Steam Linux Runtime
  and Proton (`proton waitforexitandrun OblivionLauncher.exe`, from the game
  folder), with MoreHeap's DLL override and without `PROTON_LOG`. `--env
  KEY=VALUE`, repeatable, adds to its environment, for example `--env
  "DXVK_CONFIG=d3d9.presentInterval = 0"`; without `--direct` the runner
  refuses it, since a start through Steam takes Steam's launch options. The
  start line goes to `launch.txt`, Proton's output to `proton.log`, both in the
  run folder.

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
- **Open bug (2026-10-09): `console` lines at the start land in the game.**
  Typed once the script has started, the keys reached the world, not the
  console: the picture showed "You cannot wait while in the air" (T, in
  "additem", is Wait), and the items were never added - `holster.txt` failed
  on it the same day ("draw given up") - before the blade's contact step,
  which needs a drawn weapon, could act. `console-at <mark>` lines typed
  after a `wait 10` and a mark work (`reach-open.txt`, `blade-wall.txt`);
  typing them takes over 8 s, so
  a step that needs the console closed waits about 16 s after that mark
  (the third `blade-wall` run's holster grip came while the console was
  still up and did nothing). Why the start's typing goes astray is not
  looked into.

## The script

The format is documented at the top of `src/test/HandScript.h`. In short:
- `wait`
- `right|left pos|rot|trigger|grip|stick|a|b|click|trackpad|present|curl|little` (`little` the little finger's own curl, for a sensor that never reads; negative follows `curl`)
- `head yaw|pitch|pos|real|face` - `face 1` turns the yaw each frame to the nearest actor in combat with the player (a fighter circling the scripted player stays before the head), a `yaw` ends it; `pos` is the eyes with the head level; pitched or turned, the eyes swing about the neck 8 cm behind and 7.5 cm below them (the Oculus neck model, `ScriptHeadPosition`), as a real headset moves, and the hands stay placed from the level eyes
- `mark <name>`
- `dump`
- `log`
- `key <k> <0|1>` (a keyboard key down or up), `action <name>` (one of OBVR's actions as its settings row fires it: `holster_fit`)
- for OBVR's native windows (the first start's pages, the controls card): `action menu-primary`, `action menu-secondary`, `action menu-reset` press the window's buttons as a click would (NativeMenuPrototype.cpp reads them in place of MenuQue's poll)
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
- **The game window does not follow the synthetic head's pitch**
  (`head-pitch-probe.txt`, 2026-10-11). With the head pitched and held six
  seconds before each mark, the "Camera: at the mark" line read exactly the
  pitch scripted (-60, 0, 40, -85) while the window showed the hall level
  at -60, the ceiling at 0, the floor at 40 and the hall again at -85 - the
  monitor's picture is not the headset's. The eye dumps (`dump` in the
  script, `dump-*-eyes.png`) show the eyes as OBVR submitted them and agree
  with the camera's line; use them for anything that depends on where the
  head looks.
- **Smoothness is measured, not seen.** A body that moves a few units from
  one frame to the next is a stutter in the headset and nothing in a
  picture. `ini Debug BodyTrace=4000` writes a "Body trace:" line a frame
  for the shown body (the body against the headset, the head bone's
  animated offset, the let-down, the torso's yaw, each arm's reach, bend
  and elbow direction, how many bone writes survived the draw), and
  `tools\body-trace.ps1 -Log <run>\OBVR.log [-Jumps]` prints, per window
  between the script's marks, the largest frame-to-frame change of each
  and counts the jumps. The first run (2026-10-12, the build of 835f15a):
  with the hands held still 0.014 units and 0.01 degrees a frame - the
  engine never rewrote a bone between the write and the draw ("held
  16/16") - but 0.3 units a frame while walking (the animation's head bob
  carried the body), 1.45 as the walk began (its lean) and 44 the frame
  the sneak animation began: what the tester felt as "ruckelt und zuckelt
  ... alles auf einmal" (docs/player-body-spec.md, round 2).
- **Whether a look sees into the body is measured, not judged by eye.**
  `ini Debug BodyView=1` casts each eye's rays at the shown body at every
  mark (`game/BodyView.h`, the arithmetic in `BodyViewLogic.h`): the
  body's NiTriShapes as the draw is about to use them - their vertices
  skinned on the CPU the way the draw skins them, each triangle's outside
  told by the shape's own normals - against the camera's frustum, the near
  plane included. Each ray is classified by the nearest triangle it meets:
  the trunk (its bone the pelvis, the spine, the neck, the head or a
  clavicle) or a limb, from outside or from inside; a shape with a
  NiStencilProperty is taken as drawn from both sides, its back a surface.
  The lines: "Body view: shape ..." once per skeleton (vertices,
  triangles, every vertex weighing one, the winding), "Body view: at
  "<mark>", the left/right eye ..." with the shares, and a verdict - "down
  the body at", "into the body at" (a ray of either eye on the trunk's
  inside) or "the trunk nowhere in view at" (under half a percent of both
  views) - with how far below the view's bottom edge the trunk's inside
  begins (24 rows over 20 degrees below it). The picture
  `OBVR-BodyView-<mark>.png` shows the rays: green the trunk from outside,
  blue a limb, red the trunk's inside, orange a limb's, the rows below the
  view under a grey line. Checked against the game: on 2026-10-12, run
  without the headset (the monitor's view is then the camera's), the
  picture's outline of the body at 85 degrees down matched the game window's
  shot (the shoulders' two humps at 0.42 and 0.60 of the width against
  0.41 and 0.58, the top at 0.34 of the height against 0.36), and the
  view without the step back (no headset, no neck pivot) came out "into the
  body" on 3.1 % of it - the collar, as round 2's dumps showed. Two engine
  facts found on the way: NiSkinData's per-bone weight count is a UInt16
  followed by two uncleared bytes (xOBSE declares a UInt32; LowerBody read
  0xBEA10081 for 129), and `mem::LooksLikeObjectAddress` wants an aligned
  word, so the end of a block is checked at its last word. About 4 ms a
  mark for 9 shapes and 10 570 triangles.

## Scenarios

| Scenario | What it proves | Last run |
|---|---|---|
| `tools/hand-scripts/teleport.txt` | holding the right stick aims (arc shown), releasing goes, the player arrives | PASS 2026-09-27: "going - 2.08 m away", "arrived, 0.0 units from the target" |
| `tools/hand-scripts/quick-menu.txt` | the right trackpad opens the ring with the save's hotkeys; the hand chooses slot 1, then slot 3; letting go taps key 3 and the game equips the item | PASS 2026-09-27: "3 Iron Longsword" read from the engine, "hotkey 3 used", the game's own "Iron Longsword equipped." on screen. FAILS 2026-10-11 with the body on and off alike (control with `ini Body Visible=0`): the save's eight hotkeys are empty ("1 -, 2 -, ... 8 -"), so slot 3 has nothing to use |
| `tools/hand-scripts/holster.txt` | with the bow in hand, a grip at the left hip equips and draws the sword; the same reach sheathes it; a grip behind the left shoulder equips and draws the bow | PASS 2026-09-27: states bow sheathed, then sword drawn, then sword sheathed, then bow drawn (the `HandScript: state` lines). FAILS 2026-10-11 with the body on and off alike (a control with `ini Body Visible=0`, the same log lines): "the weapon equipped never showed in the hand - draw given up" twice, the bow "none seen this session" - cause not looked into |
| `tools/hand-scripts/fist.txt` | with nothing in the weapon slot, a scripted fist raises the fists, an open hand lowers them | PASS 2026-09-27: "Fist: the weapon hand closed - fists up", then "opened - fists down", weapon state 1 then 2 then 1. FAILS since the tester's save of 2026-09-29 (the runner continues the latest save; "the weapon hand closed - nothing to ready"): checked against a build without that day's changes, it fails the same way |
| `tools/hand-scripts/fist-armed.txt` | sword drawn: a fist does nothing; sword sheathed: a fist takes it off and raises the fists | PASS 2026-09-27. FAILS 2026-10-11 with the body on and off alike (control with `ini Body Visible=0`): the draw by the holster is given up first ("never showed in the hand"), so the fist steps meet another state - cause not looked into |
| `tools/hand-scripts/holster-fit.txt` | the weapon places' fit takes all three places, saves them to the run's INI only, and a reach to the new place draws | PASS 2026-09-27: "Holster fit: done - one-handed at 0.25 -0.10 -0.55, two-handed at 0.20 -0.10 -0.10, bow at -0.10 -0.20 0.05"; OBVR.ini's hash unchanged |
| `tools/hand-scripts/teleport-noise.txt` | with TeleportMakesNoise=1 the detection reads walk-forward while the teleport moves, and the player does not walk on after the landing | PASS 2026-09-28: 0.0 units across the ground a second after the landing (21.7 with the old setter hook); whether an NPC then hears it: not tested; 2026-09-29 also "the landing sound dodge (000CBA79) played" (TeleportSound, default dodge) |
| `tools/hand-scripts/teleport-sound-landing.txt` | with TeleportMakesNoise=1 and TeleportSound=landing the landing plays the game's own landing sound (0x006B1900 with the controller's ground material) | PASS 2026-09-29: "the landing sound landing (00000000) played"; heard: not checked (the harness listens to nothing) |
| `tools/hand-scripts/walk-direction.txt` | WalkDirection=head: the head turned 90 degrees, the left stick straight ahead walks where the head looks and the view holds still | PASS 2026-09-28: heading 0 to 270 degrees, the feet moved 161 units along it, the compass reads W before and after |
| `tools/hand-scripts/walk-background.txt` | the left stick walks the player in front, then again with another window in front (BackgroundInput=1, OblivionAlwaysActive installed) | PASS 2026-10-09: 182.6 units in front, 184.1 behind, 0.0 while switching; the poll took the controllers' state |
| `tools/hand-scripts/walk-background-off.txt` | the control: BackgroundInput=0 - the stick walks in front and does nothing behind another window | PASS 2026-10-09: 182.7 units in front, 0.0 behind, no poll line |
| `tools/hand-scripts/turn-background.txt` | smooth turning (SnapTurning=0) by the mouse route, in front and behind another window | PASS 2026-10-09: 122.4 degrees in front, 123.7 behind - the injected movement is the size mouse_event gave |
| `tools/hand-scripts/stick-diagonal.txt` | a flick up jumps once; diagonal pushes and flicks neither jump nor aim; a hold aims and goes | PASS 2026-09-27: exactly 1 "jump sent", exactly 1 "Teleport: going" |
| `tools/hand-scripts/inventory-drop.txt` | the left A held 1.5 s over an item in the inventory drops one item | PASS 2026-09-27: exactly 1 click; the weight went 99 to 75, the Steel Longsword's 24 (pictures). PASS again 2026-10-13 on the build of fd9ec72 with the game in front: the items line at "held" names a new weapon reference with a dynamic form id 102 units from the eyes - the dropped sword - so the drop the tester lost (4.43) is not lost there |
| `tools/hand-scripts/inventory-drop-background.txt` | the same drop with another window put in front at `inventory_later` (`background-at`), so the left A goes through the engine's own input (game/EngineInput.h) - the tester's way | written 2026-10-13; its one run that morning says nothing about the drop: the runner's console text went into the game as keys (the wait menu 0x3F4 was on top from the start, "active menu none"), the inventory never opened. Not rerun (no game starts that day); the cause of 4.43 was read from the engine instead, and the fix is to be proven by this scenario |
| `tools/hand-scripts/vanilla-quickkeys*.txt` | experiments: holding a number key opens no ring, in the world or the inventory; the world use equips at once | ran 2026-09-27, see controls-spec 4.4 |
| `tools/hand-scripts/holster-two-handed.txt` | over the right shoulder the claymore is drawn and sheathed; the hip brings the sword; with the sword drawn the shoulder does nothing; then the claymore again | PASS 2026-09-27. FAILS 2026-10-11 with the body on and off alike (control with `ini Body Visible=0`): "the two-handed weapon - none seen this session", the one-handed draw given up - cause not looked into |
| `tools/hand-scripts/holster-staff.txt` | the same with a staff (Staff of Burden 000912BE) | PASS 2026-09-27 |
| `tools/hand-scripts/quick-menu-stick.txt` | the right stick's button without a trackpad (controls-spec 4.4, the Oculus Touch's way): a short click opens the ring latched, the hand moved right lights slot 3, the next click uses it; the button held 0.9 s opens the Rest menu (SleepWait), and the right B closes it with the menu's own key; the left stick stays the walk's | PASS 2026-10-15 (Linux, the swapped scheme): "QuickMenu: opened at the hand" once, "hotkey 3 used", SleepWait once, "menu 0" at `rest_closed` |
| `tools/hand-scripts/spell-ring.txt` | the spell ring on the other hand (controls-spec 4.44): Flare added by the console; a short left click opens the spellbook's ring latched, the hand moved up lights slot 1, the next click makes it the active spell (read back through GetActiveMagicItem); the left stick held 0.9 s runs and opens no ring on its release | PASS 2026-10-16 15:20 (Linux; the fourth run - the first three read the spell type at xOBSE's +0x38, which left Flare out and let the power in as a spell; the dump of the words put the type at +0x34): "3 spell(s) known, page 1 of 1: 1 Flare, 2 Heal Minor Wounds, 3 Lover's Kiss", the list line with Flare 000A97DF type 0 cost 9, Lover's Kiss type 2, Heal Minor Wounds cost 24; "slot 1 used (Flare) - the engine took it; the active spell now Flare"; quick-menu-stick.txt PASS right after (the hotkey ring untouched) |
| `tools/hand-scripts/controls-card.txt` | the controls card (ui/NativeControlsCard.h) opened by the script's `action controls-card` in the world - the weapon hand's page - and closed from outside by the right B | PASS 2026-10-15 (Linux, the fourth run): "controls card - Controls: the weapon hand (right)" twice, "the other hand (left)" once, "menu 1 (Generic)" at every mark, "controls card closed". Learned on the way: Escape over a generic menu opens the game's pause menu on top of it (the card stays beneath), so Close is the way out; and a window of OBVR's may not open before the script runs, since the script starts only in the world with no menu up - hence the script's `action` asks instead of INI keys |
| `tools/hand-scripts/first-start.txt` | the first start's chain (ui/FirstStart.h), begun in the world by the script's `action first-start`: the recenter page sees the stick chord and says Recentred; Next; which hand, Right saved; the controls card, its second page, Close; the comfort page opens - the buttons pressed by the script's `action menu-primary` / `menu-secondary` | PASS 2026-10-15 (Linux, the fifth run): the chord recentred and the page said "Recentred", "right-handed saved", both card pages, "the first start's guide ends with the comfort page", "comfort page open" in the world |
| `tools/hand-scripts/quick-menu-assign.txt` | in the inventory the ring sets slot 5 to the item under the laser; in the world slot 5 then equips it | PASS 2026-09-27: "the hotkeys now: ... 5 Steel Longsword", exactly 1 click, "Steel Longsword equipped." on screen. PASS again 2026-10-16 (Linux, the 2026-10-01 save, slot 5 empty before, the laser on the Iron Longsword row, rot 64.5): the cursor held at 856,587 through the ring (the fix of 4e1806f), "(empty until now)", one click on "Iron_Longsword", "5 Iron Longsword", "hotkey 5 used". With the laser on the top row (rot 68) the ring closed without a hotkey before the release - open, docs/controls-spec.md 4.5 |
| `tools/hand-scripts/vanilla-quickkeys-assign.txt` | experiment: a number key held and a click on an item sets the hotkey | ran 2026-09-27: slot 5 read "Steel Longsword" |
| `tools/hand-scripts/stow.txt` | a dropped sword grabbed, brought to the chest and let go goes into the inventory | PASS 2026-09-27: "taken (no owner ...)", the inventory lists it. FAILS since 2026-09-27 18:44: the save stands the player elsewhere and the hand no longer reaches the sword - new positions needed. FAILS again 2026-10-11 with the body on and off alike (control with `ini Body Visible=0`) |
| `tools/hand-scripts/hand-bodies.txt` | the hands and a drawn sword as keyframed Havok bodies: made, in the player's world, driven, out in a menu, a dropped sword pushed by a hand body alone (PushWorld off), the bodies following into the exterior | PASS 2026-09-28: the sword from 252,-1585 to -162,-1581 |
| `tools/hand-scripts/hand-measure.txt` | the hand and weapon bodies against what is drawn: finger bones and the drawn weapon node, `Measure:` lines; the push-people layer switching with a fist | PASS 2026-09-29: the finger joints 4.4-7.5 units behind the grip, the capsule to 11.5 ahead; the drawn blade 11 degrees off the capsule, its far end 14 units aside; "no longer pushes people now (layer 23)" with the fist |
| `tools/hand-scripts/hand-shapes.txt` | small objects placed beside the player: the mesh's world box against the Havok shape's (getAabb) | PASS 2026-09-29: a cup, Havok 1.2-2.7 units beyond the mesh per side (a goblet 1.1-3.0, a tankard 1.7-5.0, the longsword 0.0-0.8, from copies of it with other items) |
| `tools/hand-scripts/blade-body.txt` | WeaponStopsAtBodies and the hit-stop: a beggar placed and frozen (tai), the blade turned slowly round the player at chest height and level just above the floor (rests on them, pressed on goes in), then fast (a swing goes in) | PASS 2026-10-09: "rests on" the lying beggar, "pressed on into" them at 21.2 units, "a swing went into someone" in the frame the strike met them; 18 of 18 bones read; at most 0.008 ms a frame. FAILS 2026-10-11 with the body on and off alike (control with `ini Body Visible=0`, the same contact lines): the beggar placed stands 1.7 m ahead (capsules at -144..-164), not lying where the sweeps go; no "rests on" - the scene, not the contact step, by the capsules; not looked into further |
| `tools/hand-scripts/blade-parry.txt` | WeaponParries: a melee bandit (00069AD1) attacks a player made untouchable (tgm) while the blade circles at the chest; every blow at the player logs where the bandit's blade stood | PASS 2026-10-09 with CombatReach 85: parries, "blocked 1.00 of it". The first run, at the game's own 128: no parry - the bandit's blade tip 131-191 units from the eyes at every blow, its torso 140-165 away; with the console's setgs 85: the tip 24-55 units, ten parries |
| `tools/hand-scripts/slow-approach.txt` | SlowApproach with the radius at 2 m: a melee bandit (00069AD1) comes at an untouchable player, who walks off and back; within the radius its run is a walk, past the radius and a metre it runs | PASS 2026-10-09: "is near - walks" (flags 0101, moving 58-104 units/s), "is no longer near - runs as it will" at 3.0 m (then 0201, run 1, 357 units/s), near again - walks; the first run with 10 m saw it walk (398 runs turned) but the scripted player cannot outrun it |
| `tools/hand-scripts/reach-walk.txt` | walking with a reached container's menu open (the player's controls gate 0040DC61), the switch to a second chest by reaching, the walk-away close | PASS 2026-10-10 (the second run; the first had the console eat the scripted walk, both chests at one spot): "the player walks with it open", "the player's controls run - the sticks walk under menu Container", the feet 0.36 then 0.85 m off with the menu up, "a hand at another - closed for it" -> "the other one reached - activated" -> "its menu opened", "walked away - closed" at 1.51 m |
| `tools/hand-scripts/grapple.txt` | OffHandGrabs: a melee bandit (00069AD1) attacks an untouchable player with a longsword drawn and held aside; the left hand works a grid to the player's left (where the bandit stands), closing its grip at each spot, sliding 30 cm, then jumping back as it opens | PASS 2026-10-10 (the fifth run): "holds 1B381BE8 by the torso (-0.0 units off it)", "legs held", "holding ... 0.3 units from where the hand wants them" at 0.2 s, 2.2 at 0.4 s, 10-12 at 0.6-0.8 s (the stagger and the AI pull against the placement), "thrown down - the hand at 27.3 m/s". Runs 1-2 found the bandit left and behind the head (the grid was ahead); run 3 held once and lost it in 0.6 s with the proxy knockback over a frame, run 4 twice with it over 0.3 s; the placement each frame holds. 2026-10-11 with the body IK on: PASS (run 2); run 1 ended when the console typing at "ready" went into the Esc menu, and a control with the body off failed "holding" and "thrown" - flaky either way |
| `tools/hand-scripts/grapple-ragdoll.txt` | GrabRagdoll: grapple.txt's scene and grid with the ragdoll hold on - a grip on the bandit's torso, an arm or a leg knocks it down into its ragdoll (the engine's knockdown, no push), the part's Havok body is driven to the hand under the weight's budget (GrabWeight=1: `the pull on`, `cut to the budget`), the jump back throws it with the hand's speed | PASS 2026-10-13 15:54 with the weight (and 2026-10-13 morning without it, the first run): "the pull on 1D274CF0 - the Bip01 R UpperArm body's mass 40.0, the ragdoll 530 in 16 bodies (Havok's masses), gravity 73.6; Strength 25, GrabWeight x1.00: a budget of 5518, which lifts 75", "holding ... right arm as a ragdoll 0.2 s - the body 19.7 units from the hand, driven at 0.5 m/s, pulled at 13.8 m/s^2 (19 of 19 changes cut to the budget)", 20-31 units behind through 0.8 s, "thrown - the hand at 27.8 m/s ... sent off at 2915 units/s (41.7 m/s)", a second hold by the left arm; the morning's run: the body 1-5 units from the hand with no limit. Not seen: a grip on the lying body, a hold ended by the get-up, the standing-hold fallback. The run of 15:50 quit to the main menu at the drawn mark (Pause, a Message box, the game gone), cause unknown, not seen again | 2026-10-15 afternoon (the reach 14, the pull x4, the loss a throw): PASS, two takes - the torso at -2.5 units and the right arm at 10.1 (past the old reach of 6) - the budget 22073 lifting 300, the torso 25 to 38 units from the hand over 0.8 s, the arm 3.5 to 18, both thrown by the jump back at 22.7 m/s, no loss |
| `tools/hand-scripts/enemy-health.txt` | EnemyHealthBars: two melee bandits (00069AD1) attack an untouchable player standing still, the synthetic head facing the nearest and pitched up; each gets a bar above its head (`Enemy health: a bar over ... above the head bone`) painted with the game's own frame (`the game's own bar model found`, `read off the game's texture`, at the marks `the overlay up (the game's own frame n of 40)`; the first bar's painting saved as OBVR-EnemyHealth-<mark>.bmp), then `killall` at fight3 so the bars go empty, linger two seconds and go (`bar went`); OBVR's own painted bar is the rejected fallback | PASS 2026-10-14 09:09 with the raise as a setting ("1.7 m away, 0.32 m above the head bone (bar 1 of 8)", the second at 2.3 m, the marks and the linger as before); PASS 2026-10-13 15:46 and 15:53 (the game's frames; the first build's PASS of the morning had OBVR's own bars): "geometry 18324EE8 health:0 ... 40 textures, the frames", "frame 1 of 40 read off the game's texture ... 128x32 pixels, the picture opaque from column 26 to 104" (the offline decode's columns exactly), frame 40 "59 to 69", both bars "the overlay up (the game's own frame 1 of 40)" at fight1-3, "bar went" for both; the saved picture the golden arc on nothing. Six rounds before (14:36 to 15:31) tried the model itself cloned into the world: a crash on a clone without render states, then a black, a white and a grey box - the world's shader refuses the frames for having no mipmaps (hud-on-hands-spec). SteamVR's dumps do not carry overlays, hence the picture file |
| `tools/hand-scripts/exhaustion.txt` | Exhaustion: a longsword drawn by the holster; the console sets the Paralysis actor value (the fall skipped, the exhaustion begun by it, the game's gasp and pants, the weapon lagging under slow swings), clears it (over); then forces the fatigue to -100 (the collapse skipped, the gasp, the pants), back to the base (over) | PASS 2026-10-13 15:48 (the game's gasps; the fourth run of the morning, with OBVR's own waves, PASS too): "paralysis fall was skipped ... knock state 0; they stay on their feet, exhausted", "begun ... paralysed 1; the weapon's weight x4.0", "the gasp - the game's NPCHumanGaspFemale (000C80EC) at 100% volume; a pant every 1.3 s with 100% of the fatigue missing", "a pant ... (1 pants, 1 gasps this spell)", "over after 5.6 s"; "fatigue collapse was skipped ... fatigue -99.9", "begun - fatigue -100 of 145 (-69%)", the gasp and the pants, "over after 9.8 s"; the swings up to 0.33 m from the hand (0.12 m fresh) in the morning's run. Runs 1-3 of the morning: the game has no `damageav` (ForceAV instead); the tutorial's "Your Fatigue is low" message box pauses the world (no knock step under it; Enter and Space do not close it) - so the fatigue comes last |
| `tools/hand-scripts/blade-wall.txt` | WeaponStopsAtWalls: a longsword drawn by the holster, pointed down and lowered onto the floor, then far below it, then raised: held, let go, free again (`Contact:` lines) | PASS 2026-10-09: the floor met at -256 (layer 1, motion 7), the tip held at -254 while the hand went to -272, let go at 24.2 units, free again; 2.0 rays and 0.004 ms a frame |
| `tools/hand-scripts/stow-owned.txt` | the same sword owned by Baurus first: taken as stolen | PASS 2026-09-27: "taken (owner 00023F2A ...)", the red hand on it in the inventory |
| `tools/hand-scripts/take-only-by-hand.txt` | TakeOnlyByHand=1: A at the sword is kept from the game, the sword stays; stowing still takes it | PASS 2026-09-27. FAILS since the save change of 2026-09-27 18:44, like `stow`: checked 2026-09-29 against a build of 423b77a, it fails there the same way - new hand positions needed |
| `tools/hand-scripts/activate-takes.txt` | the option off: the same A takes the sword - the path of the crash of 2026-10-12 (the pick's hold on a thing the game deletes): expects "Pick: ... is gone from the cell - let go at once" since | PASS 2026-09-27, before the pick's hold existed; to be rerun with the fix of 2026-10-12 |
| `tools/hand-scripts/laser-drag.txt` | the laser placing the cursor (LaserCursorDirect=1): the map opened with F4, the trigger pulled on it and the hand turned - the button held, the engine's own cursor walked after the beam, the map panned (read from the map menu: `HandScript: the map's drag point`, the drag's "let go" line) | PASS 2026-10-10: "the engine's own cursor went from 1323,961 to 1680,1349, the map's pan from 2037.7,1657.4 to 1886.7,1493.4". The control on the 4.36 code: no tile search at all ("active menu none", no "placed at" line). The game window's pictures do not show the pan (the monitor's copy is not redrawn) |
| `tools/hand-scripts/slider-knob.txt` | Pause -> Options -> Gameplay; the trigger pulled 20 pixels under the difficulty slider's knob and dragged right: the knob snap takes the knob, the drag moves it | PASS 2026-10-10: "takes the knob \"horizontal_scroll_marker\" 0,-23 pixels off the beam", the knob's x 3.6 -> 292.4. The same pull on the build before: the track pressed, nothing moved |
| `tools/hand-scripts/combat-ring.txt` | CombatRing with a 3 m ring, one at a time: three bandits (00069AD1) at an untouchable player; one has the turn, the others wait at the ring and go round | PASS 2026-10-10 (the second run): the waiting held at 3.0 m (order 2), one sent back walked out 1.8 -> 3.0 m, the turn passed six times in 25 s. The first run, with the flags alone: a waiting fighter given a walk backwards stood at 1.9 m - the flags do not move NPCs, the ring now places the steps |
| `tools/hand-scripts/attack-tempo-vanilla.txt` | the control for the two below: one bandit at an untouchable player, the ring off, tempo and frequency 100 - nothing followed, nothing scaled; the swings, attack actions and hold draws logged as the game has them | PASS 2026-10-11: the swings' time ran x0.90 of the clock (the weapon's speed; the sequence's frequency reads 1), 66 and 32 swings in two minutes of marks, holds drawn 0.5-1.5 s |
| `tools/hand-scripts/attack-tempo.txt` | EnemyAttackSpeed=50: every attack sequence of the bandit followed, its time run at half what the engine runs it | PASS 2026-10-11: "x0.50 read back" on every attack - the swings ran x0.35 of the clock at a weapon's 0.70; a power attack of 1.47 s of animation in 4.18 s. Two earlier runs read 0.40 and 0.60 for 0.50: the engine adds the weapon's speed to the offset as this does, so the step taken back is now scaled by the rate measured over the first frame |
| `tools/hand-scripts/attack-frequency.txt` | EnemyAttackFrequency=50: half the attack chance, twice the hold between the bandit's melee decisions, in scope through Actor::GetCombatStyle | PASS 2026-10-11: 33 swings in the minute the control had 66; 12 holds drawn, each "(x2.00)" (1.1-3.0 s of 0.5-1.5) |
| `tools/hand-scripts/hand-wall.txt` | HandsStopAtWalls: nothing drawn, the left hand pointed down and lowered onto the floor, pressed on, raised; then the right hand the same (`Contact: the left hand` / `the right hand` lines) | PASS 2026-10-11: both hands "met body 188401C0 (layer 1, motion 7)" at -256, the fingertips held at -253 while the controller went to -288, "let go - 31.8 units" (the right 33.2; the cap 21), "free again" raised; 2.0-2.9 rays and 0.011 ms a frame |
| `tools/hand-scripts/shield-wall.txt` | HandsStopAtWalls with a shield: the Arena Iron Shield (000733ED) on the arm and a longsword drawn by the holster; the shield's disc read from its model, the left arm lowered until the rim meets the floor, pressed on, raised | PASS 2026-10-11: "the shield's shape - 695 vertices, ... spokes 20.9 and 20.9 units, 13.1 thick", the blade's step on the right hand ("active 1, the blade 67.2 units"), "the shield met body 181CA4F0 (layer 1, motion 7) at -256 - its point 0.00 of the way along" (the rim), pressed on to a gap of 12.2, "let go - 22.7 units", "free again"; 2.0-3.7 rays and 0.005-0.012 ms a frame |
| `tools/hand-scripts/hand-body.txt` (written by `tools/gen-hand-body.ps1`) | HandsStopAtBodies: a beggar placed and frozen (tai), nothing drawn, the left hand swept in 5 cm steps every 0.4 s through the space round and through the middle of the player at three heights (rests on them), then the same path in 10 cm steps every 0.05 s (goes through them) | FAIL 2026-10-11 after five runs, the slow half proven: run 5 "the left hand rests on 1BBD3060" three times (chest and hip, held 0.3-3.0 units from the controller). Runs 1-2 (10 cm steps) read 3-11 m/s from one frame's velocity and went through - the speed is now the way come over 0.1 s (`HandTravel`); run 3 found the swing detector started by a scripted step - a fist's swing now counts only with the fists up; run 4 missed the beggar standing in the middle (0.85 m ahead) - the path now crosses the middle. Open: in run 5 the eyes jumped 190 units sideways and 61 up during the knee loop while "player at" stayed put (the camera node moved, the player did not), so the fast loops ran out of the beggar's 130-unit reach - cause not found yet, the fast half unproven |
| `tools/hand-scripts/blade-shield.txt` | the stoppers (controls-spec 4.32): the Arena Iron Shield on the arm and a longsword drawn, the blade brought slowly onto the shield's face, pressed on past the cap, drawn back and brought on again | PASS 2026-10-11: "the shield's shape - 695 vertices, ... spokes 20.9 and 20.9 units, 13.1 thick", "the blade rests on your shield at 251 -1635 -163 - its point 0.25 of the way to the tip, 1.33 m/s into it; held 0.2 units from the hand", "pressed on into your shield - 21.4 units from where it rested; it goes into it, passed", and after the way back "the blade rests on your shield" again |
| `tools/hand-scripts/hand-blade.txt` | the stoppers: a longsword drawn in the right hand, the left hand (nothing in it) brought onto the blade from above, pressed on, raised | PASS 2026-10-11 (run 2; the first run's only failed line was the state line's exact ray count): "stoppers 0+0 and 1+0 (capsules+discs)" (the blade a capsule stopper for the left hand), "the left hand rests on your blade at 262 -1619 -156 - 8.04 m/s into it; held 10.3 units from the controller", the gap 10.2 then 8.0 units pressed on, "held 0" raised |
| `tools/hand-scripts/ring-wall.txt` | CombatRing with the steps probed, turns 5-15 s, the weakest first: three bandits at an untouchable player, the player walks ahead 25 s to the hall's far wall and stands 14 units from it, so the waiting fighters' 3 m circle crosses the wall | PASS 2026-10-11 (runs 3-5 of the day, the final build): "step round refused - something fixed at knee height (rise 0 units); it goes round the other way" for all three, 0-2 refusals a fighter in 5 s, every position on the player's side of the wall; the legs 67-88 units a second along the step and the ring 0-12 placed (the state lines); turns drawn 5.5-11.6 s, ended within a swing of their length. Runs 1-2 (the sides the wrong way round, the action as the blow): the waiting went round the wrong way at 20 units a second and stood at the wall with no refusal logged, turns of 6 s ran 15 - the two findings in combat-comfort-spec |
| `tools/hand-scripts/hands-in-menus.txt` | HandsInMenus: the inventory opened with the left B (the hands stay, pinned and drawn), closed, the Esc menu opened with the key (the hands go), closed | PASS 2026-10-11 (run 2): "menu F1-F4 stack entry (0x001) - the hands stay - pinned and drawn as in the world", "at the mark - menu 1 (F1-F4 stack entry), the hands shown, pinned 1", "menu Pause (0x3F5) - the hands go", "at the mark - menu 1 (Pause), the hands away, pinned 0", "no menu - the hands as in the world"; the pictures show the hands beside the inventory and gone in the Esc menu. Run 1 behaved the same but named the menu "Inventory" in two expect lines: the stack's top reads as the F1-F4 entry |
| `tools/hand-scripts/body-probe.txt` | Debug.BodyTreeProbe: the body shown with its arms kept, the head looking down; the third-person tree ("Body tree:") and the arm and leg chain's local transforms ("Body bones:") logged, eye dumps at each mark | PASS 2026-10-11: 22 bones, every one along its +x (the upper arm 21.85 units to the forearm, the forearm 14.78 to the hand, the thigh 33.67, the calf 27.33), the forearm's and the calf's local rotation a turn about z alone (-32.5 and -18 degrees, -17 and -14); the eye dumps show the body's legs and torso under the headset at -60 and -80 degrees |
| `tools/hand-scripts/head-pitch-probe.txt` | the synthetic head pitched to -60, 0, 40 and -85 degrees and held six seconds before each mark; "Camera: at the mark" against the game window | PASS 2026-10-11: the camera read exactly the pitch scripted; the window showed the hall level at -60, the ceiling at 0, the floor at 40 - the window is not the headset's picture (see "What the pictures cannot show") |
| `tools/hand-scripts/body-ik.txt` | the body IK: the hands held out in front, raised, across the chest, 1.2 m out to the side; a 45 cm crouch; a 70 degree turn of the head in steps | PASS 2026-10-11 (run 9 of the day): "22 of 22 bones found"; every reachable hand reached with the wrist 0.00 units off it and the first-person arm on the body's within 0.01 units and 1.3 degrees; across the chest the elbow "moved out of the torso" and in front of the chest; 1.2 m out "falls short of its hand by" the rest, grown x1.15; the crouch "let down 32.0 units ... the feet lifted back 29.0, 2 knee(s) bent"; the turn "-35.8 degrees off the head" right after it and "-1.8" three seconds on; 0 body bones left by the engine. Runs 1-8 found: the flat wrist lid drawn over the arm (a dark octagon), Enhanced Camera's eye offset 14 putting the shoulders 12 cm back, the elbow through the neck for a hand across the chest, the swivel clamp that came first taking a natural elbow-back for one inwards, a controller "pointing ahead" holding the hand 54 degrees fingers-down. PASS again 2026-10-12 on round 2 (27 of 27 bones, "the trunk held at the standing pose" at every mark; the crouch "let down 45.1 units, the feet lifted 18.8" - 31.5 of it the crouch, the rest the game's sneak camera, docs/player-body-spec.md; the turn -35.7 then -1.8). PASS again 2026-10-16 with the skeleton's check in front of the placement (docs/player-body-spec.md, round 4): "Body check: Bip01 scale 1.000 under a parent of scale 1.000 (columns 1.00 1.00 1.00), the head 112.4 units from it, the right arm 21.8 + 14.8, the left 21.8 + 14.8 units (0 = not read) - sane", the body never left hidden; 3212 frames, 1.8 % reprojected. PASS again 2026-10-16 (beta 3): the dry run "after the engine's own update of Bip01, nothing written" and the check "after the placement (Bip01 let down 2.4 units)" both "- sane", the same numbers as before them; 3184 frames, 1.9 % reprojected. PASS again 2026-10-16 (beta 4, the true inverse in place of the transposes): the same three lines, 3228 frames, 2.4 % reprojected |
| `tools/hand-scripts/body-ik-tall.txt` | the body IK on a race with height (the Altmer report of 2026-10-16): the three "Body check" lines on a body a tenth taller, every arm following, nothing left to the body; `console-menu SetRaceScale <race> 0/1 1.1` for every race before the load | FAIL 2026-10-16 (run 1, beta 4): all twenty SetRaceScale lines taken and run at the main menu, the loaded player still columns 1.00 and the head 112.4 - the race's scale set at the menu does not reach a player built from a save; every other line sane and PASS. PASS 2026-10-16 (run 2, beta 4, the tester's Altmer save in the Imperial Prison cell copied to OBVRHarnessTall): the body read exactly the reporting user's numbers before the placement ("columns 1.00 1.00 1.10", the head 123.7, the right arm 23.9 + 16.2) and stayed so after the engine's update and after the placement ("let down 1.8 units" where his log read 55423), every arm reached (81 lines), the far hand fell short by 15.9 grown x1.15, the trunk held, nothing left to the body; 3215 frames, 2.1 % reprojected |
| `tools/hand-scripts/body-ik-shield.txt` | the body IK with the Arena Iron Shield worn and a longsword drawn: the shield held out, raised to block, brought in close, lowered onto the floor and pressed on | PASS 2026-10-11: "1 thing(s) on its forearm carried with the hand", 9-19 units off where the IK's forearm would have it; the shield's disc "centre -6.6 -9.4 1.7" in the hand's frame (without the IK -6.5 -9.4 1.7); "met body ... a knock", "let go - 22.8 units" (without the IK 22.7), "free again". The first build carried the shield with the IK's forearm: the disc read again each second jumped and `shield-wall.txt` never held at the floor; a locked wrist was tried next and left the forearm 19-46 degrees off the body's arm. PASS again 2026-10-12 on round 2 (met, let go, free again; the left arm reaching with the shield carried) |
| `tools/hand-scripts/yield.txt` | the yield by gesture hardened (vr/Yield.h): a melee bandit attacks an untouchable player (tgm), the head facing it (`head face 1`); both hands held up before the chest, the palms to the front (right rot 90 0 90, left rot 90 0 -90; the fingers up until the palm gate of 2026-10-12), rocked together 8 cm left and right at 0.2 m/s with the hands empty; then fast sideways swings of both open hands (40 cm in a step); then the longsword drawn by the holster and the rocking again | PASS 2026-10-12 (run 5 of the day; run 1 found the bandit circling out of the head's 30 degree cone - `head face` built for it; run 2 found activate without a block opening the bandit's pockets, "Container (0x3F0)" - the fists raised first since; run 4 found the armed yield's crosshair on nobody, the bandit having walked on in the 20 frames of block - the pick's target follows the attacker since): with the hands empty "the fists raised first for the engine's block", "the fists are up after 0.12 s", "activate goes down after 10 frames of block - the player's action 6 (block), the crosshair on 1BDB0E60", "done - the fists raised for it are lowered again"; the swings: no yield, "the right hand swinging, the left hand swinging, the right hand too fast, the left hand too fast" at the mark; with the sword drawn the yield again, "a weapon in hand", the crosshair on the bandit. The bandit fought on both times (a hostile one refuses, as in vanilla); PASS again 2026-10-12 with the palm gate: "palms to the front 1.00/1.00", both yields, the swings refused |
| `tools/hand-scripts/body-ik-smooth.txt` | the body IK's smoothness (Debug.BodyTrace, `tools\body-trace.ps1`): the hands hanging, held out at the reach's edge, the right one before the chest at the torso's keep-out and slid an inch at a time across it; the player walking; the head pitched in steps; a crouch (which asks for the game's sneak) | PASS 2026-10-12 (run 4 of the day, round 2): frame to frame, with the hands still the body 0.014 units against the headset, walking 0.017, the walk's start 0.017, the sneak animation's first frame 0.03, the trunk held at every frame after 0.5 s, the feet solved down 3.4-4.1 while walking and up 22-31 for the crouch, every bone write through the draw ("held 27/27"). Run 1 (the build of 835f15a): 0.30 walking, 1.45 at the walk's start, 44 at the sneak's first frame - the finding of the round. Run 2 found the feet solved to the animation's own spot while the placement stands the body 8 units forward of it (the legs angled back; carried since). Also seen: the game's sneak lowers the camera 21.7 units and 12.7 back in one frame on top of the headset's crouch, and the camera stands 2.5 units higher at 60 degrees down than level, one frame behind the headset (docs/player-body-spec.md, round 2) |
| `tools/hand-scripts/offhand-punch.txt` | the off hand's punch with a little finger that never reads (vr/FingersRead.h): a beggar frozen 60 units ahead, a longsword drawn, the left hand closed with `left little 0` and driven into the beggar's chest | PASS 2026-10-12: "the left hand's fist goes by 3 fingers - one never read (... little never ...)", "made a fist (curls 1.00 1.00 1.00 0.00)", "the left hand at 54.4 m/s ... - hard (the off hand's punch, a weapon drawn)", the beggar knocked down; the second blow, open, met nobody |
| `tools/hand-scripts/ring-walk.txt` | the fighters' ring walking and holding still in a pause (CombatRingLogic.h, "The legs walk; the ring only adds to a walk"; game::WorldPausedForMenus): three bandits, a 3 m ring, four marks fighting, the Esc menu for four seconds, three marks after | PASS 2026-10-12: the waiting walking round by their legs (legs group 5/6), the legs 46-86 units a second and the ring 1-34, steps held 0-21 a mark; in the Esc menu "paused - <actor> at x y" the same to the tenth at both marks; after it the ring went on |
| `tools/hand-scripts/body-look-wrist.txt` | round 3 of the body IK: eye dumps at 45, 65 and 85 degrees down with the hands hanging (the body stepped back, BodyLookDownShift, always on; "Body: stepped back" expected), then both hands ahead turned 30 degrees a tenth of a second to 180 and on to 210 (ForearmRollsFor); since the evening of 2026-10-12 with `Debug BodyView=1`: "Body view: down the body" expected at 65 and 85 degrees, "into the body", "left out" and "not measured" rejected at every mark | PASS 2026-10-12 (run 3 of the day), before the view check; with it run four times the same evening without the headset's display (SteamVR's "Monitor not found", so no neck pivot and no step back): the check read "into the body" at 85 degrees on 3.1 % of each view - the collar - and "down the body" at 65, every shape read; the PASS with the headset is open. Earlier that day: no open collar at any of the three looks; the hands' roll followed the whole way (right 61.5 -> -190.7 -> -205.9, left -102.8 -> -173.9 -> -160.2), the forearm a third, the twist two thirds; the eye dumps show the wrists full. Runs 1-2 (the elbow swung with the roll, since taken out): the left elbow swung 36 degrees with the hands at rest, and with the hands turned half round in one step (run 2) both elbows across the chest. A one-off sweep with the then-setting LookDownBack at 16 (65, 75, 85 and 89 degrees) found the shift has to grow with the look: at 85 no ring, at 65 the torso out of sight; the setting and the sweep went the same day |
| `tools/hand-scripts/left-punch.txt` | the off hand's punch with a weapon drawn (GrappleLogic.h OffHandStrikeFor, the grip squeezed as a fist on the Index is): a melee bandit at an untouchable player facing it, a longsword drawn by the holster, the left fist (curl 1, grip 1) punching straight ahead at about 4 m/s at eight heights; "Shove: the left hand at N m/s towards ... (the off hand's punch, a weapon drawn)", no "the grip is held" refusal | PASS 2026-10-15 (Linux): two punches landed, 6.7 and 3.4 m/s, both hard; the grip's closing logged "held nobody" once (72 units off, the fist closed far from them) and nothing else |
| `tools/hand-scripts/left-fist.txt` | the left fist's strike by motion with the fists up (MeleeHits.cpp, hand 1): the weapons taken off, the fists raised by the weapon hand's fist, a frozen beggar placed, the left fist swung round the player at chest height a lap each way; "Hands: the blade met ... swing 21474836nn" (the left serials), "Hands: struck", "Hands: a left-fist ... swing" | PASS 2026-10-15 (Linux): the beggar met on every lap - a light strike (health 26.0 -> 25.0) and two standing power strikes (25.0 -> 22.4 -> 19.8); the fist's reach 57 units. Since the afternoon the beggar stands 55 units ahead and the hit needs the touch (MotionHitsByTouch): PASS, three hits by touch ("the fist touched ... within 4.0 units of a capsule"), 26.0 -> 25.1 -> 22.7 -> 20.4, the beggar standing where he was |
| `tools/hand-scripts/block-angle.txt` | the block's angle and reach (BlockByAngle, BlockReachUnits, game/BlockAngleLogic.h): a melee bandit attacks an untouchable player facing it (`head face 1`), a longsword drawn by the holster and held as a guard level across the body, then upright, then level 55 cm over the eyes; every blow the engine would block logs "Block angle: a blow from ... at the weapon - the blades, N degrees (at least 60), M units off (at most 40) - blocked / THROUGH, the guard stands wrong / THROUGH, the blow passes the guard"; the marks' state lines show "blocking 1, block key 1" | PASS 2026-10-14 21:52 (Windows, the reach and the third phase; degrees between the blades / units apart): the level guard 83/27, 80/37, 58/14 (through by the angle), 89/5, 64/15; the upright guard 63/10, 64/19, 67/30 (blocked); the high guard 65/65, 88/84, 87/64, 79/86, 86/44, 62/66, 62/66 "THROUGH, the blow passes the guard" and 46/77 by the angle - every blow at it through; the engine's cone 1 to 7 degrees off the view. PASS 2026-10-14 (Linux, before the reach): the level guard 56 (through), 61, 88, 83 and 61 degrees, the upright one 64 to 67; the parry's lines of that run put the attacker's blade 16 to 37 units from the player's at the blow, the measure the reach's 40 came from |
| `tools/hand-scripts/swing-fatigue.txt` | swings cost fatigue as vanilla's attacks do (game::SwingFatigueFor, the engine's own cost 0x00547560 and change 0x005E07D0): a longsword drawn, a light swing of 30 cm and a power swing of 1.3 m through the air, each charged once as it ends | PASS 2026-10-14 (Linux): "a weapon light swing cost 9.00 (weight 20.0); fatigue 145.0 -> 136.0", the power swing 45.00 |
| `tools/hand-scripts/hand-hud-always.txt` | the hand HUD shown always, the default since 2026-10-14 (ShowOnLook=0): the same palm turns as hand-hud-look.txt, no look counted, the HUD up at the marks | PASS 2026-10-14 (Linux), hand-hud-look.txt PASS the same day as the control |
| `tools/hand-scripts/body-look-into.txt` | the control for Debug.BodyView: the eyes put inside the chest (`[Body] EyeForward=20`, `EyeUp=-17`), the head bowed 65 and 85 degrees - the check has to say "into the body" at both marks, or body-look-wrist's silence about it means nothing | written 2026-10-12, not run (the headset's display was off that evening) |
| `tools/hand-scripts/blade-held.txt` | SwingsStopAtBodies (weapon-collision-spec section 17): a beggar placed 70 units ahead and frozen (tai), a longsword drawn by the holster and brought slowly level (pitch -27) to 45 cm out at chest height, four slashes across them at 4.5 m/s; "Contact: a swing is held by", "Hands: the blade met", since the afternoon of 2026-10-15 also "held until the hand comes back" (BodyLetGoMetres 0) and "Comfort: the knockback of" (EnemyKnockback 0: the beggar stays, every slash held), rejects "pressed on into" and "a swing went into someone"; `ini Debug ContactTrace=400` | PASS 2026-10-15 (Linux, run 3, the morning's build - the cap 0.60 and the game's knockback then): slashes 1 and 2 held by them (the guard's end at the slash's start, the tip at 4.1 m/s), both strikes landed, pressed on 42.4 and 43.3 units the blade went in; the strikes' stagger moved them 19 units back each time, so slashes 3 and 4 only struck. Runs 1-2: the jump to the slash's start was a swing of its own (0.37 m, 24 m/s) that struck and knocked them out of the tip's reach before the first slash - found by the trace. PASS 2026-10-15 (run 4, the afternoon's build and expectations): the beggar stood at y -1596 through all four slashes, every strike landed (health 26.0 to 22.2), "held until the hand comes back" at slashes 1, 2 and 4 (at 3 the blade was still held from slash 2, stuck on their far side while the hand came back round - the hold for good), the knockback skipped at every hit, the power flag's stack reading agreed. Run 5 (the hit by touch): all four slashes hit by touch on the drawn blade's own span, 28 units from the centre each; a run with the touch along the hand's axis hit nothing (the longsword's axis 25 degrees off it) |
| `tools/hand-scripts/blade-held-cap.txt` | the cap at people set and the game's knockback kept (BodyLetGoMetres 0.30, EnemyKnockback 100): blade-held's slashes; expects "Contact: pressed on into" as well - the old way, the blade into them past the cap, the beggar moved by each hit | PASS 2026-10-15 (run 2): held at 0.0 to 4.5 m/s, pressed on 21.0, 21.4 and 24.0 units the blade went into them, the strikes' knockback moved them 19 units a hit (y -1596 to -1520 over four), the stack reading agreed. Run 1 failed on the script, not the game: the expectations inherited from blade-held (held for good, the knockback skipped, nothing pressed in) contradicted the cap's own |
| `tools/hand-scripts/fist-held.txt` | PunchesStopAtBodies (weapon-collision-spec section 17): a beggar placed 75 units ahead and frozen, a longsword drawn, the left fist (curl 1, grip 1) punching straight ahead 85 cm at 4.5 m/s at three heights; "Contact: a fast left hand is held by", "the off hand's punch, a weapon drawn", no "the grip is held"; `ini Debug ContactTrace=400` | PASS 2026-10-15 (Linux, run 2): the first punch held by them 0.2 units from the controller at 4.3 m/s and landed hard (3.5 m/s, knocked down), the next two found nobody. Run 1: a punch of 72 cm ended 1 unit short of their chest - the fingertips trail the controller by 4 units (the hand's span). Run 3 (the afternoon, the hold for good): held 2.8 units from the controller at 4.3 m/s, "held until the hand comes back", the punch hard at 6.7 m/s and the knockdown (the shove's own, untouched by EnemyKnockback) |
| `tools/hand-scripts/tutorial-cards.txt` | the tutorial cards' key names in the mode's words (onboarding-spec 3.4, game/ControlWords.h): on the Altmer prison save, `setstage MQ01 28` raises the quest update's card with three `&sUActn…;` tokens, CloseAllMenus puts it away, then stage 34 (cast, journal) and stage 80 (sneak); then two cards swapped whole (game/TutorialCards.h): stage 26 (the drop) and stage 76 (the hotkeys, with a token of its own) | PASS 2026-10-16: "control 8 (DrawSheatheWeapon) answered \"reach to your left hip and squeeze the right grip, or make a fist\" (the engine's own \"push F\")", "Tutorial card: the entry \"Click on an item to equip it. Click agai...\" swapped for OBVR's words", the cards' pictures read so; the walk's stage 10 raised nothing (passed on that save), Escape over a card opens the Pause menu on top of it |
