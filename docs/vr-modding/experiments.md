# Experiments

Measurements that decided something, kept apart from the architecture they decided.
Each: hypothesis, implementation, observed result, evidence, conclusion, follow-up.
Dead ends are in [failed-approaches.md](failed-approaches.md); this file is about
questions that got an answer.

## Does Gamebryo render twice per tick without advancing the simulation?

- **Hypothesis:** calling `kRenderScene` twice with the camera moved draws the world twice
  without the update step running twice.
- **Implementation:** `Debug.DualPassProbe` ladder: 1 = second render only, 2 = plus the
  camera move, 0 = the whole mechanism (captures included), hot reloaded so one session
  climbs it.
- **Result:** eleven thousand frames on rung 1, then 2, then 0; both passes returned,
  every submit answered 0, not one device error; then a cold start at rung 0 ran four
  thousand frames clean. The first run's device loss was the machine's known flake.
- **Evidence:** `OBVR-dual-ladder-pass.log`, `OBVR-dual-cold-start-verified.log`.
- **Conclusion:** yes for the simulation; no for the render walk's own time-driven work
  (see the frame-clock experiment). `CONFIRMED`.

## Which part of the render walk advances time?

- **Hypothesis:** animation controllers and the NPC head-aim advance by the frame delta
  on every walk.
- **Implementation:** zero `TimeInfo+0x0C` (`0x00B33E9C`) across the second render, guarded
  by a plausibility check.
- **Result:** the long-standing stutter (controllers ticking twice) and the sideways hair
  and helmets (head-aim re-run) went away. A microsecond instead of zero made the FaceGen
  head-part offset worse.
- **Evidence:** `7e98925`, `664b56d`; `OBVR.log` "Dual clock" line.
- **Conclusion:** `CONFIRMED`. Follow-up: the FaceGen face mesh is built once per frame
  with the first render's camera (open).

## Where does the skinning collapse come from?

- **Hypothesis chain (each acquitted in turn):** device state between the passes;
  different draw counts; different constant uploads; different palette bytes; different
  bindings; different dynamic-buffer regions or written bytes; zero-matrix draws;
  software vertex processing; draw addressing; index-side state; DXVK's handling of the
  discard traffic.
- **Implementation:** the counter suite in `InterfaceRenderHook.cpp` and the trace lines in
  `SceneRenderHook.cpp`, the pool timeline, `tools/DxvkRepro`.
- **Result:** every order-independent D3D9 sum equal between the renders; the replay
  rendered identically; the bone-range sums differed; the collapsed draws were fed by
  re-evaluated rows.
- **Evidence:** `docs/verification/pool-timeline-20260828.md` and `.txt`; commits
  `27fc4a2`..`52cacb1`.
- **Conclusion:** the engine's second skeleton evaluation is the fault. `CONFIRMED`.

## Are the bone palettes world-space or camera-relative?

- **Hypothesis (first):** world-space; the first uploads were bit-identical between the
  renders.
- **Result (headset):** the verbatim replay froze every body at the first eye - the
  identical rows were *stale*, not camera-free. Matched re-evaluated rows differ by
  exactly the eye baseline (about 4.6 units).
- **Conclusion:** camera-relative. `CONFIRMED`. The sign of the convention against the
  camera shift is calibrated at runtime rather than assumed.

## Does the eye's frustum read top-negative or top-positive?

- **Result:** reading "top" at v = 0 put the picture 0.602 of the height down; the next run
  reported it too low by about that much; `bottom/height` put it at 0.398 where it belongs.
  Both eyes report |top| > |bottom|, the usual brow-closer-than-cheek shape.
- **Evidence:** `bd1c2c8`, `eye_geometry_test` pins the direction.
- **Conclusion:** the texture's v runs opposite to the frustum's vertical axis. `CONFIRMED`.

## Is `fDefaultFOV` horizontal at the current aspect or a 4:3 figure?

- **Result:** the camera's frustum reads 1.0231 x 0.5755 at 75 degrees, 16:9: the 4:3
  reading with the horizontal widening.
- **Evidence:** `7d01b04`, `OBVR-camera-frustum.log`.
- **Conclusion:** `GameFovIsFor4x3=1`. `CONFIRMED`.

## Does the frustum write hold?

- **Result:** Oblivion renders with the frustum OBVR writes (`0eba115`,
  `OBVR-camera-frustum-used.log`). The frustum differs between the camera pass and
  Present (1.0231 vs 1.1188); which pass owns the wider one is not settled.

## Does the compositor's submission cost frame time?

- **Result:** 17.8 to 18.5 ms per frame with rendering on and with `Render.Enabled=0`,
  same distribution; `WaitGetPoses` never gated the game at 55 fps.
- **Evidence:** `OBVR-openvr-first-frames.log`, `OBVR-framerate-baseline.log`.
- **Conclusion:** the earlier guess (consistent times = pacing) was wrong. `CONFIRMED`
  for the mono era; the dual pass was not measured separately.

## Does DXVK on Windows change frame time?

- 18.4-18.7 ms with DXVK against 18.0-18.5 without; five samples; nothing claimed.

## Why does leaning forward feel unlike leaning sideways?

- **Result:** leaning forward lowers the camera about six times as much as leaning
  sideways (the head travels an arc), and forward motion carries little parallax; at
  gain 3 both felt alike, which confirms perception rather than arithmetic
  (`head_offset_test` proves the axes equal).
- **Evidence:** `OBVR-firstperson-lean.log`; `HANDOFF.md` "Open: leaning forward".
- **Conclusion:** not a fault. The predicted follow-up (stereo would make gain 1 enough)
  came true: `HeadMovementScale` ships at 1.0.

