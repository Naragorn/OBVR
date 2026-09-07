# The hand-tracked mode

OBVR today is a head-tracked mod: the headset moves the camera, the gaze aims, and
everything else stays with the mouse, keyboard or gamepad. The hand-tracked mode is the
other shape - motion controllers in both hands, the weapon in the right one, menus on the
wrists, the world driven from the sticks - behind one toggle, `[Hands] Enabled` in
`OBVR.ini` and "Hand tracking" in the headset settings menu.

**Under construction.** The mode is off by default, the first-start walkthrough shows the
"Standing Experience" but refuses it, and `ControllerMenus` is off with it: nothing here
has been seen working in a headset, and it is not working as intended. Switching it on in
`OBVR.ini` or the settings menu is at your own risk; reports from doing so are welcome.

This document is the ladder, rung by rung: what is built, how it works, what it assumes,
and what the first headset session has to look at. Every rung was built from the engine
facts already established and from the OpenVR header, and every number is a starting
point.

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
go, and vanilla looses on release. The reach-back gate is built behind
`BowNeedsReachBack` (off by default): with it on, the trigger draws only after the right
hand has been behind the head since the last release - one reach per arrow. Switch it on
once the log shows "Hands: the right hand is reaching back" firing where it should.

## Rung 5 - melee: swings that register by speed

Built. The right hand's speed relative to the head crossing `SwingLight` starts a swing;
when the hand slows to half of that the swing ends, light or heavy by its peak against
`SwingHeavy`. With `MotionHits=0`, a light swing taps the attack control, a heavy one holds
it for `HeavyHoldSeconds` - the engine's own power attack - and hit detection stays the
engine's animation, which starts when the control goes down, so the blade lands a little
after the hand. The log says "Hands: a light/heavy swing" for the first twenty.

To tune: the two speeds. Walking does not swing, because the speed is measured relative to
the head.

### Strikes by motion (`MotionHits=1`, the default)

With a blade, a blunt weapon or bare fists drawn, the swing is the attack and nothing is
pressed: no attack control, no animation, and the trigger does not attack (it still draws a
bow). While the hand is swinging, `game::StrikeByMotion` draws the blade - the hand's
position along the controller's pointing axis for the weapon's reach, the reach being the
engine's own arithmetic (the weapon's reach through `fCombatDistance`, or the hand's reach,
times the actor's scale) - and tests it against every actor in the engine's high-process
list, the same list the engine's own target search walks. Each living one whose bound sphere
the blade passes within `HitBoundFactor` of the radius plus `HitPadUnits` is handed to
`Actor::AttackHandling` (0x005FEBF0) with that actor as the target and the power-attack flag
set when the swing's peak has crossed `SwingHeavy`; the engine does the rest - damage, block,
sneak attack, enchantment, crime, script events. One strike per body per swing
(`MeleeHit.h`, tested). The evidence for the function and its arguments is in
`GameAddresses.h` at `kAttackHandling`.

What to look at first: "Hands: strikes by motion armed" names the weapon type, the reach in
units and the reach setting's name - which should read `fCombatDistance`; anything else
means the setting address is not what it was read as, and the reach is still the engine's
helper's answer. Then "Hands: the blade struck ..." per strike, with the distance from the
body's centre and the bound radius the decision used. Strikes that land too easily want a
smaller factor; swings through a body that do nothing want a larger one, or a check of the
hand's yaw calibration, since the blade follows the controller's forward.

Not built: the swing's sound and the fatigue an attack costs (both tied to the animation),
and NPCs block less, since they read the player's attack animation to decide when.

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

### The controllers on every menu, from the main menu on (`ControllerMenus=1`)

The target: SteamVR up, Oblivion started from its menu with the controllers in hand, and
nothing needs a keyboard or mouse from there to the game - the main menu, the walkthrough,
loading, character creation, the Tab menus, OBVR's own menu.

- **With the mode off** (`Hands.Enabled=0`, which is what the walkthrough starts with) the
  controllers still reach the menus: `UpdateMenusOnly` in `HandMode.cpp` runs instead of
  the mode, and presses nothing in the world - no aim, no arms, no gestures, no keys.
