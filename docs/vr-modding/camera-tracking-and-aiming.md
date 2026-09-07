# Camera, head tracking and aiming

The relationship between the things a flat game treats as one "camera", and how OBVR
keeps them apart. The heart of it is `OBVR_OnCameraUpdated` in `src/camera/CameraHook.cpp`
(about 1500 lines of callback) with its decisions lifted into `src/camera/FrameLogic.h`.

## The separation that VR forces

`Scope: General VR`, with Oblivion's specifics marked

```text
GAME BODY ORIENTATION      rotZ at PlayerCharacter+0x20 (radians, zero north, clockwise)
        |                  written by the mouse / stick, read by walking (MobileObject::Move)
GAME MOVEMENT DIRECTION    follows rotZ; never touched by OBVR

GAME CAMERA                the vanilla camera the engine builds each frame from rotZ/rotX
        |                  (first person: at the head; third person: on a sphere about
        |                   a pivot above the feet, eased at the physics rate)
        x
HMD ORIENTATION            relative to the recenter reference, yaw-only reference
HMD POSITION               relative to the reference position, in the recentered frame
        =
VR VIEW                    CameraNode.localTransform = base * head, position + offset
        |
LEFT / RIGHT EYE           the view stepped +-half the eye separation along its own x axis

AIM DIRECTION              the VR view's forward (cyclopean), before the eye step
        |
PROJECTILE / SWING         the player's rotX/rotZ, set to the gaze only INSIDE the engine
        |                  call that reads them, restored on the way out (AimAtSource)
WEAPON DIRECTION (drawn)   first person: the arms node turned at render time
                           third person: Bip01 Spine2 / Bip01 Head turned after animation
CROSSHAIR                  a quad at the depth of the aim ray's target, on the gaze
ACTIVATION RAY             the engine's pick ray, direction replaced by the gaze in third person
```

The rule OBVR arrived at, and the one worth taking to another game: **the body's heading
is one field with two jobs** (walking and aiming) in a Bethesda game, and they cannot be
separated in space. They can be separated in *time* (turn it only for a few frames) or,
better, in *place* (set it only inside the engine call that reads it for the shot). OBVR
went through both; the second is what ships.

## The camera chain, in order

`CONFIRMED` from code (`OBVR_OnCameraUpdated`)

1. The engine has written this frame's camera into `CameraNode.localTransform` (position
   and 3x3 rotation) and the hook fires at `0x0066BE6E` with `eax` = the node.
2. Point-of-view bookkeeping (`ObservePointOfView`): on a switch the look control is
   reset, the remembered menu camera dropped, the traces armed.
3. Config hot reload every N frames; `ApplyDialogZoom`.
4. `BeginFrame`: `WaitGetPoses` under DXVK's queue lock; the render pose is kept.
5. The engine's frustum is read off `NiCamera+0xEC`, written with the headset's union
   frustum (`MatchHeadsetFov`) or an override, and its tangents remembered for placement.
6. `HeadTracker::Update`: orientation from the render pose (fallback: `ReadHeadPose`),
   relative to the reference, converted `FromOpenXR` into Oblivion axes; position
   difference against the reference, rotated by the reference's conjugate, converted,
   scaled by `UnitsPerMetre * HeadMovementScale`, clamped by `MaxLeanUnits`.
7. Recenter edge (`KeyEdge`, `GetAsyncKeyState`, polled once per frame): the reference
   becomes the current orientation's yaw only and the current position.
8. The engine's rotation is remembered as the base for menu frames and the crosshair;
   the player's rotation is read *before* anything writes it.
9. Third person only: the chase camera's easing rate is measured off the camera itself
   (`MeasuredChaseRate`) and OBVR's record of how much of its own aim offset the camera
   has taken so far is stepped (`ChaseStep`).
10. `LookControl::Update` (headset connected only): levels the base (roll removed, pitch
    turned into height in third person and dropped in first), eases the turn if asked.
