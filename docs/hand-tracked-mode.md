# The hand-tracked mode

OBVR today is a head-tracked mod: the headset moves the camera, the gaze aims, and
everything else stays with the mouse, keyboard or gamepad. The hand-tracked mode is the
other shape - motion controllers in both hands, the weapon in the right one, menus on the
wrists, objects picked up by hand - behind one toggle, `[Hands] Enabled` in `OBVR.ini` and
"Hand tracking" in the headset settings menu. This document is the ladder from the first
rung to the last, with what each rung needs from the engine and what is already known.

Everything below the line marked "built" exists on the `hand-tracked-mode` branch. Nothing
above it has been started. The order is the order the rungs depend on each other.

## Rung 1 - the hands are read, the weapon hand turns with the controller (built)

- `OpenVRBackend::ReadHand` reads a controller by role (`GetTrackedDeviceIndexForControllerRole`)
  and takes its pose and legacy state in one call (`GetControllerStateWithPose`, seated
  universe, the same the head is read in). Indices 18 and 37/38 of `IVRSystem_026`'s
  function table, counted in `openvr_capi.h`.
- The first-person arms node ("Player1stPerson") is turned by the difference between the
  right controller's heading and the head's, on top of the camera. This reuses the
  machinery that already turns the bow to the gaze (`TurnFirstPersonArms`), so the bow, the
  spell hand and the sword visibly follow the controller left and right.
- Verified: the pure parts (`hand_input_test`). Not yet verified in a headset: the sign and
  scale of the turn on a real controller, and whether the legacy controller state is
  delivered to an application without an action manifest on current SteamVR (the header
  documents `GetControllerState` as the legacy path; whether SteamVR 2.16 still feeds it
  without an `actions.json` is something only a run can say - if it does not, the next
  step is the IVRInput action API, which needs a manifest next to `OBVR.dll`).

**What to look at first in the headset:** with `Enabled=1`, draw a bow in first person and
move the right controller left and right. The bow should follow; if it turns the wrong way
the sign in `CameraHook.cpp` at the "hand-tracked mode's first rung" comment flips.

## Rung 2 - the hand in space: pitch and position

The arms node gets the controller's full rotation (pitch and roll as well as heading) and
its position relative to the head, in game units (`UnitsPerMetre`). The engine rewrites the
node every frame from the animation, so the write stays in the render pass where the
heading write already survives (measured: the node's world transform moved). Open
questions: the node's local axes against the controller's, and how far the arms may leave
the animation's pose before the mesh tears - the weapon and the hand are one skinned mesh
with the arm.

## Rung 3 - the shot goes where the hand points

The aim is already set at the source (the attack update and the projectile factory read a
heading OBVR hands them). Rung 3 replaces "the gaze" with "the right hand's pointing
direction" in that hand-over for bows and spells. No new engine sites: `AimAtSource`
already owns both.

## Rung 4 - the bow: reach back, draw, loose

- Draw: the trigger on the right hand held is the attack control held (the input handler
  reads a movement/attack flags word; the attack update is already wrapped, so the flag is
  set from there).
- Loose: trigger released is the control released - vanilla already fires on release.
- "Reach back for an arrow": a gesture - the right hand near the head's back-right - that
  the draw is gated on. Cosmetic in the engine's terms, the arrow still comes from the
  quiver; the gate is OBVR's.

## Rung 5 - melee: swings that register by speed

The attack update `0x005FCAB0` is wrapped already. A swing is the right hand's speed over a
few frames above a threshold; the light attack is the attack control tapped, the heavy
attack the control held for the engine's power-attack time. What the engine needs is the
control state, which OBVR feeds through the same flags word; what it does not know is
where the blade is, so hit detection stays the engine's swing animation. Whether the
animation can be shortened or skipped so the hit lands when the hand lands is the open
question of this rung - the animation's length in frames is stable (measured for casts),
so the timing can be planned.

## Rung 6 - the shield: raised is blocking

Block is a control. The left hand held up in front of the chest, roughly vertical, holds
the block control. A threshold on the left controller's pitch and height relative to the
head.

## Rung 7 - spells from the hand

Cast is a control and the cast hook at `0x00699190` already turns the heading for one call.
Rung 3 gives it the hand's direction; rung 7 puts the cast control on the left hand's
trigger when no shield is up, or on a button.

## Rung 8 - menus on the wrists, unpaused

- The HUD (health, magicka, fatigue, compass) on the right wrist and the Tab menu on the
  left: `IVROverlay::SetOverlayTransformTrackedDeviceRelative` hangs an overlay on a tracked
  device, which is exactly a wrist. The HUD overlay exists and is captured per frame; the
  Tab menu is the same texture on a menu frame. Two overlays, two transforms.
- Unpaused: `Render.UnpausedMenus` already keeps the world running behind the player's
  own menus.
- The cursor: the game's menu cursor follows the mouse. A laser from the right hand to the
  left wrist's quad, mapped into the texture's pixels, drives the same cursor position the
  mouse does (the cursor pick normalisation at `0x00701540` is already hooked for the
  hover offset, and knows the believed size).
- The quick menu: the quick-keys wheel is a menu like the others (id 0x416) and can hang on
  the right wrist the same way.

## Rung 9 - picking things up and throwing them

Vanilla grabs with the activate key held ("Z"/grab) and the engine moves the object with
a Havok constraint towards a point in front of the camera. Rung 9 makes the grab a button
on the hand and the target point the hand's position, so the object follows the hand, and
sets its velocity from the hand's on release, so it is thrown. The engine's grab code is
the one site not yet located: it lives in the player's activate handling and the
`bhkRigidBody` it constrains. Finding it is the first step of this rung, with the usual
two-source standard.

## Rung 10 - the body

Third person and the visible body are not part of this mode's first version. In first
person the arms are the body; the rest of the skeleton is never seen.

## What stays as it is

Locomotion stays on the left thumbstick (the engine's movement flags, already understood),
turning on the right one or on the head. Nothing in this mode touches the render pipeline:
stereo, the bone lock, the menus in the world, the crosshair all carry over unchanged.
