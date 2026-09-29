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
| Stick click | the Rest menu (T: wait, or sleep in a bed) while weapons are drawn by reaching; with `Holsters=0` ready / sheathe the weapon | run (with the stick pressed in) |
| Trackpad click | held: the quick menu, the ring of the eight hotkeys (4.4, built) | F1: the key the game binds to "Quick Menu" (Oblivion.ini [Controls] `Quick Menu=003BFFFF`; UESP lists F1-F4 as the journal pages) |
| Gesture | a swing strikes by motion | raised hand: block |

- Both stick clicks within a quarter second: OBVR's own menu.
- **Which way the left stick walks** (`[Hands] WalkDirection`, the settings
  row "Walk direction"; the tester, 2026-09-28: "gerade laufen mit linken
  stick [läuft] in eine andere richtung ... weil das recentering wo anders
  liegt").
  - Choices: head, right, left, blend (the default: halfway between the
    head and the stick's hand).
  - Why it went wrong: the stick is the W/A/S/D keys, and the game walks
    them along the player's heading. The head turned only the camera,
    measured from the recenter. So forward was where the player faced at
    the last recenter plus turns, not where they now stand facing.
  - Now, while the stick walks, the body turns to the chosen direction
    through the aim's hand-over (`g_aimBodyOffset`): the camera takes the
    turn back out, so the view holds still. The menus and the HUD keep the
    recenter's place; they are anchored in the room, not on the body.
    Standing still turns nothing. The aim keeps the body while it turns it
    itself.
  - Code: `vr/WalkDirection.h` (walk_direction_test), the headings from
    the recenter in `vr::HandMode` (`WalkYawOf`), the turn in CameraHook
    after the aim's. Harness: `tools/hand-scripts/walk-direction.txt`
    PASS (head turned 90 degrees: heading 0 to 270, the feet moved along
    it, the compass the same before and after).
  - **Not yet tried in the headset.** Unknown: whether the blend's
    turning with the hand while walking feels right.
- Left-handed (`LeftHanded=1`): the controllers swap roles as a whole
  (`vr::AssignHandRoles`); the table reads with "right" as the weapon hand.

### In a game menu

| Control | Effect |
|---|---|
| Pointing hand's trigger | click (on the release; a drag scrolls, sideways holds) |
| Left A, inventory only | drop the item under the cursor (vanilla's Shift + click, over four frames: `vr::StepDropPress`); one drop per press, the button has to be let go for the next |
| Left B / right B | Tab / Escape |
| Left stick | the mouse wheel |
| Right trackpad held, inventory or magic menu | the ring sets a hotkey to what the cursor is on (4.5) |

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
  right A. Works in the headset (Nadi, 2026-09-29), also with the right
  hand alone blocking and A.
- **The dodge roll needs no button.** Block and jump (from Acrobatics
  Journeyman): the raised hand and a flick up. UESP Oblivion:Acrobatics:
  "while holding block, you can jump in any direction", so a direction on
  the left stick belongs to it. Not yet tried.
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
| Yield, dodge | gesture + button, see section 2 | yield works (2026-09-29), dodge untested |
| Hotkeys 1-8 | right trackpad held: the quick menu; in the inventory or magic menu the same ring sets them | built (4.4, 4.5) |
| Take an item | activate (right A), or held and let go at the body | built (4.6) |
| Wait (T) | right stick click (with the weapons drawn by reaching) | built 2026-09-27 |
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
    - The swap is silent (the tester, 2026-09-27: the weapon-change sound
      has to go). As xOBSE's EquipItemSilent does it, the `jne` at the start
      of the item-sound picker (0x005E96E7, in 0x005E96E0) is nopped for the
      call and put back after, so the picker answers "no sound". The
      fist's unequip is silent the same way. Whether any other sound of the
      swap remains is not checked; the harness cannot hear.
  - **The draw's animation.** Drawing and sheathing played about a second
    of animation each before the weapon was in the hand or gone (the
    tester, 2026-09-27), and the fists' draw still took 0.66 s after the
    first fix (2026-09-28). `game::StepWeaponDrawSpeed` now runs the Equip
    and Unequip groups' (17, 18) **time** `[Hands] WeaponDrawSpeed` times
    faster (10 by default; 1 is the game's speed).
    - Why the frequency was not enough (measured 2026-09-28, with probes
      on HighProcess::SetAction and NiControllerSequence::Deactivate): the
      player's action (process +0x1F4) ends only when the action's sequence
      is inactive (0x005FD8B3). The engine stops a group when its own time
      reaches the group's end key (0x004774A1), and that time is the
      sequence's offset (+0x48) plus the anim data's clock (+0x94),
      unscaled by the frequency. After the stop comes the group's blend-out
      (0x004733A0). So the fists' 0.2 s draw took 0.25 s blending in, 0.2 s
      playing and 0.2 s easing out, at a frequency of 10 or 40 alike.
    - Now, each frame, a playing sequence's offset is moved on by
      (speed - 1) times the clock's step, and a blend's window (+0x4C,
      +0x50) is shrunk around the clock by the speed, once per blend. The
      frequency is left alone. The keys still fire in order, only sooner.
    - Measured after (harness, 2026-09-28), from the key to the action's
      end: fists 0.66 s to 0.12 s, one-handed 0.88 s to 0.12 s, two-handed
      1.42 s to 0.13 s, sheathing likewise.
    - The two-hander's settle a second after the draw (the tester,
      2026-09-27: in the hand at once, then it slides a few centimetres)
      was this: the engine kept the Equip sequence and its end pose until
      the draw's own length, then the two-hand idle moved the Weapon node
      about 2 cm. The sequence now ends at 0.13 s, so the idle should take
      over at once. **Not yet seen in the headset.**
    - The draw's sound (keys "Enum: Equip" / "Enum: Unequip") went missing
      with the moved offset: the engine's key window skipped the span it
      was moved over. That span's keys are now fired through the engine's
      own handler (0x0051AF70). The harness showed one sound per draw and
      sheathe, and the tester heard them as expected in the headset
      (2026-09-28). The fists have none, as at the game's speed.
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
    all past 0.90 (0.80 until 2026-09-28), held for 0.25 s. The hand is open again only once all four
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
    `FistHoldSeconds`; a toggle in the settings under Hands, and the row
    "Fist at" for `FistCloseCurl` (0.40 to 0.95; the tester, 2026-09-28:
    a hand whose little finger reads low, the Index sensing it only with
    the hand close to the handle, needs a lower limit). The open limit is
    kept at least 0.15 below it (`vr::FistOpenLimit`, fist_test): at or
    above it, a hand resting between the two would flip between fist and
    open, a ready click each time.
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
- **Icons (built 2026-09-28).** Each slot shows the game's own icon above its
  name, the file the inventory and the magic menu show.
  - The path: an item's TESIcon, armour's and clothing's male icon
    (TESBipedModelForm), a spell's first effect's icon (`game::ReadIconPath`,
    addresses in `GameAddresses.h`). Under `Textures\Menus\Icons\`, TESIcon's
    own prefix (0x0046CAA0).
  - The file: opened through the engine's NiFile::GetFile (0x00748100), so
    loose files and archives count the way the game finds them, and read
    through the stream's read procedure.
  - Decoded on the CPU (`render::DecodeDds`: DXT1, DXT3, DXT5, bit-masked
    RGB(A)) - every format the vanilla icons use, counted in both vanilla
    texture archives. Fitted to 64 pixels and kept, 24 at a time
    (`game::ItemIcon`). Tests: `icon_test`, `quick_menu_painter_test`.
  - Checked outside the game: real vanilla icons (DXT3, DXT5, 16- and 32-bit)
    decoded and painted into the ring by the painter. Not yet run in the
    game: the engine's file opener, the casts, and which icon a spell shows.
  - A spell shows its first effect's icon. Whether the magic menu picks the
    same effect (the first, or the costliest) was not looked up.
  - `[Hands] QuickMenuIcons=0` goes back to numbers and names only.
- **Pages (built 2026-09-28).** Several pages of eight, 3 by default
  (`[Hands] QuickMenuPages`, 1-5, a row in the settings).
  - The trigger pulled while the ring is up turns to the next page, round to
    the first. Dots above the middle, the page shown gold. The trigger is
    the ring's while it is up and until it is let go, so it neither attacks
    nor clicks (`vr::QuickMenuKeepsTrigger`).
  - The page shown is always the game's own eight: using, setting from the
    inventory and saving stay vanilla. A turn keeps the eight's forms and
    writes the next page's the way setting a hotkey from the inventory does,
    for each slot: the old item's slot extra taken off its stack
    (0x004895B0), the list emptied (0x00573880), the form added (0x005B1E20),
    an item's stack given the slot (0x00422BA0 or 0x00489820). Then the
    engine's own check (0x005C1900) takes out what the player no longer
    has. Read with dumpbin; `game/QuickKeyPages.h` has the detail.
  - Why not the lists alone: an item's hotkey is also an extra (type 0x55)
    on its inventory stack, and the check at 0x005C1900 - called from
    about ten places - drops a list entry whose stack does not carry it.
  - The other pages live in the xOBSE co-save (record 'QKPG'), their form
    ids resolved on load for a changed mod list. Without the co-save
    interface the ring has one page.
  - Tests: `quick_menu_test` (turning, whose trigger), `quick_key_pages_test`
    (next page, a turn, the record both ways with every refusal),
    `quick_menu_painter_test` (the dots).
  - Confirmed in the headset on 2026-09-28 by the tester: "funkt wie
    erwartet". What that run covered in detail was not reported; an item
    hotkeyed on two pages was not named as tried. An item that only ever sat in the player's starting inventory (no
    change entry) cannot be given its slot this way; the check then takes it
    out.
- **Not built yet.**
  - Wait in the middle of the ring.
- **The game's own ring instead?** Tried in the game on 2026-09-27
  (`vanilla-quickkeys.txt`, `vanilla-quickkeys-menu.txt`, menus mirrored to
  the monitor):
  - **In the world.** Holding a number key for 2 s opened no menu. The item
    was equipped at once ("Iron Longsword equipped.").
  - **In the inventory.** Holding a number key over an item opened no ring
    either. Holding it and clicking the item sets the hotkey, which is what
    4.5 builds on.
  - The game's QuickKeys menu (0x416) is where hotkeys are assigned, from
    the inventory and magic menus. It is not something the world shows for
    using them.
  - So there is no vanilla ring to show. Opening 0x416 ourselves in the
    world would pause the game on an assignment screen.
  - What carries the vanilla look into OBVR's ring is the game's own item
    and spell icons, built since (the icons above).
- **Needs the action manifest.** On the legacy input path the right
  trackpad's click cannot be told from the stick's click
  (`NormalizeLegacyButtons`), so there it readies the weapon instead.
- **Not tried in the headset yet**: how 10 cm to a slot feels, and whether
  the ring at the hand is easy to read.

### 4.5 Setting a hotkey from the inventory or the magic menu (built 2026-09-27)

- **Vanilla on the PC.** Open the inventory or the magic menu, hold a number
  key 1-8, and left-click the item or spell while it is held (the PC manual:
  "define Hotkeys by holding one of the 1 - 8 Keys while simultaneously
  selecting ... by Left Clicking"; UESP Oblivion:Controls, "press and hold a
  Hotkey, then left click on an item").
- **How the game does it** (read 2026-09-27):
  - The hotkey handler 0x5C1F70 reads the eight quick controls. With the
    Inventory (0x3EA) or Magic (0x3FE) menu up, a held key calls 0x5C1B80 at
    once, with no timer. That opens the QuickKeys menu (0x416) and sets the
    byte at 0xB3B43D.
  - While that byte is set, the menus' click handlers (0x5ABC1C inventory,
    0x5B39BC magic) assign the clicked entry to the held slot
    (0x5C1100 answers which) instead of equipping it.
- **Built: the same ring, in those two menus.**
  - Hold the right trackpad with the laser on an item. The ring opens at the
    hand with "Set hotkey" in the middle; every slot can be chosen, empty or
    not.
  - Move the hand to a slot and let go. OBVR does what the keyboard does:
    the slot's number key down for 0.2 s, a click for 0.08 s while it is
    held, the key kept 0.15 s more, then both up. Each step lasts at least
    one frame.
  - The cursor does not move from the trackpad's press to the end of the
    sequence, so the click lands on what the laser was on, not where the
    hand moved it.
  - Let go in the middle: nothing. The menu closing during the sequence
    lets key and click go at once.
  - Which menu counts is the one under the cursor (`game::ActiveMenuId`).
  - Logic `vr::StepQuickMenu` (assign mode, `quick_menu_test`); the ring's
    "Set hotkey" and gold edges on empty slots in `ui::PaintQuickMenu`.
- **Checked in the game** (`quick-menu-assign.txt`, 2026-09-27):
  - Slot 5 held the Iron Longsword. The laser on the Steel Longsword row,
    the ring, slot 5: the log read "5 Steel Longsword" afterwards, one click
    reached the Inventory menu, and slot 5 from the ring in the world said
    "Steel Longsword equipped."
  - The experiment before it (`vanilla-quickkeys-assign.txt`): number key 5
    held and a trigger click on the Steel Longsword set slot 5. So the key
    OBVR sends is seen as held. The QuickKeys menu itself did not show on
    the monitor mirror; whether it shows in the headset is not checked.
- **Not checked:** spells from the magic menu. The magic menu's click path
  is the same kind of check (0x5B39BC), but no scenario opens that menu yet.

### 4.6 Stowing at the body, and taking only by hand (built 2026-09-27)

- **What it does.** An item held in the hand, brought to the chest or the
  belly and let go there, goes into the inventory. `[Hands] StowAtBody`, on
  by default; settings "Stow at the body".
- **Taken as activating takes it** (the tester: an owned item must be a
  crime, "genau so wie aktivieren"):
  - Everything but a book goes through the ref's own activate,
    `TESObjectREFR::Activate` (0x004DD260), with the arguments the player's
    activate control uses (player, 0, 0, 1; the call at 0x0067318A). So the
    item's OnActivate script runs, and the item's activate reaches the
    player's pickup (vtable +0x2CC, 0x00660910), which reads the owner
    (0x004DB6B0) and hands an owned item to the crime.
  - A book's activate opens it to read. A book therefore goes to the pickup
    directly, and is refused when it is marked "cannot be taken".
  - Only items a pack holds (`game::IsHandItemType`). Anything else held -
    a body, a basket that is not an item - is let go as usual.
  - The take waits until the engine has let go of the object itself (the
    grab's ref at player+0x578 changes), so its spring never holds a
    reference that went into the pack. It is not thrown on the way. Given
    up after 1 s if the engine keeps holding it.
- **The spot.** A gold ring at the chest, shown while an item is held and
  filled while the hand is in it (`ui::PaintStowSpot`): let go there and the
  item is stowed; let go anywhere else and it drops. It is a sphere of
  0.16 m round a point 0.17 m ahead of the eyes and 0.30 m below them, in
  the body's frame (`vr::BodyRelative`), turned to face the eyes.
  `[Hands] StowForward`, `StowUp`, `StowRadius`.
  - First test: a wide cylinder round the torso missed a sword let go
    0.23 m ahead - the controller is held in front of the body. Moved
    forward, it stowed, but then nothing could be dropped in front of
    oneself (the tester, 2026-09-27). The visible spot is the tester's
    answer: a small target one can see, placed where the hand against the
    chest was measured (0.19-0.23 m ahead, 0.17-0.30 m below the eyes).
  - A release up to 0.3 s after the hand left the spot still stows: the hand
    moves as it opens.
  - **Placed by hand, then hidden** (the tester, 2026-09-28). The ring is
    off by default (`[Hands] StowSpotVisible=0`); the spot stows unseen. The
    settings row "Place the stow spot" opens a window (MenuQue, the
    adjust-hands window with the stow's words) and shows the ring for as
    long as it is open. A grip closed on the ring (or within 6 cm of it)
    takes it along with that hand; opening the grip leaves it. Done writes
    `StowRight`, `StowForward`, `StowUp` and hides the ring; Cancel or Esc
    puts it back; Reset returns it to the default. Kept within reach: 0.6 m
    to either side, 0.4 m behind to 0.8 m ahead, 1.4 m below to 0.3 m above
    the eyes (`vr::StepStowPlace`, tested in stow_test).
- **Taking only by hand.** `[Hands] TakeOnlyByHand`, off by default; settings
  "Take only by hand". The activate button is kept from the game while the
  laser is on a loose item. A book still opens to read, and doors, chests
  and people are activated as ever. It needs stowing on: with stowing off
  the activate takes items as before, or they could not be taken at all.
- Logic `vr::StepStow`, `vr::ActivateWithheld` (`stow_test`); the game side
  `game::TakeIntoInventory`.
- **Checked in the game** (2026-09-27):
  - `stow.txt`: an Iron Longsword dropped, grabbed, brought to the chest and
    let go - "taken (no owner ...)", and the inventory lists it.
  - `stow-owned.txt`: the same sword first given to Baurus (SetOwnership
    00023F2A through the console) - "taken (owner 00023F2A ...)", and the
    inventory shows it with the red hand of a stolen item. No one saw it, so
    no bounty; a bounty with a witness is not checked.
  - `take-only-by-hand.txt`: A at the sword kept from the game, the sword
    still on the floor, then stowed. `activate-takes.txt`, the option off:
    the same A takes it.
- **Not checked:** how the zone feels in the headset, and a witnessed theft.

### 4.7 Later: putting a weapon or an item on a hotkey without a menu

The tester's idea (2026-09-27), in the style of Half-Life: Alyx. Not built.

- With the ring open in the world, the drawn weapon or an object held in the
  other hand is brought to a slot and let go there: that slot now holds it.
- A held loose object would be taken into the inventory first (as 4.6
  takes it, with its crime), then set on the slot.
- Setting the slot could not use the menu click of 4.5, since no menu is
  up. It needs the game's own assignment function, which the click handlers
  call (0x00489820 on the container changes, and 0x00484BC0 for the spell
  branch). Their arguments are read from the call sites but not tried.
- Open questions: which hand holds the ring while the other brings the
  object; what happens to a slot's old entry; how the drawn weapon is told
  from a held one.

### 4.8 What becomes free

- The right stick click, once the weapon is drawn by gestures.
- The left A in the world.

## 5. Found in the headset, 2026-09-29

- **Fixed: the spell's hand effect floated in the air.** "linker trigger
  macht den zauber ja. aber der zauber effekt bei heal zb erscheint
  irgendwo über der hand in der luft statt an der hand." The engine hangs a
  spell's hand effect on the node `magicNode`, found by name at each cast
  (0x005EDCB8, 0x00602D42) and given the effect as a child (0x00602D71). In
  the first-person skeleton it is a child of `Bip01 Spine2`, and the cast
  animations key it to where the animated hand would be
  (`1stperson_castself.kf`, its own translation track). OBVR's pins move
  the forearms and so the hands, not `magicNode`. Now, after the pins, its
  local position is set to the middle of the pinned left palm (wrist to the
  middle finger's base, `game::PalmCentre`), its rotation left to the
  animation (CameraHook `PlaceMagicNodeAtCastingHand`). Harness
  `cast-effect.txt` PASS: "the casting hand's effect node set to the left
  palm". **Not seen:** the harness picture shows no hands; whether the
  glow now sits in the palm is for the headset. The left hand is taken as
  the casting hand (the game's); left-handed play is not checked.
- **Open bug: a swing in empty air attacks, with the attack sound and a
  step forward.** "wenn ich die touch controller schwinge im leeren raum
  kommt ein h2h attacke mit attack sound und mit ausfallschritt (nausea)."
  **Fixed.** Cause (read): with the fists up and the hand open, the strike
  by motion may not strike (`FistAllowsStrike`), and the swing then fell
  back to pressing the attack control (`SwingPressesAttack` was asked with
  `strikeByMotion`, false) - vanilla's hand-to-hand attack ran, with its
  sound and its movement. Now the fallback is asked whether the swung
  thing belongs to the strike by motion at all (`SwingMayPressAttack`:
  motion hits on and a melee weapon or fists in hand): an open hand waved
  through the air does nothing. The step forward is the attack
  animation's (derived; its root motion not read). Tests in
  hand_mode_test.
- **Open bug: no swing sounds, no attack grunts.** "wir machen gar keinen
  schwing sound von schwert, axt und h2h. auch keine player attack sounds
  also grunzen." The PC's swish sounds are `WPNSwishSmall` 000872C2,
  `WPNSwishMedium` 000887F7, `WPNSwishLarge` 0008976F, `WPNSwishHand`
  00088834 (Oblivion.esm; the `...X` ones point to the 360's files, which
  the PC does not have).
  - **The swing speed in the room (2026-09-29).** "Ja ist da aber passiert
    sogar bei dem kleinsten luftzug. das müssen wir ändern dass der und der
    aktuall schlag nur passieren bei gewisser kraft vom schwung die man
    einstellen kann." The knob was there (`[Hands] SwingLight`, row "Swing
    speed", 1.6 m/s); the cause was the speed: taken from the hand's
    position relative to the head, so a head turned quickly with the hand
    still read a metre or two a second (0.45 m from the head at 5 rad/s is
    2.3). Now from the controller's position in the room
    (hand_mode_test: a head turned at 70 rad/s with the hand still starts
    no swing). Each swing now logs its fastest ("a light swing, 2.4 m/s at
    its fastest"), 60 lines a run, to set the row by.
  - **Swish, built.** Vanilla plays it in AttackHandling on a miss only
    (0x005FEC7D..0x005FEC95: 0x006AF880 cdecl(actor, 0.0, 0.0, no target,
    weapon type or -1, -1, -1, 0, 0), which picks Hand, or Small/Medium/
    Large by the weapon's speed, and plays it at the actor); the strike by
    motion calls AttackHandling only for a body it meets, so no swing
    sounded. Now every swing that may strike calls 0x006AF880 as vanilla's
    miss does, once, as it starts (`SwishDue`, `game::PlaySwingSwish`).
    Harness: "a swing's swish (weapon type 0)" per swing in `hand-bodies`
    and `fist-armed`. A hit plays the engine's hit sound as before.
  - **Open: the grunts.** The combat topics are `Attack` (000000DC) and
    `PowerAttack` (000000E6), hardcoded at 0x00B10DA4; vanilla voices the
    player on power attacks (the Nexus mod "Silent Player Voice" exists to
    stop it). The engine's say-topic call and where the attack starts it
    are not found yet.
