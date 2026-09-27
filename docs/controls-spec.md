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
| Grip | grab (holding objects, docs/holding-objects-spec.md); cancels a teleport aim; closed at the left hip: one-handed weapon, over the right shoulder: two-handed weapon or staff (4.1) | grab; closed behind the left shoulder: the bow (4.2) |
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
| Ready weapon (F) | right stick click; reaching to the hip or shoulder (4.1, 4.2); a fist with nothing equipped (4.3) | built |
| Activate, grab | right A, grips | built |
| Journal / menus, pause | left B, right B | built |
| Drop (Shift + click) | left A in the inventory | built |
| Yield, dodge | gesture + button, see section 2 | untested |
| Hotkeys 1-8 | right trackpad held: the quick menu | built (4.4) |
| Wait (T) | - | missing |
| Quick save / load (F5 / F9) | Escape menu | enough |
| Always run, auto move | - | not needed in VR |
| Change view (R) | - | Full VR stays in first person |

## 4. The target picture

The tester's aim: the weapon is drawn by reaching for it, as with a real one.
4.1 to 4.4 are built (2026-09-27) and were checked in the game with the
hand script harness (docs/hand-script-harness.md). None of them has been tried
in the headset yet.

### 4.1, 4.2 Every weapon from its own place (built)

- **Wanted** (the tester, 2026-09-27).
  - The one-handed blade or blunt weapon: the weapon hand's grip at the left
    hip.
  - The two-handed blade or blunt weapon, and the staff: the weapon hand's
    grip at the right shoulder.
  - The bow: the other hand's grip at the left shoulder.
  - The same reach sheathes again ("ja, soll es").
- **The weapon types.** Oblivion has six (xOBSE GameForms.h:2669-2682, the
  TESObjectWEAP type at +0x90, the same enum as OBVR's `WeaponTypeCode`).
  `vr::KindOfWeaponType` sorts them into the places:

  | Type | Place |
  |---|---|
  | Blade one hand (0), Blunt one hand (2) | left hip |
  | Blade two hand (1), Blunt two hand (3), Staff (4) | right shoulder |
  | Bow (5) | left shoulder |

  With no weapon the hands fight hand to hand (4.3).
- **Built.** `vr::StepHolster` (`holster_test`, and `hand_mode_test` through
  the mode).
  - **Where the zones are.** They are in the body's frame: metres from the
    eyes along the head's heading alone, so looking down at the hip does not
    move it.
    - The one-handed zone is 22 cm round (-0.18, 0.02 forward, -0.62 up).
    - The two-handed zone is 18 cm round (0.18, -0.12 forward, -0.15 up),
      over and behind the right shoulder.
    - The bow's zone is 18 cm round (-0.18, -0.12 forward, -0.15 up), behind
      the left shoulder.
    - Left-handed, they mirror.
  - **What a grip there does.** A grip that closes in a zone is the holster's
    until it opens: it does not grab and does not cancel a teleport.
    - A grip already closed when the hand arrives stays a grab, so a held
      object is not dropped at the hip.
    - The draw and the sheathe go through the ready-weapon key and its
      state machine (`StepReadyWeapon`), like the stick click.
  - **One weapon slot.** The last weapon of each kind seen equipped this
    session is remembered.
    - Reaching for another kind equips the remembered one through the
      engine's `Actor::EquipItem` (0x005FAEA0, called with (form, 1, NULL,
      1, false), as the game's own EquipItem command does, xOBSE
      GameObjects.cpp:21).
    - It is drawn once the game shows it in the hand, or given up after 2 s.
    - With none of that kind seen yet, the reach does nothing and says so in
      the log.
  - **Another weapon drawn.** Reaching for one kind while another is
    drawn does nothing ("another weapon is drawn, it goes back first" in the
    log). It has to go back first, by its own reach (the tester, 2026-09-27:
    first sheathe the bow, then draw the sword, and the other way round).
    Raised fists are not in the way.
  - **Fitting the places.** "Fit weapon places" in the settings (Hands) runs
    a guided fit, `vr::StepHolsterFit` (`holster_fit_test`).
    - A panel in front of the player says what to do.
    - The weapon hand's trigger takes the one-handed place, then the
      two-handed one; the other hand's trigger takes the bow's.
    - Either menu button cancels, and nothing changes.
    - The places are saved to `[Hands] HolsterOneHand*`, `HolsterTwoHand*`
      and `HolsterBow*`. Left-handed they are stored mirrored, as a
      right-handed body.
    - While the fit runs, the triggers and menu buttons reach nothing else.
  - **Settings.** `[Hands] Holsters` and the zone keys; a toggle in the
    settings under Hands.
- **Checked in the game** (`tools/hand-scripts/holster.txt`, PASS):
  - bow sheathed, then a reach to the hip: the sword was equipped and drawn;
  - the same reach sheathed it;
  - the left hand behind the shoulder equipped and drew the bow;
  - with the bow drawn, a reach to the hip did nothing.
  - `holster-two-handed.txt` and `holster-staff.txt` PASS: the claymore and
    the staff were drawn and sheathed over the right shoulder; the hip
    brought the sword; with the sword drawn the shoulder did nothing; sword
    back, the shoulder equipped and drew the claymore or staff again.
  - `holster-fit.txt` PASS: the fit took all three places, wrote them to the
    test run's INI (never to OBVR.ini), and a reach to the new one-handed place
    drew the sword.
