# Menus in front of the body

Nadi, 2026-11-28: "in-game alle Messageboxen und größeren Messageboxen ... sich automatisch
immer vor dem Spielerkörper ausrichten, auch wenn der ausgeblendet ist. Ähnlich zum
Hauptmenü, dasselbe für das Escape-Menü". Answers to the proposal: glide rather than
jump; every menu on the big quad when the tablet is off; the height as before, the glide
sideways only.

## Before (read in the code)

- With `Menus=world` and `HudAnchor=world` every game menu that is not on the tablet or the
  wrist hangs on the one 2D layer (`render/HudLayer`). As a menu opened, the layer's room
  anchor was dropped (`g_hudLayer.ResetAnchor()` in `MaybeSubmitOverlays`) and the next
  submit took the head's pose, levelled: the menu stood where the head looked at that
  moment, until it closed. A box that came up while the player looked aside hung aside;
  turning round left it behind.
- The main menu and the rest of the cinema screen follow the head instead
  (`vr/FlatFollow.h`: 30 degrees or half a metre away for a second, never under the laser).

## Now (built 2026-11-28)

- `vr/MenuFacing.h` (pure) and `camera/MenuFacingStep.cpp` (the caller, once a frame after
  the dialogue's and the container's placements).
- **The body's heading** is the torso's: `game::StepTorsoYaw` with `[Body]
  TorsoFollowDegrees` and `TorsoFollowSeconds`, from the head's level heading
  (`vr::LevelHeadingOf`, the same rule as `vr::BodyFrame`). It comes from the headset alone,
  so it is the same with the body hidden. Stepped every frame, menu or not, on its own
  counter clock (the camera hook's delta is not taken on menu frames without a world
  render); walking (`FeetStand`, `RoomFollowWalking`) only outside menus.
- **Placed** level ahead of the eyes along the body's heading, at the eyes' height then, as
  a menu opens - the menu mode's own start, a menu that comes onto the big quad from a place
  of its own, the recenter.
- **Glides** after the body while it is up: once the body is more than `MenuFollowDegrees`
  (30) off the menu, `vr::StepViewAnchor` (the HUD rows' glide, time constant
  `MenuFollowSeconds`, 0.4) turns it round the player on the level until within
  `kViewSettledDegrees` (2). The height stays as placed. A laser on it holds it, as the main
  menu's. The anchor is handed to the layer only on a frame it changed.
- **Which menus** (`vr::MenuFacingFor`, by the interface manager's stack): everything on the
  big quad - a message box, the Generic boxes (the tutorial's help, OBVR's own windows), the
  Esc menu and what opens from it, sleep and wait, the level-up, the journal's pages with the
  tablet off, a container's menu without its panel, a mod's menus. Left alone: a
  conversation and what opens from it (Dialog, Persuasion, Training, Negotiate; the Esc menu
  over a talk too), a container's panel, the barter, the tablet and the menu wrist (the
  caller's `ownPlace`), and the main menu, a load, the credits and the race menu (as before).
- **Settings**: `[Render] MenusFaceBody=1` (settings menu: "Menus in front of you"),
  `MenuFollowDegrees=30` (0: stands where it opened), `MenuFollowSeconds=0.4` (0: at once).
  `MenusFaceBody=0` is the behaviour before, unchanged.

## Verified

- `menu_facing_test` (2026-11-28, Linux, clang -m32): which menus face the body; opens
  along the body's heading with the head 30 degrees aside and looking down; level, at the
  eyes' height; a 25-degree glance for 0.6 s moves nothing; a turn to 90 degrees glides
  (part of the way after a few frames), settles within the settle of the body, height kept
  after standing up 0.2 m; the laser holds it and stops a glide midway; off, cinema, a
  conversation, the main menu, no menu and a place of its own hand nothing over; back on
  the big quad it is placed afresh; the next menu opens before the body as it is then;
  `MenuFollowDegrees=0` stands, `MenuFollowSeconds=0` jumps; the recenter, a menu's
  start and a menu opened by the caller place afresh; no head keeps it; walking turns the
  body; the matrix's level heading is the quaternion's. Also built at -O2: passes.
- The whole test suite: six failures, the same six in a run before the change was built
  in (eye_geometry, hand_hud, dismember, finger_skin, touch_handover, mirror_glass_map;
  none of them includes a file this change touches); nothing new.
- The DLL was not built here (no Windows SDK in the cloud session); `CameraHook.cpp`,
  `MenuFacingStep.cpp`, `Config.cpp` and `ConfigDebug.cpp` were checked with
  `clang++ -m32 -fsyntax-only -DOBVR_NO_WINSDK` (no errors in OBVR's sources; a
  misspelt member in the new call was caught by the same check).

## Not yet verified

- In the game: `tools/hand-scripts/menu-facing.txt` (the Esc menu with the head 30
  degrees aside, the glide after a turn to 100 degrees, the journal with the tablet off)
  has not run. The yaw sign of the script's head against the log's is not checked; the
  expects do not depend on it.
- In the headset: how the glide feels, and whether 30 degrees and 0.4 s are right.

## Cost

- Per frame: one level heading, one torso step, in menus a read of the ten-entry menu stack
  and the glide step. Measured on the development host (clang -m32 -O2, a 2-million-frame
  loop with the head swinging 60 degrees, the pose built in the loop included): 0.21 us a
  frame without a menu, 0.48 us with one up and gliding half the frames. Not measured in
  the game; the profiler run (`ini Performance Enabled=1`, `--exclusive`) is still to do.
- While it glides (about a second after a turn) the layer's transform is handed to the
  compositor every frame (`SetOverlayTransformAbsolute`); standing, never. That call's cost
  is not measured.
