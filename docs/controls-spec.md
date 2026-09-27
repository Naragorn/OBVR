# Controls in Full VR

Status: the layout in section 1 is **built** (808eecd, 2026-09-27). The target
picture in section 4 is **not built**; it is the tester's, written down on
2026-09-27. Full VR only (`[Hands] Enabled=1`). The seated mode is played
with a gamepad or keyboard and is not touched. With the mode off and
`ControllerMenus` on, the controllers act as a 360 pad (`PlanGamepadControls`,
docs/hand-tracked-mode.md).

## 1. The layout today

Everything reaches the game as the keys and mouse buttons the player has bound
(`game::ApplyHandControls`), so the game's own key map stays the authority. The
decisions live in `vr::PlanHandControls` and `vr::StepTeleportStick`; every
flow is covered by `hand_mode_test` and `teleport_test`.

### In the world

| Control | Right hand | Left hand |
|---|---|---|
| Trigger | attack (a bow draws while held); with a swung melee weapon the swing itself strikes | cast |
| Grip | grab (holding objects, docs/holding-objects-spec.md); cancels a teleport aim | grab |
| A | activate (what the right laser points at) | nothing |
| B (menu button) | Escape | Tab (the menus) |
| Stick | sideways: turn (smooth or snap); flicked up: jump; held up (0.2 s): teleport aim, released: go; flicked down: sneak | walk; pressed in: run |
| Stick click | ready / sheathe the weapon | run (with the stick pressed in) |
| Trackpad click | held: the quick menu, the ring of the eight hotkeys (4.4, built) | F1: the key the game binds to "Quick Menu" (Oblivion.ini [Controls] `Quick Menu=003BFFFF`; UESP lists F1-F4 as the journal pages) |
| Gesture | a swing strikes by motion | raised hand: block |

- Both stick clicks within a quarter second: OBVR's own menu.
- Left-handed (`LeftHanded=1`): the controllers swap roles as a whole
  (`vr::AssignHandRoles`); the table reads with "right" as the weapon hand.

### In a game menu

| Control | Effect |
|---|---|
| Pointing hand's trigger | click (on the release; a drag scrolls, sideways holds) |
| Left A, inventory only | drop the item under the cursor (vanilla's Shift + click, over four frames: `vr::StepDropPress`); one drop per press, the button has to be let go for the next |
| Left B / right B | Tab / Escape |
| Left stick | the mouse wheel |

- The drop presses OBVR's `RunKey` (Shift) for vanilla's Shift.
  - At the mode's start OBVR reads the game's `[Controls] Run` from
    Oblivion.ini.
  - It logs a WARNING when Run is not on Left Shift, or when `RunKey` is
    another key (`game::CheckDropBinding`, verdicts tested in
    `control_bindings_test`).
- **One press dropped two or three items** (first headset run, 2026-09-27).
  - The inventory flickers to the HUD and back after a drop.
  - The press was an edge on "A held and the inventory open", so every
    flicker fired it again.
  - It is now the button's own edge.
  - The drop's click also goes past the laser's touch-screen press, which
    would have turned it into a click on the next frame.

### In OBVR's own menu and the walkthrough

The sticks are the arrow keys, a trigger or an A is Right, a grip is Left,
a menu button closes. The laser clicks rows.

## 2. Why it is this way

- **Jump on the right stick up, the teleport on a hold of it.**
  - Skyrim VR puts the jump on the right stick up in its standard Index layout
    and in VRIK's, the one most mod lists use (reddit r/ValveIndex f1nqvd,
    r/ValveIndex cwtppr "Cargo-Cult", r/skyrimvr clsav7 by VRIK's author;
    Nexus article 3842, Auriel's Dream).
  - Tried first on 2026-09-27: jump on the left A (the left thumb walks), then
    on the right A (taken by activate). Both were awkward.
  - Skyrim VR's own teleport replaces walking (`bDirectMovementWithWands`, one
    or the other, UESP Skyrim:Skyrim VR). Players asked for both at once
    (reddit r/skyrimvr 8cf15h, 8kjm6c). OBVR has both.
  - The cost: the jump comes on the release, a fraction later than a key.
- **The cone and the threshold on the stick.** In Half-Life: Alyx the teleport
  is the right stick forward and players teleport by accident while turning
  (reddit r/ValveIndex ikzuca). A push counts only past 0.8 and within 30
  degrees of straight up (`TeleportStickStart`, `TeleportStickConeDegrees`).
- **Activate on the right A** as in Skyrim VR's standard layout and
  Cargo-Cult. VRIK puts it on the grip, which here is the grab: a door and a
  cup would be on the same button.
- **The left A never takes an object.** The grips do that; a second button
  that picked things up would compete with them.
- **Yield needs no button.** Vanilla's yield is block and activate together,
  facing the attacker (UESP Oblivion:Controls): the raised left hand and the
  right A. Not yet tried in the headset.
- **The dodge roll needs no button.** Block and jump (from Acrobatics
  Journeyman): the raised hand and a flick up. Not yet tried.
- **Quick save and load need no button**: they are in the Escape menu.

## 3. Vanilla's controls and where they are

From UESP, Oblivion:Controls.

