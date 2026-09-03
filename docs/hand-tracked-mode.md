# The hand-tracked mode

OBVR today is a head-tracked mod: the headset moves the camera, the gaze aims, and
everything else stays with the mouse, keyboard or gamepad. The hand-tracked mode is the
other shape - motion controllers in both hands, the weapon in the right one, menus on the
wrists, the world driven from the sticks - behind one toggle, `[Hands] Enabled` in
`OBVR.ini` and "Hand tracking" in the headset settings menu.

This document is the ladder, rung by rung: what is built, how it works, what it assumes,
and what the first headset session has to look at. **Nothing on this branch has been in a
headset.** Every rung was built from the engine facts already established and from the
OpenVR header, and every number is a starting point.

## How the mode is wired

- `vr::HandMode` (`src/vr/HandMode.h`) takes one frame of inputs - the head pose, both
  controllers, the frame time, whether a menu is up, the game's cursor - and decides
  everything: the aim, the arms, the controls, the wrists, the laser. It is pure apart
  from its few frames of state, and `hand_mode_test` covers every decision.
- `UpdateHandMode` in `CameraHook.cpp` runs it once per Present (so it runs on menu frames
  too, where the camera pass does not), presses the controls, moves the cursor, and hangs
  the overlay on a wrist. The camera pass reads its aim; the render pass places the arms.
- Controllers are read by role through `OpenVRBackend::ReadHand`
  (`GetTrackedDeviceIndexForControllerRole`, `GetControllerStateWithPose`, seated space).
  This is the **legacy input path**. Whether current SteamVR delivers button and axis
  state to an application with no action manifest is the first thing the log answers:
  "Hands: right controller tracked" with no reaction to the trigger means the pose comes
  and the buttons do not, and the next step is `IVRInput` with an `actions.json` next to
  `OBVR.dll`.
- The controls are the game's own: `game::ApplyHandControls` presses the keys and mouse
  buttons the player has bound (`[Hands] *Key`, vanilla defaults from Bethesda's control
  list) through `keybd_event` with scan codes and `mouse_event` - the route the input
  harness already proved reaches DirectInput. Only edges are sent, and everything is
  released when the mode stops or a controller is lost.

## Rung 1 - the hands are read, the weapon hand turns with the controller

Built. The arms' heading follows the right controller's heading relative to the head.

## Rung 2 - the hand in space: pitch and position

Built. `PlaceFirstPersonArms` applies the head-relative rotation of the right controller
and an offset in game units to the first-person arms node, on top of the animation, with
the same remember-and-restore base as the turn. The offset is the controller's distance
from a rest position (`RestHand*`, metres from the eyes) scaled by `UnitsPerMetre`.

Assumption to check: the node's parent space is the camera's. The log says
"First person arms: placed in the space of parent ..." once - if the parent is not the
camera or the arms fly off, `ArmsFollowPosition=0` keeps the rotation alone, and
`ArmsFollowPitch=0` falls back to the heading. Also to check: how far the skinned arm mesh
tolerates the offset before it tears at the shoulder.

## Rung 3 - the shot goes where the hand points

Built. `SetAimSourcePose` gets the right hand's heading (the head's plus the hand's turn
from it) and the hand's own pitch instead of the gaze, so bows, spells and swings are
resolved along the controller inside the engine calls that read the aim - no new sites.

Sign to check: aim a drawn bow at something with the controller and see whether the arrow
goes left or right of it. The heading conversion is the head's own
(`FromOpenXR`, `HeadingOf`); the pitch is `PlayerPitchForGaze(SinPitchOf(...))`, the same
function the gaze uses.

## Rung 4 - the bow: draw and loose

Built. The right trigger past 55 % holds the attack control; releasing it below 35 % lets
go, and vanilla looses on release. The reach-back gesture (`ReachBack*`) is detected and
logged but not yet a gate on the draw: making it one is a one-line change in
`PlanHandControls` once the log shows the gesture firing where it should.

## Rung 5 - melee: swings that register by speed