- **The laser points at whichever quad shows the game's menus.** On a wrist as before, and
  otherwise at the big quad on the head or in the room: `HudLayer::QuadInTracking` hands
  the quad's pose back in tracking space (the head's pose composed with the head-relative
  offset, or the room anchor's), `QuadFromPose` makes the quad, and the same `LaserOnQuad`
  and `PokeOnQuad` as on the wrist walk the game's cursor to the hit and click. Before a
  game is loaded (`game::PlayerInWorld`, the reference's parent cell) there is no wrist, so
  the main menu stays on its big picture whatever `WristMenu` says.
- **The laser meets the cinema picture when there is no quad** (2026-09-07). The main menu,
  character creation, loading screens and films are flat frames: the back buffer in both
  eye textures, centred on each eye's optical axis, submitted with a frozen levelled pose -
  a picture at infinity, not a quad in the room, which is why the beam used to end in the
  air there. `HeadsetRenderer::FlatPictureInTracking` now hands the anchor pose's axes, the
  picture's angular half-extents (its eye-texture pixels times tangents per pixel, from
  `EyeMirror::FlatPlacement` and the left eye's projection) and the frame window it shows
  (the flat source crop, in the cursor's pixel space) to `HandModeFrame::flat`;
  `LaserOnFlatPicture` meets the hand's ray with a stand-in plane two metres ahead of the
  anchor, where the beam ends, and takes the pixel from the HEAD's view of that point -
  tangents against the half-extents - so the cursor lands where the wearer sees the beam
  end. No finger press there: nothing is at arm's length.
- **The hand that pulls its trigger holds the pointer** (`HandMode::StepPointerHand`): a
  pull on the left takes the laser to the left hand, and that pull is also a click; a pull
  on the right takes it back. Both pulled on one frame changes nothing; a hand that is not
  tracked cannot hold it. `PlanHandControls` clicks from the pointing hand
  (`HandFrameInput::pointRight`). The wrist menu is the exception: there the hand without
  the menu points.
- **The beam** is drawn: `render::LaserLayer`, an overlay of raw pixels (`SetOverlayRaw`,
  index 62 of `IVROverlay_028`) hung on the pointing controller along its pointing axis for
  as long as the way to the quad - `LaserBeamTransform` - with its width re-derived from the
  length so it stays the same thickness to the eye. `LaserBeam=0` switches it off.
- **The wheel:** the left stick scrolls the game's lists as mouse wheel notches - one on the
  flick, then repeating while held (`StepRepeat`), through `game::ScrollMouseWheel`.
- **OBVR's own menu and the walkthrough:** the sticks are the arrows, a trigger or A is
  Right - the next value, the next page - a grip is Left, a menu button closes the menu.
  Both sticks clicked open it, mode or no mode.
- **The laser on OBVR's own panel** (2026-09-07): `SettingsMenuLayer::QuadInTracking` hands
  the panel's quad back (the room anchor, or the head composed with the distance),
  `HandMode::SteerSettingsMenu` meets the pointing hand's ray with it and answers the canvas
  pixel; `ui::RowAtPixel` (the painter's own layout, pinned to the highlight bar by
  `menu_painter_test`) names the row, `SettingsMenu::Hover` / `OnboardingMenu::Hover` put the
  highlight there, and the pointing hand's pull is `ui::ClickActionFor` on that row - a
  toggle flips, a number steps by the half it was clicked on, a button fires, text is
  nothing (`PointAtPanel` in CameraHook). While the beam is on the panel the pull is that
  click and not the stick's Right; off the panel it is Right as before. The pull that moves
  the pointer to the other hand is spent on the move, here and on the game's menus.

What to look at first: at the main menu, a beam from the right controller onto the menu
and the game's cursor following it (the log says "Hands: the laser has the flat picture to
point at" when the picture is known); a pull on the left trigger moving the beam to the
left hand; the trigger pressing "New"; the walkthrough answering
the trigger with its next page; and in an inventory, the left stick scrolling the list. A
beam that misses the quad by a constant offset means the quad pose is not where the layer
hung it - `QuadInTracking` composes the render pose with the same head-relative transform
the layer submits, so the offset says which of the two moved.

To check: the wrist transform's orientation (the quad should lie on the forearm like a
watch face, tilted up towards the eyes); the laser's convergence (oscillation wants a
lower gain); and whether the cursor fields are in the believed pixel space, which the
cursor probe's numbers already suggested.

## Rung 9 - picking things up and throwing them

Built. The right grip holds vanilla's grab control (Z), and the per-frame grab update is
wrapped the way the attack update is: `PlayerCharacter::HandleInput` calls the grab
handler `0x00671170` every frame, which with a grab in progress calls the update
`0x0066D930` at `0x0067125E`. That update takes the grab distance at `player+0x584`, adds a
constant, and hands it to `0x005F11F0`, which builds the eye vector from the player's own
rotation fields through the same rotation makers the aim work read; the result is the
spring's target. So around that one call the rotation is swapped to the hand's direction
(the aim pose the camera pass already hands over) and `+0x584` to the hand's distance from
the eyes in game units, both put back on the way out. The held object hovers where the
hand is; swing the hand and open the grip, and it keeps the spring's velocity - vanilla's
own fling is the throw. The spring pointer at `+0x574` and its construction site are
NorthernUI's reading (`telekinesisSpring`, "assigned at 0x0066D879"); the distance field's
role is this binary's disassembly (written from the grab handler's argument at
`0x0066D8EF`, zeroed with the spring at `0x0066AD72`, read at `0x0066D9F9`).

To check: pick something up with the grip and move the hand - the object should follow
the hand, not the gaze. If it follows the gaze but not the distance, the constant added
to `+0x584` (the qword at `0x00A2FC68`) is larger than assumed; if it snaps to the face,
the distance floor (a quarter metre) is too small. The log says "Aim: the grab update
runs with the hand's direction and a distance of ..." once.

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