## Does the lean limit ever cut anything?

- **Result:** `raw=` equals `|lean|` on every line of the 0.0.5 log; maximum 12.1 units
  against a limit of 80. `CONFIRMED` never in seated play.

## Does a written player heading survive the frame?

- **Result:** yes; the engine adds mouse movement to `rotZ` rather than replacing it (rotZ
  standing at 1.8708, then 3.0148 across dozens of frames).
- **Evidence:** `9a27496`.
- **Conclusion:** the turn must be counted once (offset accumulated, base compensated).

## Which way do the camera heading and `rotZ` run?

- **Result:** the two columns sum to zero across sixty frames when the head is centred
  and to the head's turn when not.
- **Conclusion:** opposite; the step is subtracted. `CONFIRMED`.

## Where does the "aim jump" come from?

- **Result:** a 1.7-unit sideways step of the viewpoint on the frame the body takes the
  turn, reversed when it gives it back, eighteen times in nine shots; the arm that
  produces it is 4.60 units by chord and 4.58 by direct read.
- **Conclusion:** a displacement, not a rotation; `AimArcCorrection`. `CONFIRMED`.

## How does the third-person camera move?

- **Result:** on a sphere about a pivot above the feet, always looking at it (height below
  the pivot = r sin pitch to 0.01 units), radius 30 units in the measured run (a wall
  shortens it), eased by 0.05 of the remainder per physics step at 60 Hz.
- **Evidence:** `tools/third-person-run.ps1`, `ThirdPersonProbe`; the headset trace.
- **Conclusion:** measure the rate off the camera every frame. `CONFIRMED`.

## How long is a cast, and what does the action field do during one?

- **Result:** 54 frames Attack, 24-26 FollowThrough, then None, six casts alike; the
  "casting" flag stands 79 frames. At 90 Hz the animation is 0.59 s, not 0.88 s.
- **Conclusion:** a cast is an attack to the action field; lead times are measured in
  frames per cast. `CONFIRMED`.

## Which engine call makes the player's arrow?

- **Result:** not the animation-key handler (the NPC route); the attack update
  `0x005FCAB0` called from `PlayerCharacter::HandleInput` at `0x00672E0D`, found by the
  stack probe from inside the wrapped projectile factory.
- **Evidence:** `e29bb0a`, `079cec4`, `OBVR-aim-at-source-works.log`.

## Does the engine redraw the world behind a pause menu?

- **Result:** no; the scene counter stands still across every held frame; the engine
  blits a snapshot (`0x0040D160`, valid byte `0x00B33397`). Dialogues and the persuasion
  minigame keep rendering with no camera pass.
- **Conclusion:** `HeldStereo` for pause menus, `MenuStandIn` for the rest,
  `LiveMenuBackground` through the engine's own switch. `CONFIRMED`.

## Does a self-initiated world render draw anything?

- **Result:** from Present: ~344 vertex setup calls, zero draws, on menu and world frames
  alike. From inside the 2D pass: 461 draws in an open Esc menu.
- **Conclusion:** the moment decides, not the menu. Used for the live background's
  design, then superseded by the engine's own switch. `CONFIRMED`.

## Does the engine take the world away while a menu is up?

- **Result:** scene graph, camera and culling process pointers identical on world and menu
  frames; `NiCullingProcess+0x08` null on every frame.
- **Conclusion:** the emptiness comes from the walk (flag bit / frustum / accumulator),
  not from a missing scene. The +0x08 list reading is a dead end.

## Which screen size does each part of the 2D lay out against on an eye-sized frame?

- **Result (LayoutProbe):** films and the main menu background at the believed 2560x1440
  corner; in-game menus and freshly built text over the whole 4028x3380 buffer; the mouse
  between the two.
- **Conclusion:** raise the one process-local copy, shrink the interface viewport, detour
  the pick. `CONFIRMED`.

## Where does the hover offset come from under a raised copy?

- **Result (cursor ladder):** the hover zones have the drawn items' spacing, centred half
  the height difference lower; re-asserting the believed viewport around the tile search
  moves the zones; the pick normalises by the renderer's size getters.
- **Conclusion:** two detours (`kFindTileAtCursor`, `kPickNormalizePoint`). `CONFIRMED`.

## Does the interface pass draw after a second world render?

- **Result:** entered once per frame, every gate open, zero draws; 21-22 with one render;
  recovers within the run when the second render is cut.
- **Conclusion:** run the pass between the renders. Cause open.

## Is the recenter key dead during films?

- **Result:** the log shows the flat re-anchor firing; `WaitGetPoses` answers without a
  usable pose for the first seconds (logged on change since `72df027`).
- **Conclusion:** the key works; there is no pose to anchor to (task #25, open).

## Does Oblivion tolerate a 9Ex device?

- **Result:** no, on both runtimes. See failed-approaches.

## Do the freezes come from OBVR?

- **Result:** every freeze was a GPU device loss preceded by an `nvlddmkm` graphics
  exception; the watchdog placed the render thread inside DXVK's wait; Valve's wiki names
  `WaitGetPoses` as queue work.
- **Conclusion:** lock the queue around the wait (`6f4ccde`). Headset confirmation open.

## Which eye the collapse belonged to

- **Instrument:** `SwapEyeOrder`. The question was posed (`02116d5`); the bone-lock
  investigation answered it from the other side (the second render, whichever eye).

## Is the vanilla crosshair drawn in third person?

- **Result:** no plain crosshair, but the context icons and the sneak eye are; the first
  version pasted the borrowed copy over them. `BorrowedCrosshairWanted` stands down for
  both. `CONFIRMED` in the headset.
