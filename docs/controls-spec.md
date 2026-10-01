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

- Both stick clicks within a quarter second: OBVR's own menu, on their
  release. Both held in for three seconds: the recenter, as the recenter key,
  and no menu (the tester, 2026-09-30: "beide thumbsticks für 3s gedrückt
  macht ein recenter"; `vr::StepStickChord`, hand_mode_test; harness
  `bow-by-hand` ends with it: "Camera: both sticks held - recentering").
- **The flat picture follows the head** (the main menu, loading screens,
  films, a menu on the cinema screen; the tester, 2026-09-30: "das
  hauptmenu auch wie ingame alle paar momente neuausrichten abhängig wo man
  gerade steht oder hinsieht"). It hangs where the head looked when it
  appeared; turned more than `[Render] FlatFollowDegrees` (30) from it, or
  half a metre off, for a second, it is taken along to where the head is
  (`vr/FlatFollow.h`, flat_follow_test; CameraHook's flat path). 0 keeps it
  in place until the recenter. **Not tried in the headset**, nor in the
  harness (its scripts start after the load).
- **The laser starts beside the controller**: `LaserRightMetres` (-0.01)
  and `LaserUpMetres` (-0.02) in the controller's own frame, the right
  mirrored for the left hand (the tester, 2026-09-30: "der laserpointer muss
  auch noch 2cm weiter runter und 1 cm weiter nach links"). The beam, the
  dot, the crosshair on the hand and every pick made along the laser move
  with it (`vr::LaserOffsetLocal`, laser_geometry_test). Settings rows
  "Laser sideways", "Laser height" and "Laser start" (the origin along it).
- **The HUD on the hands moved on its own face**: `[HandHud]
  OffsetRightMetres` (0) and `OffsetUpMetres` (-0.03), both ways it is
  shown (the tester, 2026-09-30: "die hud an den händen sind auch noch zu
  weit oben. man muss x, y einstellen können"); settings rows "Hand HUD X"
  and "Hand HUD Y" (`vr::AlongOwnXY`, hand_hud_test).
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
  the left stick belongs to it. Works in the headset (Nadi, 2026-09-29).
  Since the left hand blocks only with a shield (section 5), without one the
  roll needs the weapon held across; see 4.9 for a proposal.
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
| Yield, dodge | gesture + button, see section 2 | yield works (2026-09-29), dodge works (2026-09-29) |
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
- **The staff's and the bow's draw sounds** (the tester, 2026-09-30: "stab und bogen keine equip sounds bekommen im vergleich zu 1 hand oder 2hand").
  - The game plays `WPN<type>Equip` / `WPN<type>Unequip` at the "Enum: Equip" / "Enum: Unequip" text keys of the first-person draw animation (0x006B07F0 builds the name from the weapon's type: WPNBlade1Hand, WPNBlade2Hand, WPNBlunt1Hand, WPNBlunt2Hand, WPNStaff, WPNBow; read from Oblivion.exe). With `WeaponDrawSpeed` those keys are fired by OBVR (`game::StepWeaponDrawSpeed`).
  - The staff's `staffequip.kf` and `staffunequip.kf` have no such key (only Attach/Detach; pyffi over `Oblivion - Meshes.bsa`): the staff is silent in the game itself.
  - The bow's `bowequip.kf` has it at 0.5 s, and the harness logged the span 0.435 to 0.536 s fired through the game's handler ("Draw speed: keys fired", `bow-hand` 2026-09-30 16:30); the sound files are in `Oblivion - Sounds.bsa`. Yet the tester heard nothing. Why is not found (open).
  - So OBVR plays the game's own sound forms for these two when the weapon shows drawn or sheathed (`vr::DrawSoundForm`: WPNStaffEquip 00029BB2, WPNStaffUnequip 00029BB3, WPNBowEquip 00088B2B, WPNBowUnequip 00088B2C). For the bow this is a detour: the direct fix is whatever keeps the game's own key from sounding. If the game does play it somewhere, the bow sounds twice.
  - Harness 2026-09-30: `holster-staff` PASS (…/20260930-163904) with "the staff's draw sound (00029BB2) played" and the sheathe's (00029BB3); `bow-hand` PASS (…/20260930-163431) with the bow's. `holster-staff` now runs with `SwingLight=1000`: the scripted hands jump at 44 m/s, a swing, and the staff's cast held the sheathe off (a runner effect since swings by motion).
  - The tester, 2026-09-30 after that build: the bow's "war da whl schon vorher" - the game plays it, the earlier report was a mistake; OBVR's own bow sound is taken out again (`DrawSoundForm` gives the staff's only). The staff's is "sehr leise": `tes4-combat-staff_ready.wav` as the game has it. **Open:** louder, if wanted - `PlaySoundForm` sets no volume.
- **Open: where the staff is held.** The tester, 2026-09-30: the staff "sitzt nun ok noch nicht perfekt aber mir ist aufgefallen dass man den eh eher vorne hält oder" - the right hand holds it 30 to 38 units down from the Weapon node (the model's grip as the game places it); held further forward (towards the head) would be a different grip on the weapon hand, not built.
  - The game's own staff pose (`_1stpersonstaffidle.kf`, pyffi, 2026-09-30): the staff hangs on the weapon hand the same way round as a sword (the Weapon node's axis in the hand identical), only moved 37.6 units along it - the Weapon node's key to the hand (13.9, 3.1, 36.7) against the sword's (6.2, 2.0, 0.8), so the right palm is 37.6 below the node, as measured live. Both hands are on it: the left palm 9.4 below the node and 2.8 off the axis - 28 units above the right hand, towards the head (the Staff of Apotheosis's head end is its +y, 67 above the right palm; its butt 47 below). The two-hander idle has the left hand 7 below the right instead.
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

### 4.9 Proposal: the dodge roll from the free left hand (2026-09-29, not built)

**Wanted.** The tester: "Kann man hier einrichten dass wenn ich hier Anime
style meine linke hand heben als rechtshänder und da ist kein schild und
rechts meine waffe, dass ich dann zwar nicht blocken kann aber die
ausweiche rolle machen kann. oder die nur geht wenn ich eine faust balle
links und dann sogar einfach nur vor links rechts und hinten mache und die
rolle geht dann in diese richtung."

**Why it is needed.** Vanilla's roll is block and jump, in the direction
held (UESP Oblivion:Acrobatics: "while holding block, you can jump in any
direction"; from Acrobatics Journeyman). Since the raised left hand blocks
only with a shield (section 5), a player with a one-handed weapon and a
free left hand has to hold the weapon across to roll - the sword hand is
busy guarding exactly when it should be ready to strike after the roll.

**Two gestures, both for the free left hand (no shield, the right hand
armed or empty):**

1. **Dodge stance: the raised open left hand.** The same pose that blocks
   with a shield - hand up and out in front of the chest (`IsBlockGesture`)
   - becomes a stance without one: it does not block (no block key, no
   block animation, no damage reduction), but it arms the roll. While the
   stance is held, the jump flick (right stick up) rolls in the direction
   the left stick holds, as vanilla's block-and-jump does.
2. **The fist flick: a left fist moved sharply.** The left hand made a fist
   (`HandBodyFist`, the same limits as 4.3) and moved fast - a short flick
   of 15 to 25 cm - forward, backward, left or right relative to the head:
   the roll goes that way at once, without the stick and without the jump
   flick. Like the power attack's direction (`ClassifyPowerSwing`), the
   flick's way decides the direction; mostly up or down is no roll.

The tester's "oder" leaves the choice open; proposed: both, each with its
own settings row, the fist flick on by default (one hand, no stick, the
most "anime"), the dodge stance off by default (it can catch a hand
raised for something else).

**How it would reach the engine.** Two routes, the first to try first:

- **Vanilla's own roll, fed as vanilla wants it.** For the frames of the
  roll, OBVR holds the block control and taps jump with the movement
  direction set - the same inputs a gamepad player gives. Risk: the block
  control for those frames may show a block animation or block a hit for
  an instant; to be measured (harness: the player's action and the block
  state per frame around the roll). The direction for the fist flick is set
  by holding the matching movement key for the roll's first frames.
- **The engine's dodge call directly.** Where HandleInput turns block +
  jump + direction into the roll (the player's input handler around the
  jump, 0x0065EC96..0x0065EEC6 reads the movement flags for the power
  attack's direction the same way) - not read yet. Calling it with a
  direction would need neither the block nor the stick. Needs the research
  first.

**Conditions kept from vanilla:** Acrobatics Journeyman or better (the
engine checks it either way, route 1; route 2 has to check it itself), on
the ground, not while swimming, the roll's fatigue cost. Not while the left
hand holds something (the grip closed) or aims a spell.

**Open questions.**
- How long the stance must be held before the flick counts (a hand that
  passes the pose on its way elsewhere should not arm a roll): proposed
  0.15 s.
- The fist flick's speed and length thresholds: to be tuned in the headset
  from logged values, as the swings were ("m/s at its fastest", "m long").
- Left-handed play: the roles swap (the right hand dodges, the left holds
  the weapon), as everything else under `LeftHanded`.

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
  - **The power attack by the swing's length (2026-09-29).** "ich finde
    aber um einen powerattack zu machen muss dieser nicht viel stärker sein.
    stattdessen muss er die selbe schwung stärke erreichen UND mehr schwung
    distanz hinter sich zurücklegen." A swing is a power attack once the
    hand has travelled `PowerSwingMetres` (0.7 m) in it, at the swing speed;
    the peak speed (`SwingHeavy`) no longer decides (`SwingIsPower`,
    hand_mode_test: 0.8 m at 2 m/s is one, 0.4 m at 10 m/s is not). Row
    "Power swing length". The log gives each swing's length. Open: the
    swing's direction as the power attack's direction (next-up.md 2) and
    the grunt - under research.
  - **Swish, built.** Vanilla plays it in AttackHandling on a miss only
    (0x005FEC7D..0x005FEC95: 0x006AF880 cdecl(actor, 0.0, 0.0, no target,
    weapon type or -1, -1, -1, 0, 0), which picks Hand, or Small/Medium/
    Large by the weapon's speed, and plays it at the actor); the strike by
    motion calls AttackHandling only for a body it meets, so no swing
    sounded. Now every swing that may strike calls 0x006AF880 as vanilla's
    miss does, once, as it starts (`SwishDue`, `game::PlaySwingSwish`).
    Harness: "a swing's swish (weapon type 0)" per swing in `hand-bodies`
    and `fist-armed`. A hit plays the engine's hit sound as before.
  - **The grunt, built.** Vanilla says the PowerAttack combat topic (index
    10; topic pointer at 0x00B10EA8) for the player as a power attack
    starts, through PlayerCharacter's vtable +0x308 (0x006608A0,
    thiscall(player, target, topic index, interrupt), ret 0Ch; nothing
    while sneaking), when a roll under fCombatSpeakPowerAttackChance lands
    (0x0065EF10). OBVR does the same once per swing that becomes a power
    attack (`GruntDue`, `game::PlayPowerAttackGrunt`). Harness (a 1 m sword
    swing): "a power attack's grunt said (the game's chance 1.00)". Not
    heard yet: whether the player's voice has the lines. No grunt for light
    attacks - vanilla says Attack only for the combat AI (read).
- **Headset (the tester, 2026-09-29, the next run).**
  - "linke hand heben wenn kein schild equiped, darf nicht als block zählen
    ... wenn schwert in rechter hand ... dann soll ich mit rechter hand das
    schwert blocken können aber nicht mit linker hand ... wenn ich links ein
    schild trage dann soll ich damit weiter blocken können. same für
    zweihänder". **Built:** the raised left hand blocks only with a shield
    worn (`LeftHandBlocks`); the weapon held across (`IsWeaponGuard`, any
    drawn weapon, two-handers and fists included) blocks as before. The
    shield is the process's GetEquippedShieldData(true) (vtable +0xF8,
    0x0064B2D0 in both process tables; worn only, 0x00484E80), as the
    engine asks it at 0x00489A35 and 0x005FF7F5. A first try - something
    hanging on the left forearm's twist bone - read "a shield" as soon as a
    sword was drawn, and was dropped. Harness: `shield-block.txt` (Arena
    Iron Shield 000733ED equipped: "a shield worn", the raised hand blocks)
    and `shield-block-none.txt` (no shield: the same hand does not block)
    PASS. A getter that is not the one read keeps the old behaviour.
  - "keine der Perks ist mir ingame aufgefallen". The log showed the cause:
    of 20 strikes one was heavy - the blade met its target 0.3 to 0.6 m
    into a swing, before 0.7 m made it a power attack, and the swing became
    one only after. **Built:** a body met before the swing is a power
    attack is held and struck when the swing becomes one, or when it ends,
    as what the whole swing was (`SettleHeldStrike`, melee_hit_test); one
    met by a swing already a power attack is struck at once. The log names
    each strike ("struck ... - power attack (group 19)"). Not seen yet.
  - "Weit ausholen: Machtangriff mit Grunzen. Kurz und schnell: normaler
    Schlag. ja geht beides die werte die wir jz haben als default setzen":
    `SwingLight` 2.6 (was 1.6), `ShoveHardSpeed` 3.6 (was 4.0);
    `PowerSwingMetres` stays 0.7.

### 4.10 Two hands on a two-hander (built 2026-09-30)

The tester: "bei 2 händern mit der linken hand meine hand an das schwert/axt/whatever andocken kann mit grip und so zweihändig halte".

**Taking hold:** with a two-handed weapon drawn (claymore, war axe, battle hammer; the staff counts too), the left grip closing on the handle takes hold. On the handle means:
- within `TwoHandReachUnits` (12, about 17 cm) of the weapon's line;
- below the right hand, at least 5 units; above it is the blade and does not take hold (the tester, 2026-09-30: "damit es realistisch bleibt und ich nicht irgendwo in der luft dann halte");
- on the handle measured from the weapon's own model, when the grip closes: down to the pommel end (the lowest vertex along the weapon's axis, `game::AxialExtentOf`), and up to 6 units past it (put on the end). Unmeasured: down to 25 units below the right hand.
- The measurement is logged ("the two-hander's model - measured; pommel N units below the right palm ...").

**The hand on the handle - the game's own grip (reworked 2026-09-30).** The first build put the left hand at the handle's line but kept the controller's own turn of the hand, and the tester saw it float in the handle ("die linke hand ist am zweihänder aber nicht am griff sondern schwebt im griff. wir brauchen die vanilla hand die wirklich exakt den griff greift nur eben an der position die wir wollten"). Now:
- **Read from the animation.** While the left hand is not holding, the game's first-person two-handed animation puts the left hand on the handle. Each frame, before the hands are pinned, the left hand bone's pose is taken in the Weapon node's frame, with both palms along the weapon's axis (`vr::VanillaGripFrom`; a palm is halfway from the hand bone to its middle finger's base). It is kept only while the left palm is within 9 units of the axis (`vr::kGamePalmOnHandleUnits`) and below the right palm, and the last good one stays. The threshold is its own, not `TwoHandReachUnits`: widened to 60 for the harness, that let a frame of the draw animation with the palm 29 units off the axis pass (2026-09-30).
  - Measured in the game's files with pyffi (`_1stperson\twohandidle.kf`, 2026-09-30): the left hand bone sits 8.3 units down the axis from the Weapon node and 6.8 from it, steady through the idle. The right hand bone sits 2.1 down: the left hand directly under the right one, around the handle.
  - The attack animations (B-spline keys) were not read. In the game the pose is re-read every frame the left hand is free, and kept from the last frame it held the handle.
- **Put on the handle.** The left hand bone gets that pose against the weapon as the pinned right hand holds it this frame, moved along the axis from where the game holds it to where the grip closed (`vr::LeftHandOnHandle`).
  - It is kept between the pommel end (3 units inside it, half a hand) and the game's own place, right under the right hand.
  - Unmeasured, or a handle with no room below the game's place: the game's own place.
- **The fingers** are the animation's (`FingerPose::Animation`), the game's grip around the handle, not the controller's.
- **No jump (2026-09-30).** The tester: "es wäre cool wenn statt einem sprung der linken hand eine sanfte bewegung der hand an den griff passiert. idealerweise mit allen fingern auch die sich langsam zu einem griff formen ... ähnlich wie alyx".
  - Over `TwoHandBlendSeconds` (0.2 s) the hand moves from where its controller puts it to the handle, eased at both ends (`vr::StepTwoHandBlend`, `vr::TwoHandBlendWeight`). Position is blended straight, rotation by quaternion (`game::BlendRotation`).
  - The fingers blend the same share from their own pose (tracked curls or the grip curl) into the animation's grip (`StepHandFingers`' `towardAnimation`).
  - Letting go runs the same way back. A sheathed weapon ends it at once.
- **The player's own turn of the hand (2026-09-30).** The tester: "ziemlich geil aber noch ein wenig nauseating. denn die linke hand hat eine ausrichtung vom controller. wir zwingen aber die ingame hand an die waffe mit bestimmter position und angle. dieser ist anders als der natürlichen hand am controller ... fix die ingame hand bleibt beim gleichen angle wie die controller hand".
  - A fist round a handle can turn round the handle, and only that, without letting go of it. So the hand on the handle keeps the controller's own turn and is tilted only by the smallest turn that lays the handle it would hold along the weapon (`vr::LeftHandOnHandle` with the controller's hand rotation, `vr::RotationBetween`). The handle then lies through the fist exactly where the game's hand has it, and the animation's fingers still close round it.
  - Held exactly as the game holds it, that is the game's pose unchanged (`two_hand_test`).
  - What is still forced is the tilt: the angle between your hand and the weapon's line. With the controllers held like a real two-hander that angle is small. A hand held across the handle is tilted onto it.
- **Shaped on the way in, like Half-Life: Alyx (2026-09-30).** The tester: "wir machen die annäherung an den griff progressive wie in alyx. also je näher ich komme umso mehr geht die ingame hand schonmal in die passende handposition".
  - With its grip open, the left hand coming near the handle already takes the handle's pose and the fingers the grip's shape, more the nearer it is (`vr::PreshapeWeight`). Nothing from `TwoHandPreshapeUnits` (18, about 26 cm) from the handle's nearest point, all of it at 3 units, eased. `TwoHandPreshapeUnits=0` switches it off.
  - Its target is the nearest point of the measured handle (`vr::NearestOnHandle`), or the game's own place when the handle was not measured. Above the right hand is the blade, far from any of it: no shaping there.
  - Closing the grip goes on from where the approach had got to (`vr::HeldWeight`), not from the controller again. Opening it near the handle goes back only as far as the approach holds it.
  - The handle is measured once per model now (`game::AxialExtentOf`), not at each grip. A model that cannot be read yet is tried again about once a second. The tester's run of 2026-09-30 measured it on the first frame of the draw ("not readable") and never again, so the whole session had no handle. The harness shows the same first frame, then "measured" a moment later.

**Fixes and additions after the tester's run, 2026-09-30:**
- **Objects stuck to the left hand.** The tester: "Wenn ich mit links den 2 händer halte passiert es manchmal das objekte die ich rum haue an der linken hand kleben bleiben". The log showed the cause: the left grip on the handle also armed the grab, and the grab took what came near ("grab - left grip took 2FEC8600 within 1.00 m" while the handle was held).
  - Now the left grip belongs to the handle while it holds it, and while the open hand is near enough to be shaped more than half (`HandModeFrame::leftGripOnHandle`, `g_leftGripOnHandle`). In the hand mode it counts as open, like a grip at a holster: it neither grabs nor takes someone's hand (lead).
  - A grip that closes off the handle is refused, and then it is an ordinary grab again.
- **The grip lost now and then.** Every let go in that run was the grip reading open ("the grip opened", 22 and 35 times), so whether the controller flickered or the hand opened cannot be told from the log.
  - The grip may now read open for less than `TwoHandReleaseSeconds` (0.12 s) and close again without letting go.
  - Each such hold-on is logged, with how long it read open ("the left grip read open for N s and closed again"). That tells a flickering controller from a real release.
- **One-handers with both hands** (`TwoHandOneHanders`, default on; the tester: "ist es möglich den 2 hand auf auf einhänder zu haben?").
  - A one-hander's own animation keeps the left hand at the side, so the grip comes from the two-handed animation. The game's files hold it for when that has not been read yet this session (`vr::GameGripFromFiles`: twohandidle.kf's first key composed with pyffi, quaternions normalised). Its palm is 7.0 units below the right one, as the game showed it live every time.
  - The Weapon node hangs on the right hand the same way for both kinds (the same key in onehandidle.kf and twohandidle.kf).
  - A short handle with no room below the game's place is one place, at its pommel end: the left hand cups the pommel.
  - A grip up to a hand's width (10 units, was 6) past the pommel end still takes the handle. Two controllers sit no closer than that: the harness's left controller closed 13 units down against a longsword pommel 9.5 down.
- **The hand slides with its controller** (`TwoHandSlide`, default on; the tester: "ist es möglich diesen auch abhängig vom left conrtoller zu haben?"). While held, the hand's place on the handle follows the left controller every frame, kept on the handle. It lets go when the controller is more than 30 units from the handle, where before it let go when the controllers came 30 units further apart or closer.
  - **Only with the trigger held, as in Blade & Sorcery** (the tester, 2026-09-30: "machen wir wie bei B&S das das nur passiert wenn man auch während dem halten trigger drückt und hält. beim trigger loslassen klebt die hand dann fest"). With the left trigger held the hand slides; let go of it and the hand stays where it is (`vr::TwoHandSlides`).
  - While the handle is held, the left trigger does nothing else: no spell from that hand (`HandModeFrame::leftTriggerOnHandle`; the hand mode still reads it as `leftTriggerDown`).
  - Harness `two-hand-grip`, 2026-09-30 13:42: PASS (artifacts/hand-script/two-hand-grip/20260930-134250). "slides along the handle with the trigger held - from 12.9 to 13.4 units below the right palm" (13.4 is the handle's end). The sideways move before, without the trigger, is not shown to have left it in place; `two_hand_test` covers that.
- New settings rows (Hands): "Two hands on one-handers" and "Left hand slides on the handle".
- The log says where the game's hand was read ("the game's left hand on the two-hander read - its palm N units below the right palm") and where it was drawn ("the left hand drawn on the handle - its palm N units down the weapon from the right palm (wanted M)").

**While held - the weapon hand leads (2026-09-30):**
- The weapon stays with the weapon hand, exactly as with one hand; the left hand goes to the weapon, never the weapon to it. The tester: "die dominante hand sollte rechts sein wenn nicht linkshänder gesetzt ... sie sollte aber immer zugunsten der dominaten hand ... bleiben ... die linke hand muss dann einen sprung an den griff machen statt andersherum". The build before turned the weapon onto the line between the controllers, so taking hold swung it towards the left hand.
- The weapon hand is the game's right hand. With `[Hands] LeftHanded=1` the two controllers swap roles as a whole (`vr::AssignHandRoles`), so it is always the dominant controller, and the off hand is the one that takes the handle.
- The left hand is drawn on the handle as above, wherever the left controller is.

**Letting go:** opening the left grip for more than 0.12 s, sheathing, or the left controller more than 30 units from the handle for more than 0.12 s.

**Code:** `vr/TwoHandLogic.h` (`two_hand_test`), `game/HandGrip.h` (`finger_test`: the quaternion of a rotation and the blend), and `StepTwoHands` in `CameraHook.cpp`. `PinLeftHandOnHandle` pins the left hand after the right one, so it sits on the weapon where the right hand now holds it.

**Tested:**
- The tester, in the headset, 2026-09-30 (the game's grip, before the weapon hand led): "an sich gut das die hand sich nun um den griff schließt". The log: the game's hand read 7.0 units below the right palm, taken 7.0 to 15.9 below and drawn where wanted.
- Harness `two-hand-grip`, 2026-09-30 11:08, with the weapon hand leading and the blend: PASS (artifacts/hand-script/two-hand-grip/20260930-110821). The hand was taken 13.4 units below the right palm and drawn there. The scripted head shows no hands in the window picture, so the blend itself was not seen - only its end.
- The tester, in the headset, 2026-09-30 (the weapon hand leading, the blend): "ziemlich geil aber noch ein wenig nauseating" - the forced angle of the hand. The log: taken between 8.2 and 13.4 units below the right palm, drawn where wanted.
- Harness `two-hand-grip`, 2026-09-30 11:48, with the hand's own turn and the approach: PASS (artifacts/hand-script/two-hand-grip/20260930-114833). The open hand reached the handle's full shape at 13.4 units below the right palm before the grip closed ("already shaped 1.00"), and was drawn there. The turn and the approach are not seen in the window picture.
- The tester, in the headset, 2026-09-30 13:23 (the own turn and the approach): "jo das ging also super! sehr gut gemacht!" Found: objects stuck to the left hand, the grip lost now and then, and the handle never measured all session (all above).
- Harness 2026-09-30 13:34-13:35, with the fixes above: `two-hand-grip` PASS (artifacts/hand-script/two-hand-grip/20260930-133536) and the new `two-hand-one-hander` PASS (…/two-hand-one-hander/20260930-133443).
  - The longsword: "not readable yet", then "measured; pommel 9.5 units below the right palm". The hand was taken 6.5 below and drawn there, cupping the pommel end.
  - Neither run logged a left-grip grab while the handle was held (a reject line).
  - The slide and the flicker hold-on are covered by `two_hand_test`, not by the harness.
- The tester, 2026-09-30 14:00: "geht gut nun ... klebt nix mehr. mit trigger kan ich gleiten". Still: the grip lost now and then during or after a swing with both hands, one-handers "da passiert noch nix", and staffs working only in the right hand. What the log showed and what was done:
  - **Staffs, and any second weapon: another weapon's handle.** The handle was measured once per "Weapon" node. That node belongs to the skeleton, and every weapon hangs on it. A staff drawn after a longsword and a claymore kept one of their handles all session, so grips 14, 36 and 48 units down its shaft were refused.
    - Now the handle is measured per weapon form, once the draw is over (player action -1). During the draw the node can still carry the old model.
    - Harness `two-hand-switch` (staff, then claymore in one run): PASS (artifacts/hand-script/two-hand-switch/20260930-141437). The staff measured its pommel 84.1 units below and took a grip 25.8 down. The claymore then measured 16.4, refused the same grip ("closed off the handle - -26 units along"), and took one at 12.9. The staff alone: `two-hand-staff` PASS.
  - **The game's grip read from an attack.** Two takes gave "the game's own hand at 14.6" and "16.4": frames of an attack animation that passed the check. The game's grip is now read only while the player does nothing (action -1). The idle holds it at 7.0.
  - **Off the handle during swings.** Three lets go were "the hand left the handle", right after swings, and none of the grip lets go was a short flicker (no "read open for" line). A hand more than 30 units off the handle now has to stay off for 0.12 s, like the grip: a tracking jump of a frame or two holds on. The other 32 lets go were the grip reading open for longer than 0.12 s. The log cannot tell a hand that opened from a controller that did.
  - **One-handers.** Grips were taken (7.0 to 11.7 units down) and let go a moment later, and two were refused at 1 and 5 units down: the left controller closing right beside the right one. `minUnits` is 2 now (was 5).
  - **The weapon turned in the hand by the animations** (found on the way). `_1stperson\twohandattackleft.kf` keys the Weapon node up to 17 degrees from the idle's pose and back. The idle, the block and the one-handed idle hold it still (pyffi). In Full VR the weapon, and a left hand on its handle, turned with it during every strike and draw.
    - `HoldWeaponAtRest` now keeps the pose the Weapon node has while the player does nothing, per model, and writes it back while any action plays. It runs before the hands are pinned, for one- and two-handed weapons, not the bow.
    - The harness logged the draw turning it up to 27.7 degrees ("an animation turned the weapon ... (player action 0) - held at its rest").
    - `hand-bodies`, `holster`, `holster-two-handed` and `shield-block` still PASS with it.
- The tester, 2026-09-30 15:19: two-handed swings "ja besser". Still: a staff has "keine freie Wahl wo ich den anfassen will mit links. scheint nur einen ort zu geben", on some staffs the hand "greift ... ganz weit oben und nur dort"; on a one-hander the left hand "zum griff sich ausrichtet und dann mit offener hand im griff schwebt". What the log showed and what was done:
  - **A staff hangs 37 units further down the hand.** The left hand was put from the right palm as the game's grip file has it (-0.9 along the Weapon node). On the staff the palm is at -37.6 (measured live now), so the harness put a hand wanted 12.9 below the right palm 23.8 above it, and in the tester's run the one take on the staff drew the hand far up the shaft. The right palm is now measured with the handle (`g_twoHandRightPalm`, logged "the right palm ... along it") and each frame the hand is pinned (`LeftHandOnHandle(..., rightPalmAxial)`).
  - **A staff's shaft above the right hand was refused as a blade.** The tester's staff grips closed 24, 26 and 35 units above the right palm ("closed off the handle"). A staff's handle is now its shaft on both sides of the right hand (`HandleSpan::above`, from a fist's width past the right hand to the far end, inset). Sliding keeps the hand on its side of the right hand (`vr::SlideAxial`).
  - **A hand held the other way round** is tilted onto the handle the other way, not turned half round (`LeftHandOnHandle`): a fist holds a shaft either way up.
  - **The open hand on a one-hander.** The left fingers on the handle were the animation's; a one-hander's animation has the left hand open at the side. They are now the game's own grip from `twohandidle.kf` (`game::kHandleLeft`, pyffi, constant keys), on every weapon, and the approach blends towards it (`FingerPose::Handle`, "the left hand's fingers close round the handle").
  - Harness 2026-09-30 15:38-15:40: `two-hand-staff` PASS (artifacts/hand-script/two-hand-staff/20260930-153820): the right palm -37.6 along the staff, a take 12.9 below drawn at 12.9, takes 14.4 and 7.0 above the right palm. `two-hand-one-hander` PASS (…/20260930-153918) with the fingers line; `two-hand-grip` PASS (…/20260930-154009), the claymore's right palm at -0.9 as before. That the fingers look closed in the headset is not seen by the harness (the hands are off the harness view).

- The tester, 2026-09-30 15:53: on the staff and on a one-hander the left hand "ist nicht in einer linie vom stab zur rechten hand sondern greift iwo in den leeren raum"; the two-hander fine. What the log showed and what was done:
  - **The game's grip read from a staff.** The takes on the second staff logged "the game's own hand at 18.0", the one-hander drawn after it "at 1.5". The grip is read live from the drawn two-hander's idle; the staff counted as a two-hander, and its own animation, the left hand elsewhere, passed the check. That grip (the hand's place and turn against the handle) was then used on every weapon after it. It is now read from blade and blunt two-handers only (`IsStaffWeaponType`).
  - **The handle's line.** The left hand was put on the Weapon node's axis. A sword's handle is modelled on it; a staff's need not be. The line is now moved across to where the model's vertices lie within 4 units of the right palm along the axis (`vr::ShaftOffset`, `AxialExtentOf`'s band), and the hand arrived logs how far both palms are off that line.
  - Harness 2026-09-30 15:56-16:01: `two-hand-staff` PASS twice (…/20260930-160153): the Staff of Apotheosis's shaft 0.5 and -0.9 off the axis (26 vertices); a take 25.8 below with the left palm 3.8 off the line, the right 4.3. `two-hand-one-hander` PASS (sword 0.0 0.0 off, palms 3.8 and 3.3), `two-hand-grip` PASS (as before). `two-hand-switch` FAIL twice: the claymore's console equip at a mark did not take (the staff icon stays in the HUD at "claymore"; a SteamVR window was over the game), so no claymore was drawn - the runner, not the two hands.
  - Not reproduced in the harness: the tester's staffs (pommel 44 to 63 below, one with the right palm at -26) and the grip read at 18.0. That the fix puts the hand on those staffs is for the headset.

- The tester, 2026-09-30 16:23: "jo nun viel besser". The staff "ungefähr in einer linie noch etwas versetzt aber schon akzeptabel", the one-hander "passt nun"; wanted: "sobald man das schwert mit links hält gerne noch stärker locken an das schwert sodass beim schwingen nicht der griff verloren wird". What the log showed and what was done:
  - **Locked on the handle.** Both of that run's lets go by distance ("the hand left the handle") came right after a swing. Held, the hand now stays on the handle however far its controller swings away; only the grip opening (for 0.12 s) lets go. `TwoHandSlackUnits` (new, default 0 = locked) gives the old release by distance back.
  - **The staff a little off.** The tester's staff measured "the shaft there 0.0 0.0 off its axis (0 vertices)": no vertex within 4 units of the right palm along a long plain shaft, so the node's axis was kept. For a staff the band now widens to 20 units when the narrow one has too few vertices (`kShaftWideBandUnits`). Not seen on that staff yet: the harness staff has 26 vertices in the narrow band.

### 4.11 The bow by hand, as in Blade & Sorcery (built 2026-09-30)

The tester: "Pfeil und Bogen wie in blade and sorcery. Linke hand hat ja bereits den Bogen. Jetzt muss rechte noch auf rechter schulter den Pfeil bekommen und an den bogen führen. Beim loslassen Schuss. Zielen im groben mit links mit dem bogen, im feinen mit rechts dem Pfeil. Crosshair muss voll sichtbar sein wenn Optionen dafür an ist und man zielt damit."

**The shot, step by step** (`vr::StepArchery`, `vr/Archery.h`, `archery_test`; reworked 2026-09-30 evening after the tester's second round, see below):
1. **Take:** with the bow drawn (the bow hand already holds it, 4.2), the weapon hand's **trigger** pulled at the quiver takes an arrow (`ArrowWithGrip=1`, settings row "Arrow with grip": the grip instead). The quiver is over that hand's shoulder, behind it: `QuiverX/Forward/Up` (0.15, -0.12, -0.10 m from the eyes), `QuiverRadius` 0.20 m, mirrored with `LeftHanded`.
   - A button already down when the bow comes out takes nothing.
   - From then on that button is the arrow's: it neither grabs nor attacks.
2. **Nock:** the arrow brought within `NockMetres` (0.25 m; 0.15 until 2026-10-01: "pfeil anlegen darf noch großzügiger sein etwas. also früher erkannt werden") of the bow hand **from behind** is on the string: at least 2 cm behind it along the bow and no further across than behind - a 45-degree cone (`vr::FromBehind`; the tester, 2026-10-01: "der pfeil nur auf die sehne springt wenn man ihn von hinten an die stelle am bogen führt. von vorne kommen oder andere seiten soll natürlich nicht gehen"). Before the bow's axis is known, any side.
3. **Draw:** pulled back **along the bow** past `BowStringMetres` (0.15 m, the string's rest; apart from the nock's reach, so the wider nock does not make the draw begin later) and `BowDrawStartMetres` (0.08 m) further, the engine's own draw begins: its attack control held.
4. **Loose:** the button let go lets the control go, and the engine looses as vanilla does on release - at once (below).
5. **Ease** (the tester: "oder wieder zurück wie am anfang"): the hand brought back to the bow while drawn (the pull along the bow within `BowStringMetres`) takes the draw back - no shot. The arrow stays on the string.
   - Vanilla cannot take a drawn arrow back (reddit.com/r/oblivion/comments/piwuhr: "You supposedly can't without a mod"). OBVR does what the DenockArrow mods do (github.com/dannywarren/Oblivion-DenockArrowToo, src/DenockArrowScript.txt): `player.playgroup unequip 1` while the attack control is still held, the control let go 0.3 s later (`kDenockHoldSeconds`), then `player.playgroup idle 1`. The lines run through xOBSE's console interface from an xOBSE task (`game/ConsoleLine.h`).
   - The interface answers false for both lines and they take all the same: the harness saw the player's action go 5 -> 12 (ScriptAnimation) -> -1, never 3 (the follow-through of an arrow that left); a real loose goes 5 -> 3 -> -1.
6. **Off the string:** nocked, the hand moved `BowUnnockMetres` (0.12 m) off the bow's line - and out of the nock's reach, or it would come off as it goes on - takes the arrow off the string, into the hand. **Put back:** let go at the quiver, the arrow is put back ("inklusive pfeil wegstecken"); let go elsewhere it is dropped. No arrow is spent either way - the engine takes one only when it looses.
- The bow put away with an arrow out: dropped, no shot.
- **The trigger no longer draws the bow** while `BowByHand` is on (settings row "Bow by hand", default on). Off, it draws as before.

**Aim** (the tester, 2026-09-30: "zielen doch nur noch mit linker hand wo der bogen ist"; that evening: "wenn der bogen equipped ist den aim ändern auf dahin wo der pfeil fliegen würde"):
- The shot goes **where the bow points as the game holds it**: its model's +x, read each frame after the hands are pinned (`game::BowShotAxis`), taken into the bow hand's controller frame (`vr::ControllerLocalOf`) and carried by that controller the next frame (`HandModeFrame::bowShotLocal`). Before the bow is seen, along the bow hand's laser. The drawing hand does not steer it.
- **The bow hand's laser goes the same way** while the bow is drawn by hand: its angles are the shot's (`vr::LaserAnglesOf`, CameraHook's `LaserAnglesFor`) for the crosshair on that hand, its pick and the item search. The beam itself is only drawn at menus, which keep the settings' angles.
- Before (2026-09-30 afternoon) the bow model was turned onto the laser instead (`vr::BowFacing`, removed). The tester wanted the bow back in the game's own grip: "linke hand wieder wie sie in vanilla war vor unseren changes einfach normaler griff 90 grad am bogen".
- **How it has to be held:** with the live INI's hand calibration the shot runs down the controller's handle, (-0.07 -1.00 -0.04) in its frame (harness 2026-09-30, at the nock). To shoot ahead the controller's handle has to point ahead - in the harness it is turned up 90 degrees, and the bow stands upright. Whether that is comfortable in the hand is the headset's to say; the hand calibration decides it.
- It goes into the aim-at-source pose ahead of `AimWithHand` and the gaze; the line is kept 0.4 s after the loose (`vr::StepArrowAimHold`), as the engine makes the arrow a few frames later.

**The crosshair:**
- While an arrow is nocked or drawn, the crosshair hangs on the shot's line, ahead of the bow at the crosshair's distance, facing the eyes (`CrosshairLayer::SetRoomPlacement`).
- It is wanted while aiming whatever "only when needed" says (`CrosshairVisibility::aiming`), and still not with the crosshair switched off or under a menu (`frame_logic_test`).

**What is seen** (built 2026-09-30, `game::StepBowVisual`, `game/BowVisual.h`; the tester, 15:19, had seen nothing of it: "da passiert nix ausser dass ich die motion mache und dann wenn ich spanne oder loslasse ... dann kommt die ingame animation"; 2026-09-30 evening: "also schonmal super das es geht!"):
- **The arrow, from the quiver on.** At the take OBVR makes the arrow the way the engine makes its own at the draw's Attach key: the ammunition's quiver model holds an "Arrow:0", and 0x005FCFD4-0x005FD03F clones it (NiObject::Clone, 0x00700900) and adds the clone to the bow's `ArrowBone`. OBVR clones the same "Arrow:0" (its length read from the model, 46.6 units head to nock on the iron arrow) and places it each frame after the hands are pinned:
  - **in the fist: through its middle, straight ahead along the hand** - the line from the hand's bone to its middle finger's knuckle (`Bip01 R Finger2`), turned 15 degrees down from it, away from the fist's grip axis (the tester, 2026-10-01: "der pfeil muss noch 30 grad runter damit er geradeaus schaut", then "fast da. jetzt bitte 15 grad hoch"; `vr::kArrowInFistDownDegrees`) - the nock 3 units behind the fist's grip point (the tester, 2026-09-30 evening: "rechte hand der pfeil zeigt nicht wie der laserpointer sondern hier auch einfach mittig in der hand gerade aus"; `vr::ArrowInFist`). Before it lay on the laser, and before that along the fist's grip, pointing up.
  - **on the string: on the bow's line** through the arrow's rest, (0, 2.8, -2.45) in the bow's frame, where `bowattack.kf` lays it at full draw; the nock as far back as the fist, no nearer than the string at rest and **no further than a full draw** (the tester: "wenn man maximale spannung des bogens erreicht hat ist stopp. mehr spannen geht dann nicht"; `vr::ArrowOnBowLine`). The drawing hand is moved onto that point, so it stays on the string and stops with it ("kann man den nur noch in einer linie ... ziehen").
  - It hangs on the first-person root, not on the hand's bone, and its bound is kept in view like the hands'. It goes when the arrow is loosed, dropped or put back; a new ammunition makes a new one.
  - The right hand's fingers close round it while it is held.
- **The bow as the game holds it** in the bow hand, not turned by OBVR (the aim follows it, above).
- **The draw sound in its parts** (the tester, 2026-09-30 evening: "den bogen spann sound müssen wir whl stückeln so dass das spannen des bogens und andere teile davon einzeln abspielen"; `game/BowDrawSound.h`, `vr::BowSoundsFor`):
  - The engine plays one sound for the whole draw: `bowattack.kf` has "Sound: WPNBowDraw" at 0.033 s, made by the key handler at 0x0051B059 (call 0x006AE0A0). WPNBowDraw is 00097C38, `fx\wpn\wpn_bowdraw.wav`: 1.58 s, 16-bit mono 44100 Hz; silent to 0.30 s, a first part to 0.98 (two clicks), silent to 1.04, a louder second part to the end. Which part is the string's stretch is read off the loudness and the animation's timing, not heard.
  - While the bow is drawn by hand that call answers "no sound" for WPNBowDraw (the handler skips the key), and OBVR plays the parts itself: 0.30-0.98 s at the nock, 1.04-1.60 s as the draw begins, the second cut off whenever the bow is not drawn (loosed, eased, off the string, dropped). Each cut fades over 5 ms.
  - The file is read from a loose `Data\Soundx\wpn\wpn_bowdraw.wav`, else from `Oblivion - Sounds.bsa` (`game/SoundSlice.h`, sound_slice_test); played on a DirectSound device of OBVR's own on the game's window, at the game's master times effects volume (OSGlobals' sound system +0xB8 and +0xC4, from Oblivion Reloaded's `SoundControl`; the harness read 1.00 and 0.80). Not placed in 3D. If any of it fails the engine's sound stays whole.
  - The loose sound (`bowShoot`, at the release key) is the engine's, untouched.
- **The engine's own arrow is hidden** while the bow is drawn by hand (`ArrowBone` culled), and `ArrowBone` is put where the hand's arrow is, so what the engine reads off it at the release is the arrow the wearer saw.
- **The string follows the hand.** The bow's `BowMorph` weight is written after the animation and blended by the morpher's own blend (0x006D0CF0; its weights at +0x44, the flag it waits for at +0x58, found in its Update 0x006D13C0 and blend 0x006D0C30). The weight is how far the nock is behind the string's rest along the bow over the full draw's travel; rest and travel are read from the morph itself (the iron bow: -15.6 and 28.1 units).
  - After the loose the string is held at rest until the engine's shot is over (its action back to -1, at most 3 s). After a dropped arrow it is set to rest once. Otherwise it is the game's (`vr::StepBowString`).

**The loose at once** (the tester, 2026-10-01: "nach dem loslassen erwartet man einen direkten schuss. oftmals kommt aber eine sekunde verzögerung! das muss weg!"; `game/BowRelease.h`, `vr::StepReleaseSnap`, archery_test):
- Cause, read from Oblivion.exe (static, 2026-10-01): the engine looses in the attack update's action-5 case (0x005FD106) only when the draw's slot-3 key counter (ActorAnimData +0x54) reaches 3 - the Release key, 1.433 s into `bowattack.kf`. Held, the draw pauses at Hold (1.367 s; 0x0065EF38-0x0065F045); let go before it, the draw plays on to Release first. A hand draws in half a second: the arrow left up to a second after the trigger.
- So, a frame after the loose (the engine sees the control up first, or it would pause at the Hold again) and only with the arrow attached (action 5; before the Attach key there is no arrow, and the counter steps once a frame), the draw's sequence is put at its Hold: its offset set to KeyTime(2) - clock, the same write the engine makes reaching it (0x004771B5-0x004771D8), on the first- and third-person draws. The engine then plays Hold to Release, 66 ms. Let go before the Attach (0.267 s) it waits for it; given up after 2 s.
- The power is not changed by it: it is a timer of its own (player +0x640, grown while the control is held; power = min(1, fArrowBowTimerBase 0.25 + fArrowBowTimerMult 0.4 * timer), 0x005FD278-0x005FD2BF): full power after about 1.9 s held, about 0.8 at the Hold. A quick shot is a weak one, as in vanilla.
- The tester, 2026-10-01, after that build: still "wenn ich nur kurz spanne ist 1s pause dann kommt der pfeil". The log of that run shows the snap working ("put at its Hold (1.367 s, it was at 0.268 s)", then 5 -> 3 within frames): the engine looses at once. What was left was the arrow itself: let go after a third of a second the bow timer gave power 0.36, and the arrow's speed is 1500 * (p + (1 - p) * 0.01) units/s with fArrowWeakSpeed's floor - the harness probe saw such an arrow 304 units from the eyes a second after the loose, crawling. That is fixed by the power from the draw (below).

**Legolas-quick** (the tester, 2026-10-01: "wenn ich sehr schnell spanne dann gibts die pause. also Legolas Style Schiessen net möglich!"):
- Cause: the Hold snap above waited for the arrow to be on the string (action 5, the Attach key at 0.267 s). Let go before it, the draw played on to the Release key: the harness measured "the arrow left 1.515 s after the let-go" with no snap at all, 0.381 s with the Hold snap alone.
- So the draw is moved on key by key (`vr::StepReleaseSnap` ToAttach then ToHold, `game::SnapBowDrawToAttach`): to its Attach while the action is still 4, the engine attaches the next frame as if the draw had reached it, then to its Hold. One key at a time - the counter steps once a frame, and case 4 attaches only on 1. A draw not yet showing (the action still -1 on the loose's frame) is waited for up to 0.25 s.
- Harness `bow-by-hand` 2026-10-01 13:12: PASS (artifacts/hand-script/bow-by-hand/20261001-131202): a draw let go 0.1 s in, "put at its Attach (0.267 s, it was at 0.000 s)", "the arrow left 0.135 s after the let-go"; the 0.5 s draw "0.092 s". Each loose now logs that time ("the arrow left ... after the let-go", or "NO ARROW" after 3 s).
- Harness only: the console's `player.equipitem` equips the arrows one at a time, so each shot left none equipped - the quiver gone, the next draw refused (re-equipped after a shot the next one drew). The tester's game equipped from the inventory shot one after the other. The script equips again after its first loose.

**The power from how far the string is drawn** (the tester, 2026-10-01: "in vanilla scheint das ja iwas mit wie lange der bogen gespannt wird. das geht in vr nicht. wir müssen das ändern auf wie weit der bogen gespannt wurde vom user. kommen noch ander faktoren dazu? skill oder so?"; `vr::BowTimerForDraw`, `vr::StepBowPower`, `game::WriteBowPowerForDraw`, archery_test):
- Vanilla, read from Oblivion.exe (2026-10-01): for the player, power = min(1, fArrowBowTimerBase + fArrowBowTimerMult * timer) at 0x005FD278-0x005FD2BF; other actors shoot at 1. The timer is player +0x640, zeroed when a draw starts and grown by the frame's time only while the control is held (0x0065ED7B). The settings: fArrowBowTimerBase 0.25 (initialised 0x009E9AA0 from 0x00A41304), fArrowBowTimerMult 0.4 (0x009E9AD0 from 0x00A47E6C); no .esm/.esp in Data names either. Full power after 1.875 s held.
- **No skill in it.** The one other factor: power times 0.1 when the actor's process reports movement flag 0x800 (0x005E0530, process vtable +0x2C0; 0x005FD3FF). Which movement that is was not looked up - left as the game has it.
- What the power does (the arrow's constructor 0x0060C940, from the analysis of 2026-10-01): its speed is the ammunition's speed * fArrowSpeedMult (1500) * (p + (1 - p) * fArrowWeakSpeed 0.01); its damage is (the weapon's + the ammunition's damage, both through 0x00484F80 with the actor) * p - the skill, strength and the rest of the hit's formula stay the game's, apart from p; fArrowWeakGravity (1.75) is mixed in by (1 - p) (0x00547700, its caller not traced).
- So the timer is written instead of grown: while the string is drawn by hand each frame, and from the loose until the engine looses, for the string's weight w (0 at rest, 1 at full draw, the weight its morph is given): timer = (1 - base) * w / mult, so power = base + (1 - base) * w - vanilla's least at rest, full at a full draw, by the game's own settings. A draw whose weight was never read keeps the game's timer.
- Harness `bow-by-hand` 2026-10-01 10:53: PASS (artifacts/hand-script/bow-by-hand/20261001-105303): "Bow power: the draw 1.00 gives the bow timer 1.87 s - power 1.00 (fArrowBowTimerBase 0.25, fArrowBowTimerMult 0.40)"; the second draw let go 0.5 s in at "a draw of 0.63", put at its Hold, loosed; the run before (10:51) saw that arrow 1068 units off 1.5 s after the loose, where the slow one of 10:45 was 304 units off after a second.

**Feasibility: the arrow back in the hand after an ease** (the tester, 2026-10-01: "hätte aber erwartet den pfeil zurückzubekommen und wieder in den köcher legen. ist so aber auch ok recht gamey"; analysis only, nothing built):
- No arrow is spent by an eased draw: the ammunition is taken only in the loose's path (0x005FD4A4, the actor's vtable +0x2E8, the player's 0x00662590); the Attach takes none (read 2026-10-01). So the arrow is still in the inventory - putting it "back" is OBVR's picture and state only: after the ease go to `InHand` instead of `Nocked`, the arrow in the fist, put back at the quiver as today.
- One clash found: the Attach hides one arrow of the quiver's model (`or word [node+0x18], 1` at 0x005FD0D6, "Arrow:0" or "Arrow<count-1>"); after an ease it stays hidden. 0x005F8300 (thiscall actor, root, 0; ret 8 - the player calls it with both roots at 0x00660C58/0x00660C68) shows the quiver's arrows again from the count; OBVR would call it after the ease. Not verified: whether the engine's clone on ArrowBone is taken off on the eased path (OBVR hides it anyway).
- Verdict: feasible, small - a state change in `vr::StepArchery`, the quiver's refresh, and a harness check of the arrow count before and after.

**What the engine still decides:**
- The draw's power: the time the control is held (the timer above). How far the hand pulls does not change it.
- Where the arrow starts: only its direction is set.

**Not built:**
- Power from the pull's length.

**Tested (what is seen):** harness `bow-by-hand`, 2026-09-30 18:19: PASS (artifacts/hand-script/bow-by-hand/20260930-181935).
- "an arrow made for the hand from the quiver's ..., 46.6 units head to nock"; "the string measured - at rest -15.6 along the bow, 28.1 units to full draw"; drawn at weight 0.54; after the loose "at rest", then the game's.
- The eyes' dumps (SteamVR's, `dump-*-eyes.png`) show the arrow in the closed right fist, then on the string from the fist through the bow.
- The game window's picture is NOT the check: in it the arrow on a drawn string did not show while the eyes had it (the runs of 17:38-17:58). Cause not found; the dumps are what the headset gets.
- Not seen in a picture: the string's bend itself (the log's weight and the morph's blend are the evidence), and a real headset.
**Tested (the second round):** harness `bow-by-hand`, 2026-09-30 19:50: PASS (artifacts/hand-script/bow-by-hand/20260930-195023): the trigger took, "the arrow in the hand along the laser", nocked, drawn along the bow, "at full draw - stopped" (weight 1.00), eased ("the player's action 5 -> 12", "the eased draw ended with no arrow loosed"), off the string, "put back in the quiver", a second arrow taken, drawn and loosed (5 -> 3), then both sticks held: "Camera: both sticks held - recentering".
- The eyes' dumps of a variant with the bow held ahead (the script's positions along the laser, the head level) show the bow upright, the arrow along the fist's laser, and drawn the arrow from the fist through the bow.
- Not seen in a headset: all of it. Not measured: whether the eased draw's `playgroup unequip` shows a sheathe flicker, and whether the engine's power still follows the time the control was held after an ease.

**Tested (the third round):** harness `bow-by-hand`, 2026-09-30 22:06: PASS (artifacts/hand-script/bow-by-hand/20260930-220616). "the bow shoots along (-0.07 -1.00 -0.04)", "the arrow in the fist along the hand", nocked ("the first (nock) part played"), drawn ("the second (stretch) part played", "the engine's WPNBowDraw at the draw's key kept quiet"), full draw stopped, eased (5 -> 12 -> -1, no arrow loosed), off the string, put back, a second draw loosed (5 -> 3), the sticks' recenter.
- The eyes' dumps show the bow upright in the left fist as the game holds it, the arrow lying straight ahead out of the right fist, and drawn the arrow from the fist through the bow.
- Not run this round: the other harness scenarios. Not heard: the parts themselves - the log says they played, nobody has listened. Not seen in a headset: all of it.

**Tested:** harness `bow-by-hand`, 2026-09-30 14:52: PASS (artifacts/hand-script/bow-by-hand/20260930-145217).
- The arrow was taken (the hands 0.56 m apart), nocked at 0.10 m, and drawn at 0.38 m "along the arrow's line".
- The attack update ran at action 5 with "heading 0.0000 set to 6.1524 and pitch -0.0000 to 0.0521": 7.5 degrees left and 3 degrees down. That is the line from the scripted right hand through the bow, which sat 5 cm left of and 2 cm below it, 38 cm ahead.
- After the loose an arrow reference (form FF000B9E, type 0x22 ammunition) lay 669 units away.
- The crosshair's placement is not in the window picture: not seen.
- The runner's console at a mark leaves the menu state up for some seconds; the script waits 12 s for it.