- **Open.**
  - Whether the zones sit right on a real body, standing and seated. They
    are starting values.
  - The two-handed zone and the arrow's reach back over the right shoulder
    (`BowNeedsReachBack`, off by default) share a place. They do not clash:
    the reach back arms the trigger while a bow is drawn, and with a bow
    drawn a grip at the right shoulder does nothing. Not tried in the
    headset.
  - Calling EquipItem directly skips the tail the game's command handler runs
    for the player. In the harness the HUD's weapon icon did follow.
  - A weapon never seen equipped this session is not searched for in the
    pack.

### 4.3 Fists by making a fist (built 2026-09-27)

- **Wanted.**
  - A fist with the weapon hand: hand to hand is ready, and striking is all
    that is left.
  - The hand opened: hand to hand is put away and cannot strike.
- **Built.** `vr::StepFist` (`fist_test`, and `hand_mode_test` through the
  mode).
  - **The curls.** They come from SteamVR's skeletal summary:
    - `GetSkeletalSummaryData`, entry 20 of IVRInput_011 (openvr_capi.h
      v2.15.6), `VRSummaryType_FromDevice`.
    - The skeleton actions are `/actions/obvr/in/right_skeleton` and
      `left_skeleton`, in the manifest and bound in the knuckles and
      oculus_touch files, in the form the Action-manifest wiki and SteamVR's
      own binding files use.
    - The log says `hand skeletons resolved` at startup, and once per hand
      whether its skeleton reads.
  - **What counts as a fist.** The index, middle, ring and little fingers are
    all past 0.80, held for 0.25 s. The hand is open again only once all four
    are below 0.35.
    - The thumb is left out: on an Index it rests on the stick and buttons
      either way.
  - **When a fist counts** (the tester, 2026-09-27: a fist made through the
    touch sensing makes hand to hand active when no weapon is drawn; it can
    be switched off).
    - Nothing in the slot: a fist raises the fists, an opened hand lowers
      them, through the ready-weapon key and its state machine.
    - A weapon in the slot but sheathed: a fist takes it off first. That is
      the game's own UnequipItem path: the worn stack is looked up through
      0x0041E6F0 and 0x00485FA0, then Actor::UnequipItem 0x005F2E70 is
      called, as the command handler 0x005164C0 does. Once the slot is
      empty, the fists are raised. The holster remembers the weapon, so a
      reach to the hip or the shoulder brings it back. The fists are given
      up after 2 s if the slot does not empty.
    - A weapon drawn: a fist is the hand round the handle and does nothing.
    - While the fists are up, a swing strikes by motion only as a fist.
    - When the curls are unknown (no skeleton), fists strike as before.
  - **Not while holding or holstering.** A fist made round a held object, or
    while the grip is the holster's, changes nothing. A squeezed grip alone
    does not block it: on an Index a real fist squeezes the handle too.
  - **Settings.** `[Hands] Fists`, `FistCloseCurl`, `FistOpenCurl` and
    `FistHoldSeconds`; a toggle in the settings under Hands.
- **Checked in the game** (`tools/hand-scripts/fist.txt`, PASS, with the
  scripted skeleton reporting every finger at the script's curl):
  - bare hands, curl 1: "fists up", and the game showed them drawn;
  - curl 0: "fists down", sheathed again;
  - `fist-armed.txt` PASS: sword drawn, fist: nothing; sword sheathed, fist:
    "the sheathed weapon taken off for the fists", then "the slot is empty -
    fists up".
- **Open.**
  - Whether a real Index gives the curls this reads. The harness plays the
    skeleton; the device path (`hand skeleton reads` in the log) has not run
    with a controller in hand.
  - Whether the thresholds tell a fist from a relaxed grip on the handle.
  - A fist that squeezes the Index's grip also presses the grab. With
    nothing within reach the grab takes nothing, but an item right at the
    hand would be picked up. Not tried.
  - SteamVR keeps the bindings a player has changed. With custom OBVR
    bindings saved, the skeleton stays unbound until the defaults are
    reloaded.

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
- **The game's own ring instead?** Tried in the game on 2026-09-27
  (`vanilla-quickkeys.txt`, `vanilla-quickkeys-menu.txt`, menus mirrored to
  the monitor):
  - **In the world.** Holding a number key for 2 s opened no menu. The item
    was equipped at once ("Iron Longsword equipped.").
  - **In the inventory.** Holding a number key over an item opened no ring
    either.
  - The game's QuickKeys menu (0x416) is where hotkeys are assigned, from
    the inventory and magic menus. It is not something the world shows for
    using them.
  - So there is no vanilla ring to show. Opening 0x416 ourselves in the
    world would pause the game on an assignment screen.
  - What would carry the vanilla look into OBVR's ring is the game's own item
    and spell icons. They are DDS textures in the BSAs, which the game's
    texture loader can open. Not built.
- **Needs the action manifest.** On the legacy input path the right
  trackpad's click cannot be told from the stick's click
  (`NormalizeLegacyButtons`), so there it readies the weapon instead.
- **Not tried in the headset yet**: how 10 cm to a slot feels, and whether
  the ring at the hand is easy to read.

### 4.5 What becomes free

- The right stick click, once the weapon is drawn by gestures.
- The left A in the world.