| Vanilla (PC) | Full VR | |
|---|---|---|
| Move, run, jump, sneak | left stick, its click, right stick up, right stick down | built |
| Attack, cast, block | right trigger or a swing, left trigger, raised left hand | built |
| Ready weapon (F) | right stick click | built; to become gestures (4.1-4.3) |
| Activate, grab | right A, grips | built |
| Journal / menus, pause | left B, right B | built |
| Drop (Shift + click) | left A in the inventory | built |
| Yield, dodge | gesture + button, see section 2 | untested |
| Hotkeys 1-8 | right trackpad held: the quick menu | built (4.4) |
| Wait (T) | - | missing |
| Quick save / load (F5 / F9) | Escape menu | enough |
| Always run, auto move | - | not needed in VR |
| Change view (R) | - | Full VR stays in first person |

## 4. The target picture (not built)

The tester's aim: the weapon is drawn by reaching for it, as with a real one.

### 4.1 Sword from the left hip

- **Wanted.** The right hand at the left hip and a grip: the sword is drawn.
- **Design.**
  - A zone at the left hip in the body's frame: the right controller within
    about 20 cm of a point low on the left side, below the waist.
  - The grip closing there readies the melee weapon.
  - Drawn, the same reach and grip sheathes it again (the tester, 2026-09-27:
    "ja, soll es").
- **To find out.**
  - How the body's frame is known: the head's position and heading minus a
    neck-to-hip offset, or the arms' own hip bone. The first needs no engine.
  - The grip there must not also grab: the grab is suppressed while the hand
    is in a holster zone.
  - Oblivion has one weapon slot. If a bow is equipped, "the sword" is the
    last melee weapon the player had: it must be remembered and equipped
    through the engine (xOBSE's EquipItem path) before the ready.

### 4.2 Bow from the left shoulder

- **Wanted.** The left hand at the left shoulder and a grip: the bow is drawn.
- **Design.** As 4.1, a zone behind or above the left shoulder, the left grip;
  the same gesture puts the bow away again.
- **To find out.**
  - The same one-slot question: the last bow is remembered and equipped.
  - Conflict: the arrow's reach back over the right shoulder
    (`BowNeedsReachBack`) is the right hand; the bow's zone is the left hand
    at the left shoulder, so they do not share a zone. To confirm in the
    headset that one is not taken for the other.

### 4.3 Fists by making a fist

- **Wanted.**
  - A fist with the weapon hand: hand to hand is ready, and striking is all
    that is left.
  - The hand opened: hand to hand is put away and cannot strike.
- **Design.**
  - The finger curls come from SteamVR's skeletal summary: IVRInput's
    `GetSkeletalSummaryData`, `VRSkeletalSummaryData_t.flFingerCurl[5]`
    (openvr_capi.h, IVRInput_011, read 2026-09-27). OBVR already runs
    IVRInput_011 with an action manifest, which would need a skeleton action.
  - A fist: all four fingers curled past a threshold, with hysteresis.
  - Only with no weapon in the hand (a held sword is a fist too): with a
    weapon equipped, the fist does nothing.
  - The strike by motion already exists; it would be gated on the fist.
- **To find out.**
  - Whether the skeletal action gives curls on the Index as expected.
  - How hand to hand is readied: the ready key with nothing equipped, as
    vanilla does.

### 4.4 The quick menu on the right trackpad (built 2026-09-27)

- **Wanted.** The right trackpad pressed: a quick menu in front of the player,
  chosen from the way Half-Life: Alyx's hand menu is.
- **Built.**
  - The trackpad held down opens a ring of the eight hotkeys where the right
    hand is, standing upright and facing the head. Slot 1 is at the top,
    then round to the right.
  - Moving the hand past 4 cm towards a slot lights it. Letting go there taps
    that slot's number key for 0.08 s, the way pressing it would. A number
    key held longer opens the game's own hotkey ring (0x5C1F70 calls 0x5C1B80
    past a timed threshold at 0xB38BB0), so the tap stays short.
  - Letting go in the middle, or on an empty slot, uses nothing.
  - `[Hands] QuickMenu`, `QuickMenuDeadZoneMetres`, `QuickMenuTapSeconds`
    and `QuickMenuRingMetres`; a toggle in the settings under Hands.
  - The logic is `vr::StepQuickMenu` (`quick_menu_test`), the ring is
    `ui::PaintQuickMenu` (`quick_menu_painter_test`), on a
    `ui::CanvasOverlay`, and the hotkeys come from `game::ReadQuickKeys`.
- **Where the game keeps the hotkeys.**
  - `g_quickKeyList` at 0x00B3B440 is eight pointer lists of 0x10 bytes each;
    the form is at the start node + 8 (xOBSE GameTypes.cpp:9, and the engine's
    own read at 0x5C23AD).
  - The name is the form's TESFullName, found through the engine's
    `__RTDynamicCast` at 0x009832E6 (xOBSE GameAPI.cpp:221).
  - Checked in the game (docs/hand-script-harness.md): an Iron Longsword set
    on hotkey 3 read back as "3 Iron Longsword". Using slot 3 from the ring
    made the game say "Iron Longsword equipped."
- **Not built yet.**
  - Icons. The names and numbers are drawn in OBVR's own pixel font. The
    game's icons are DDS files inside the BSAs, and drawing them needs a
    loader OBVR does not have.
  - Wait in the middle of the ring.
- **Needs the action manifest.** On the legacy input path the right
  trackpad's click cannot be told from the stick's click
  (`NormalizeLegacyButtons`), so there it readies the weapon instead.
- **Not tried in the headset yet**: how 10 cm to a slot feels, and whether
  the ring at the hand is easy to read.

### 4.5 What becomes free

- The right stick click, once the weapon is drawn by gestures.
- The left A in the world.