Built. The right hand's speed relative to the head crossing `SwingLight` starts a swing;
when the hand slows to half of that the swing ends, light or heavy by its peak against
`SwingHeavy`. A light swing taps the attack control, a heavy one holds it for
`HeavyHoldSeconds` - the engine's own power attack. Hit detection stays the engine's
animation, which starts when the control goes down, so the blade lands a little after the
hand. The log says "Hands: a light/heavy swing" for the first twenty.

To tune: the two speeds. Walking does not swing, because the speed is measured relative to
the head.

## Rung 6 - the shield: raised is blocking

Built. The left hand no lower than `BlockMinUp` below the eyes and at least
`BlockMinForward` in front holds the block control. "Hands: the left hand is up - blocking"
in the log.

## Rung 7 - spells from the hand

Built. The left trigger holds the cast control; the cast leaves along the right hand
(rung 3). A left-handed caster who wants the cast along the left hand is a later option.

## Rung 8 - menus on the wrists, unpaused, with a laser

Built. `HudLayer::SetWristPlacement` hangs the layer's overlay on a controller with
`SetOverlayTransformTrackedDeviceRelative`: on the right wrist on a world frame (the HUD,
`WristHudWidth`), on the left wrist on a menu frame (the Tab menus, `WristMenuWidth`),
placed `WristUp`/`WristBack` from the controller and tilted `WristTiltDegrees` towards the
eyes. Leaving the wrist hands the quad back to the head or room anchoring.

The laser: on a menu frame the right controller's ray is intersected with the left wrist's
quad, the hit mapped into the pixels the quad shows (the 2D's believed size), and the
game's cursor - read from the InterfaceManager's position fields - is walked towards it
with relative mouse motion (`LaserGain`, `LaserMaxStep`). The right trigger clicks. The
left menu button closes the menu, the right one is Escape.

`Render.UnpausedMenus=1` keeps the world running behind the player's own menus.

To check: the wrist transform's orientation (the quad should lie on the forearm like a
watch face, tilted up towards the eyes); the laser's convergence (oscillation wants a
lower gain); and whether the cursor fields are in the believed pixel space, which the
cursor probe's numbers already suggested.

## Rung 9 - picking things up and throwing them

Half built. The right grip holds vanilla's grab control (Z): the object lifts and hovers in
front of the view, as it always did, and the left grip activates. Following the hand and
throwing are not built: they need the engine's grab code - the point the grabbed
`bhkRigidBody` is constrained towards and the place its velocity is set on release - and
that site has not been located. Locating it (two sources, as always) is the first step;
the aim hand-over pattern (`AimAtSource`) is the shape the fix would take: on the way into
the grab update, put the hand's position where the engine reads the camera's.

## Rung 10 - the body

Not built. In first person the arms are the body. Third person keeps the head-tracked
behaviour with the hand-tracked mode on.

## Locomotion and the rest

Built: the left stick walks (WASD with `StickDeadZone`), the right stick turns
(`TurnSpeed` mouse pixels per frame), right A jumps, left A sneaks, the right stick click
readies the weapon, the left stick click opens the quick menu (F1).

Nothing in this mode touches the render pipeline: stereo, the bone lock, the menus in the
world, the crosshair all carry over unchanged.

## The first headset session, in order

1. `Enabled=1`. The log must say "Hands: right controller tracked, left controller
   tracked". If not, the roles are not assigned in SteamVR or the legacy path is dead.
2. Pull the right trigger with a bow drawn: the bow must draw. If nothing happens while
   the pose tracks, the legacy button path is dead - rung 0 becomes `IVRInput`.
3. Move the right controller: the arms should follow. Wrong direction: the sign at the
   "hand-tracked mode's first rung" comment. Torn or flying: `ArmsFollowPosition=0`.
4. Shoot at a wall: the arrow should land where the controller points, left/right and
   up/down.
5. Raise the left hand: "blocking" in the log and the shield up in the picture.
6. Swing: "a light swing" / "a heavy swing" in the log and the animation playing.
7. Open the Tab menu: it should hang on the left wrist; point the right hand at it and
   the cursor should walk to the laser; the trigger clicks.
8. Look at the right wrist: the HUD.