11. The aim's camera share is taken back out of the base (legacy turn machinery; zero
    when `AimAtSource` owns the aim), and the arc/tilt position corrections applied
    (`AimArcCorrection`, `AimTiltCorrection`; the first person camera stands 4.6 units off
    the body's turn axis, measured two ways).
12. Head offset added, rotated by the base; vertical look offset added along world up.
13. `finalRotation = base * headRotation`; the world-pick direction is set from it.
14. The aim pose for the source swap is decided (`AimSourcePose`), the third-person
    visual and head decisions made, the cast window stepped, the release clock stepped.
15. The stereo eye step: the camera is placed on the first eye (`StereoEyeStep`) and the
    dual pass armed (`g_dualArmed`, `g_dualShift`).
16. The hand-tracked mode, when on, runs its per-frame decision (`UpdateHandMode`).
17. The engine then calls `UpdateSelectedDownwardPass` on the node, which is why OBVR
    writes `localTransform` and never `worldTransform`.

## Coordinate conventions

`CONFIRMED` in the game (roll and pitch each visually verified with `Source=fixed`)

- Oblivion / Gamebryo: X right, Y forward, Z up. `EulerToMatrix(x, y, z)` in Z-Y-X order:
  X is pitch, Y is roll, Z is yaw.
- OpenVR poses: X right, Y up, -Z forward - the same convention as OpenXR. Change of basis
  `x_obl = x_xr, y_obl = -z_xr, z_obl = y_xr`; for quaternions `(x, y, z, w) -> (x, -z, y, w)`.
  Determinant +1; a mirror would be visible at once. Every source (OpenVR, fixed,
  simulated) delivers OpenXR convention and goes through the one `FromOpenXR`.
- The player's rotation triple at `TESObjectREFR+0x20` is radians; `rotX` is pitch with
  **positive looking down** (Construction Set wiki: "negative angles force the player look
  up"), `rotZ` yaw zero at north growing clockwise. Everywhere else in OBVR positive pitch
  looks up; `PlayerPitchForGaze` is the one place the sign turns.
- The camera's heading read off column 0 of its rotation runs opposite to `rotZ` (the two
  summed to zero across sixty frames whenever the head was centred; measured).
- Poses are read in the **seated** tracking universe, and the compositor's tracking space
  is declared seated too (`SetTrackingSpace` at connect). Three places seated and one on
  the default (standing) is how the HUD anchor once landed at (-9.2, -8.1, -2.7) metres.

## Head tracking

`CONFIRMED`, `Scope: OpenVR` / `Scope: General VR`

- **6DoF.** Rotation and position both reach the camera; the character never turns with
  the head. `PositionalTracking=0` drops the position.
- **Predicted pose.** The orientation and position used for rendering are the ones
  `WaitGetPoses` returned this frame (predicted to photon time). `ReadHeadPose`
  (`GetDeviceToAbsoluteTrackingPose` with zero prediction) is the fallback when there is
  no frame loop (rendering off, compositor not reached).
- **Recenter** (`Del` by default, virtual-key code, decimal or `0x` hex, `0` off): the
  reference orientation becomes `YawOnly(current)`, the reference position the current
  one. Yaw only, because a reference carrying pitch or roll tilts the whole world for as
  long as it stands. The flat-picture anchor (cinema screen) and the HUD's room anchor
  are also reset on the key, on the frames the camera hook does not run (`OnFrameEnd`
  polls the same edge).
- **First valid pose is the reference** for position, so the camera is not flung 1.2 m
  (the seated origin sits on the floor) before the first recenter.
- **Lean limit** (`MaxLeanUnits`, 120 at scale 1.0 as shipped): a sphere about the game's
  camera position, direction preserved. The 0.0.5 log showed it never cut a lean (session
  maximum 12.1 units against 80); it exists for tracking glitches and for standing up.
- **Never smoothed.** Easing a tracked head shows the wearer where their head was; the
  latency is felt. What OBVR eases is the motion it generates itself (vertical look, turn).
- **No neck model.** The offset is the tracked head position; the eye step is applied
  along the view's x axis. `Scope: Oblivion` decision, not a general one.
- **World scale:** 69.99125 units/m; `HeadMovementScale` ships at 1.0.
- **Intro films:** the render pose is unusable for the first seconds (`WaitGetPoses`
  answers without focus or with an invalid pose), so the flat picture rides the head and
  the recenter key has nothing to anchor to (task #25, open; the log now says which code
  `WaitGetPoses` returned).

## What the look controls still do

`CONFIRMED` in the game (0.0.4 report, `look_control_test`)

With a headset delivering poses:

- Turning left/right stays with the mouse and stick: there is no other way to face a
  direction behind you. `SmoothTurning` eases it (off by default; "camera rotation
  compensation" in Luke Ross's vocabulary); `TurnSpeed` is the rate.
- The vertical look is taken away (`BlockVerticalLook=1`): in third person it becomes
  camera **height** (`VerticalLookUpRange` 60 up, `VerticalLookDownRange` 120 down -
  asymmetric because the camera starts at head height, measured), eased; in first person
  it does nothing.
- Roll is removed from the base every frame: a tilted horizon the inner ear disagrees with
  is the classic trigger.
- Without a headset none of this runs; the game is the game.

These do not need Oblivion's input code: the hook runs after the camera was computed, so
whatever the stick did is already in the matrix and can be read back out and replaced.
One fewer address to keep correct.

## Aiming: the history and the current solution

### The problem

`CONFIRMED`, `Scope: Bethesda legacy`

A projectile is a `TESObjectREFR` that leaves along the player's `rotX`/`rotZ`; a swing is
tested against the attacker's heading; the crosshair's activation ray is built from the
player's rotation. OBVR replaces the *camera* after the engine computed it; the player's
rotation is never touched by that, so the mouse went on aiming invisibly while the head
looked elsewhere (measured: head sweeping a sine of -0.32 to +0.24 with `rotX` at 0.0000
throughout).

The vertical half is easy: write `rotX` from the gaze (first person every frame; third
person only while aiming, because that camera is built from `rotX` and would carry the
viewpoint with it). The sideways half is the whole story, because `rotZ` is also what
walking follows, and "the character turns with the head" was offered and refused
("character bleibt").

### The evolution

| Step | Commit(s) | Approach | Observed | Outcome |
| --- | --- | --- | --- | --- |
| 1 | `385ebca` | Write `rotX` from the gaze | arrows go where you look, up and down only | kept |
| 2 | `096588b`, `9a27496` | Turn the body to the gaze while the attack control is held, take the same turn back out of the camera base so the view holds still; write a *difference*, count it once (offset accumulates, base compensated) | a written heading survives the frame (the engine adds mouse movement to it) - so it must be counted once; shots go sideways where you look | kept as the legacy machinery |
| 3 | `c79160e` | Give the turn back over a quarter second, eased, after the shot | **nausea**: the compensation is applied from the next frame's base, so a multi-frame unwind drifts the view by a frame every frame | reverted `e334cac`; `DEAD END` for the shape, not the speed |
| 4 | `4f207b1`, `a90af51`, `34078c7` | Give the turn back in one frame, after the game says the attack is over (`currentAction` leaves `AttackBowArrowAttached`), with a 1.5 s safety limit; turn only for the shot (`AimTurnOnShotOnly`), not during the draw | works; the arrow spawns several frames after release, so straightening on the release frame would send it forwards | kept as fallback |
| 5 | `ceb91db`, `6923ab0` | Turn the first-person arms node at render time so the bow visibly points at the gaze during the draw | the engine may or may not rewrite the node per frame, so the write is remembered and self-corrected | kept (`AimWeaponFollowsGaze`) |
| 6 | `60e4561`, `4c988a4` | Measure the viewpoint: the body's turn walks the first-person eye along a 4.6-unit arc (1.7 units of step, eighteen times in nine shots) | the "aim jump" was a displacement, not a rotation | `AimArcCorrection`, kept |
| 7 | `1897c52`..`f5de113` | Spells: the cast hook at `CastMagicItem` fires at the *start* of a 53-frame animation; the turn is armed and made a measured lead time before the animation ends; animation length is measured in frames because 90 Hz is not 60 | works, fiddly | kept as fallback |
| 8 | `1314478`, `73cb288` | Third person: the chase camera is on a sphere about a pivot above the feet, eased; the easing rate is stepped by the **physics at 60 Hz** under a 90 Hz renderer, so it is read off the camera every frame rather than modelled | jitter gone | kept |
| 9 | `6661d55`, `e29bb0a`, `079cec4` | **Set the aim inside the engine call that reads it** (`AimAtSource`): an around-call stub on the player's attack update (`0x005FCAB0`, called from the input handler at `0x00672E0D`) and on the projectile factory call sets `rotX`/`rotZ` to the gaze on the way in and restores them on the way out; no frame ever sees the change | headset-confirmed `OBVR-aim-at-source-works.log`; walking never pulled; the turn machinery stands down while the hook is in | **current** |
| 10 | `450c695`, `c1141af` | Third-person visuals: `Bip01 Spine2` turned after animation by a percentage of the gaze, `Bip01 Head` to the full gaze, so the rendered attack points where the shot goes | headset-confirmed (#38) | kept |
| 11 | `b380abe`, `2fe65e1` | Third-person activation ray direction replaced by the gaze (`WorldPickHook`, origin stays the engine's player-safe one); pick normalisation divides by the believed size | tooltips and activation follow the gaze | kept |

### Why step 9 needed three sites

The first reading of the binary placed the arrow construction, `UseActiveMagicItem` and
`AttackHandling` inside the animation-key handler `0x005FC890` (the virtual on the actor's
MagicCaster base). Wrapping the player's vtable slot logged swings and casts, and the bow
never entered it: that function ends at its `ret 8` and is only the wrapper an NPC's
animation reaches them through. The player's route is `PlayerCharacter::HandleInput`
calling the attack update `0x005FCAB0` directly, found by writing the stack's return
addresses from inside the wrapped projectile factory (`LooksLikeReturnAddress`,
`core/StackScan.h`). Two headset runs, two corrections, all in `GameAddresses.h`.

Reusable lesson (`Scope: Legacy games`): the vtable route an NPC takes is not the route
the player takes. A stack probe from a site that is certainly reached (the factory) names
the caller that matters.

### What the current solution leaves standing

- The legacy turn machinery (`AimTurnOnShotOnly`, `AimReturnOnRelease`, the cast window,
  `ChaseStep` compensation) remains in the code and owns the aim again if the around-call
  slot does not hold what this build expects (`AimTurnOwnsAction`). It is exercised by
  `frame_logic_test`; it is not what a headset sees in the shipped configuration.
- The unarmed third-person body follows the gaze only with
  `ThirdPersonBodyFollowsGazeUnarmed=1` (off).
- `WeaponState` (weapon out) is read as a byte at `MiddleHighProcess+0x114` rather than
  through the virtual at index 0xBE, because a wrong vtable index crashes and a wrong
  byte offset only misplaces a crosshair.
- A spell counts as a weapon for the crosshair's "only when needed": `UNVERIFIED`
  (`PlayerAim.h` says so explicitly).

## The crosshair's depth

`CONFIRMED`, `Scope: General VR` (the vergence arithmetic) / `Scope: Oblivion` (the source)

A crosshair in the flat HUD overlay hangs at one distance; anything not at the converged
depth is seen double. The vergence error between a crosshair at c and a target at d goes
as IPD (1/d - 1/c): bounded as d grows, divergent as d shrinks - a fixed quad is fine in
the distance and always fails close up. Oblivion records the activatable reference under
the crosshair (`HUDInfoMenu::crosshairRef`, within `iActivatePickLength` = 150 units =
2.14 m), which is exactly the range where a fixed depth fails. The depth taken is the
reference origin's depth **along the view axis** (an actor's origin is between its feet; a
straight-line distance doubles the crosshair on a face a metre away), eased at
`CrosshairDepthSpeed`, snapping on target acquisition. See
[ui-and-hud.md](ui-and-hud.md#the-crosshair) for how the pixels get there.

## The visible body

`BUILT, NOT SEEN IN A HEADSET` (2026-09-07). `[Body] Visible=1` shows the player's own
body in first person: `PlayerBody.cpp` takes the hidden bit (`flags & 1`) off the
third-person root each frame, stands the skeleton (`Bip01`, not the player root - the camera
hangs under the root as well, and moving the root moved the view with the body, which the
first headset run felt as a lurch on every look up or down) where `Bip01 Head` plus an eye offset
(`EyeForward`/`EyeUp`, Enhanced Camera's 14/6) lands on the cyclopean camera
(`BodyPlacement.h`, absolute rather than Enhanced Camera's running increment, so it
cannot drift), propagates with `kUpdateNodeTransforms`, and then collapses `Bip01
Head` and both clavicles by recomputing their world transform at local scale zero and
putting the local scale back (Enhanced Camera's `UpdateSkeletonNodes(0)` /
`ApplyAnimData` / `UpdateSkeletonNodes(1)` sequence). Runs first in
`BeforeFirstScenePass`, gated by `VisibleBodyWanted` (on, first person, no menu). Two
POV-switch jumps (`0x00664FC6`, `0x00664FFB`) are NOPed at start after Enhanced Camera
so the body exists after a load in first person; bytes verified before writing.

Open until a headset run: whether the engine animates the third-person skeleton in
first person without Enhanced Camera's other hooks (it animates both skeletons through
`0x006043DC`, which is why Enhanced Camera hooks there - `LIKELY` yes); whether the
inventory paperdoll survives the collapse (the local scale is restored for that
reason); the horse, which Enhanced Camera excludes; and the head-tracking IK at
`0x00603AAA`, which Enhanced Camera disables for the player in first person and OBVR
leaves running (the head bone is collapsed anyway, but its position feeds the
placement).

## The hand-tracked mode (standing experience)

`EXPERIMENTAL` throughout: built, tests green, **not seen working in a headset, not
working as intended**, off by default (`[Hands] Enabled=0`). `docs/hand-tracked-mode.md`
is the ladder; this is the map.

- Input: the **legacy** OpenVR controller path (`GetTrackedDeviceIndexForControllerRole`,
  `GetControllerStateWithPose`, seated space) in `OpenVRBackend::ReadHand`. Whether
  current SteamVR delivers button state to an application with no action manifest is the
  first thing the log answers; the fallback is `IVRInput` with an `actions.json`, which is
  **not built**. `UNKNOWN`.
- Decisions: `vr::HandMode` is pure over one frame of inputs (poses, buttons, menu state,
  cursor) and returns everything: aim, arms, controls to press, wrist placements, laser,
  poke. `hand_mode_test` covers it.
- Acting: `UpdateHandMode` presses the game's own bound keys through `keybd_event` scan
  codes and `mouse_event` (edges only, everything released on stop); the aim goes through
  the same `AimSourcePose` the head uses; the arms are placed at render time
  (`PlaceFirstPersonArms`); the hand bones are pinned to the controllers
  (`BonePin.h`, `HandBones`), the animated arms hidden by node name (`FirstPersonHide`,
  `"Arms"`); strikes by motion test the blade against the engine's high-process actor list
  and call `Actor::AttackHandling` with an explicit target (`MeleeHits`, `MeleeHit.h`);
  the grab update runs with the hand's direction and distance swapped in (`SetGrabAtHand`).
- Menus: the HUD and Tab menus can hang on the wrists (`SetOverlayTransformTrackedDeviceRelative`),
  a laser from the other hand walks the game's cursor to the hit, the trigger clicks, the
  sticks scroll; `ControllerMenus=1` (off) does the same on the big quad with the mode off.
- Not built: snap turn, teleport, room-scale locomotion, physical object interaction,
  finger tracking, a body.

Every number in it is a starting point. A future agent picking it up should read
`docs/hand-tracked-mode.md` "The first headset session, in order" and do exactly that.
