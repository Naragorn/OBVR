# Controls in Full VR

Status: the layout in section 1 is **built** (808eecd, 2026-09-27). The target
picture in section 4 is **not built**; it is the tester's, written down on
2026-09-27. Full VR only (`[Hands] Enabled=1`). The seated mode is played
with a gamepad or keyboard and is not touched. With the mode off and
`ControllerMenus` on, the controllers act as a 360 pad (`PlanGamepadControls`,
docs/hand-tracked-mode.md).

## 1. The layout today

Told to the player since 2026-10-15 in three places that have to be kept the
same by hand: README.md "Controls in Full VR" (a table, Index and Quest), the
settings menu's Help row "Show the controls" (ui/NativeControlsCard.h, two
pages by role: the weapon hand, the other hand) and the first start's chain,
which shows that card (ui/FirstStart.h, docs/onboarding-spec.md 3.2). The
Oculus Touch has no trackpad: there the hotkey ring is the right stick's
button held (4.4) and there is no journal key.

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
| Stick click | short: the quick menu's ring (4.4); held: the Rest menu (T: wait, or sleep in a bed) while weapons are drawn by reaching, with `Holsters=0` ready / sheathe the weapon | short: the spell ring (4.44); held: run |
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
| Move, run, jump, sneak | left stick, its click held, right stick up, right stick down | built |
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
| The prison tutorial's cards | the key names in the mode's words, the mouse-bound cards swapped whole (onboarding-spec 3.4) | built 2026-10-16 |

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
  - **Open (2026-10-07):** the tester: "die faust in den gegner klappt oft
    nicht richtig". The log of that round shows 17 fist swings after the
    fists came up, the nine with a strike line all striking the same NPC
    (light and power attacks, the health going down each time) and no
    "struck nothing" line; the strike lines stop at their limit, so the
    later swings are unseen. Not pinned: whether swings miss, or land
    without the reaction the tester expects - the tester, later that day: "da
    erwarte ich ein punch sound eig. der kam nicht immer". The punch sound
    is the engine's own: the attack handling OBVR calls (0x005FEBF0) plays
    the weapon sound with the target from three sites (0x005FFB89,
    0x005FFCE5, 0x005FFD61 - the hit variants, a target and a ninth argument
    the miss call leaves 0), OBVR plays only the swish. Not pinned why it
    stays out at times: whether those sites are not reached for some
    strikes, or the sound is there and drowned by the swish played on the
    same swing. Needs the strike lines unlimited for one round, or a fight
    probe in the harness.

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

- **The stick's button held opens the ring too (built 2026-10-15; the tester:
  "für quest hotkey button wird der rechte stick button long pressed. quest
  button gibts nicht").** The Oculus Touch binding
  (assets/input/oculus_touch.json) has no trackpad: 16 of the manifest's 18
  actions are bound, the two trackpad actions are not, so a Quest had neither
  the ring nor the journal key. Now the right stick's button held past
  `[Hands] QuickMenuStickHoldSeconds` (0.4; the settings row "Quick menu: stick
  hold") is the ring's on every controller (`vr::StepStickHold` in
  vr/HandInput.h, `HandModeResult::ringByStick`, OR-ed into the ring's pad in
  CameraHook's `UpdateQuickMenu`): from the hold on the press belongs to the
  ring, its release uses the slot the hand is on and is no click of the stick -
  no Rest menu, no ready weapon. A press the chord took (both sticks within a
  quarter second) or one made with the ring switched off is never the ring's,
  and a short click stays what it was. The hold is longer than the chord's
  window on purpose. The journal key (F1, the left trackpad's click on the
  Index) still has no Touch binding; the tester wants that press to become a
  spell ring (4.44). Since 2026-10-16 he wants the stick clicks to open the
  rings on a Quest only, the Index's rings on its trackpads alone (the open
  item at the end of 4.44). Tests: hand_mode_test, the hold's flows and the mode's
  (short click, the hold, the chord, the ring off). Harness
  `quick-menu-stick.txt` PASS (Linux, 2026-10-15): the ring opened at 0.9 s of
  hold with the save's slot 3 ("3 Iron Longsword"), "hotkey 3 used" on the
  release and no Rest menu after it; a short click then opened the Rest menu
  (SleepWait), and the right B closed it.
- **Swapped the same evening (the tester: "vertauschen wir auf quest rechter
  stick halten -> warte menü. kurz quickmenü. links same mit halten -> quest
  menü").** The right stick's *short* click is the ring's tap: the ring opens
  latched (`QuickMenuInput::tap`, `QuickMenuState::latched`), the hand moves
  to a slot, the next click is the release (in the middle: closed); the trigger
  turns pages meanwhile. The right stick *held* past `[Hands] StickHoldSeconds`
  (0.4, row "Stick hold"; moved out of the quick menu's settings) is the Rest
  menu's key at the moment the hold is reached (ready weapon with the holsters
  off), its release nothing. The left stick stays the walk's and the run's
  alone (the tester, right after: "linker stick muss frei bleiben auf quest
  für laufen! jaja dann links nur laufen"); a journal key exists on the Index's
  left trackpad only. quick_menu_test (the latch: tap opens, tap uses, tap in
  the middle closes, not allowed unlatches, the trackpad beside it),
  hand_mode_test (the right stick through the mode). Harness
  `quick-menu-stick.txt` rewritten and PASS (Linux).
- **Fixed on the way (2026-10-15): the right B in the Rest menu opened the
  pause menu over it.** The frame's `restMenuUp` reached the plan only on the
  menus-only path; Full VR's `Step` never handed it on, so `PlanHandControls`
  sent Escape instead of the Rest menu's own key (T). One line in
  HandMode.cpp; hand_mode_test "The Rest menu's own key through the mode"; the
  harness run above closed the menu with the right B ("menu 0" at
  `rest_closed`).

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
- **Open bug (the tester, 2026-10-15): "hotkeys im inventar belegen geht
  nicht mehr da laserpointer sich wegbewegt vom item wenn man die
  entsprechende slot besetzen will."** Noted, not built ("notiere"). The cause,
  read from the code: the ring's `holdsCursor` only zeroes the mouse steps
  (`g_hand.cursorDx/Dy` in CameraHook's `UpdateQuickMenu`), but since
  2026-10-10 the cursor is also placed directly before the engine's tile search
  (4.36, `LaserCursorDirect`, default on: `HandModeResult::cursorWanted/X/Y`,
  HandMode.cpp `placeCursor`), and that placement is not held - so while the
  ring is up the cursor follows the laser to the slot, and the click lands
  beside the item. The fix is small: with `holdsCursor` the direct placement
  is dropped for the frame (or kept at the pixel of the frame the ring
  opened), as the mouse steps are. The proof is `quick-menu-assign.txt`, which
  passed 2026-09-27 before the direct cursor and has not been rerun since.
  **Built the same evening (the tester: "ja mach"):** with `holdsCursor` the
  direct placement is dropped for the frame too (`g_hand.cursorWanted=false`
  in `UpdateQuickMenu`), so the engine's cursor stays on the item while the
  ring is up. Harness `quick-menu-assign.txt`: PASS 2026-10-16 on Linux, the
  scenario moved to the 2026-10-01 save (slot 5 empty before, the laser on
  the Iron Longsword row): the placed cursor stayed at 856,587 through the
  ring, "setting hotkey 5 to what the cursor is on in the Inventory menu
  (empty until now)", one click on the "Iron_Longsword" tile, "the hotkeys
  now: ... 5 Iron Longsword", "hotkey 5 used (Iron Longsword)". Seen on the
  way (open): with the laser on the top row (Elven Long Sword, rot 68) the
  ring closed "without a hotkey" before the trackpad was let go - the
  `assign` input (the menu under the cursor) must have left the inventory
  for a frame there; the cancel path says nothing about why, a reason in
  that log line would (small).

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
- **The spot.** A gold circle at the chest (a faint disc with an edge
  line, firmer while the hand is in it - `ui::PaintStowSpot`; a ring with a
  clear middle until 2026-10-07), shown while an item is held: let go there
  and the item is stowed; let go anywhere else and it drops. It is a sphere
  of 0.16 m round a point 0.17 m ahead of the eyes and 0.33 m below them, in
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
    takes it along with that hand; opening the grip leaves it. The right
    stick up and down grows and shrinks it (0.12 m of radius a second at
    full deflection, past the stick dead zone; 0.06 to 0.40 m - since
    2026-10-07). Done writes `StowRight`, `StowForward`, `StowUp`,
    `StowRadius` and hides the circle; Cancel or Esc puts spot and size
    back; Reset returns both to the default. Kept within reach: 0.6 m to
    either side, 0.4 m behind to 0.8 m ahead, 1.4 m below to 0.3 m above the
    eyes (`vr::StepStowPlace`, tested in stow_test).
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

### 4.12 Rudeness: the middle finger, the slap, and talking past the things (built 2026-10-07)

The tester's asks of 2026-10-07, all in one round:

- **NPC before items.** "npc vor objekten, damit man immer in dialog gehen
  kann auch wenn objekte eig näher an der hand wären". An NPC under the
  pick hand's laser (`game::ActorUnderRay`, within 3 m and 8 degrees) takes
  the pick from the items (`game::NpcTakesPick`, pick_hold_test): the pick
  runs along the laser and Activate talks to them. Only a touched item still
  wins - a hand on a thing is not pointing past it. Logged once: "Pick: an
  NPC under the laser takes it from the items".
- **The middle finger** (`game/Insult.h`, insult_test; `[Hands]
  MiddleFinger=1`, `MiddleFingerDisposition=10`, both in the menu). From the
  controller's finger curls: the middle finger out (curl 0.35 or less), the
  index, ring and little fingers curled (0.6 or more), the thumb free. Held
  at an NPC under that hand's laser (4 m, 15 degrees) for 0.4 s, their
  disposition drops by the amount, once per gesture; the same NPC not again
  within 8 s. Logged "Insult: the right hand's middle finger at <ref>".
  **Without pointing (the same day, 8e12223):** "mittelfinger muss aber
  auch gehen ohne dass ich mit dem pointer auf npcs zeigen muss" - with no
  NPC under the laser the gesture goes to whoever is ahead of the head
  within 60 degrees, else to the nearest living NPC within 3 m. The tester:
  "mittelfinger nun perfekt" (2026-10-07). The default loss is 30 now
  (`MiddleFingerDisposition=30`).
- **The slap** is the shove (`docs/combat-comfort-spec.md`, 4.9's
  neighbour `game/Shove.h`): an open hand driven fast into someone, weapons
  away. Light (1.5 m/s): they stagger and are pushed, their disposition
  drops, no crime. Hard (3.2 m/s): knocked down, more disposition, and it
  counts as a hit (`ShoveCountsAsHit=1`: friends forgive a few, others call
  the guards). New: a light shove landed in the face - 35 cm or more above
  the body's middle (`SlapInTheFace`) - costs `[Hands] SlapDisposition`
  (10) instead of the light shove's 5 (shove_test). That is the tester's
  "ins gesicht slappen ... crime default off; mit viel wucht haut es den npc
  um, crime default an, mehr dispo verlust".
  **Its sound and the cheek (the same day).** A slap in the face plays
  OBVR's own slap, `assets/sounds/slap.wav` (one slap cut from a CC0
  recording, see the README there), shipped as `OBVR_Sounds/slap.wav` next
  to the DLL and played through winmm (`game::PlayPluginWave`). With Put it
  in its Place - Enhanced Grabbing active (its ESP in Data and on the plugin
  list, `PutItInItsPlaceLoaded`) a coin decides each slap between OBVR's
  and the mod's own noise, and the slapped one grabs their cheek the way the
  mod's slapper script does it: its idle marker token, `pickIdle`, the token
  off again, run as them through xOBSE's console interface
  (`RequestConsoleLineAs`). Without the mod the game's own gasp
  (`NPCHumanGaspMale`) and no idle: the mod's assets are not OBVR's to ship.
  `SlapSoundFor`, `SlapLines` (shove_test). Not seen in the headset.
  **The tester's round of it (the same day):** the sound "zu leise" and to
  be "mehr comic haft" like the mod's - OBVR's slap is now the CC0 slap
  layered with a CC0 whip crack (BigSoundBank 2949), at full scale; the
  mod's lines "hat nicht gestartet": the lines by editor id answered false
  and did nothing, so the mod's forms go by form id with its load index in
  the top byte (`PutItInItsPlaceIndex`: the index under which its slap
  noise and its slapped token are found in the game's form table;
  `SlapLines`, `SlapFormLine`). The tester's log of that build: `pickIdle`
  answered 1, every line with the mod's form id (`addItemNS 0B005335 1`,
  `playSound3D 0B005339`) answered 0 - the console takes the line, not the
  mod's forms by id. Why is open (the same line typed in the console is the
  normal way); nothing of the mod's ran.
  **The mod's own way in (8e12223, then fixed the same day).** What its
  slap actually is, read from its scripts (`zzPiiiPGrabQS`,
  `zzPiiiPzFunctSlapperInit`, `zzPiiiPzFunctSlapperReactionHandler`,
  extracted from the ESP; its readme: "NPCs can be slapped by tapping the
  grab key when pointing the crosshair at their face"): on the grab
  control's down (xOBSE control 28) it takes the crosshair's reference; a
  living NPC starts a count of the frames the control stays down - 12 or
  more is its pickpocket ("Grabby"), let go within 7 is its "Tap-Slap". The
  slap then wants neither side in combat, the player facing the NPC within
  25 degrees and the NPC facing the player within 18 ("I can't slap %po
  from this side - I need to face %po"), the NPC not seated, the two of a
  height (scale within 0.1, origins within 12 units), the origins within 51
  units ("Damnation ..., %ps's out of reach") and the crosshair's spot on
  the head (its `LocationGrabber`: the player's pitch under about 9 degrees
  down at that range). Then both are restrained, the NPC is moved 50 units
  before the player, both play idles (the player's `slapper.kf`, the NPC's
  `slapped.kf` - the hand to the cheek - with its slap noise), the NPC may
  dodge or block by speed and skill, and its handler takes half of their
  disposition, has them curse (over disposition 40), slap back (65 %) or
  challenge the player to a duel (First Blood or To the Death, by level and
  skill). So with the mod loaded a slap in the face is a grab tap with the
  pick on them from the head (`SlapByModsGrabTap`, `SlapGrabTapPressed`:
  one frame of the pick there first, then the grab down for 3), and nothing
  of OBVR's own - no stagger, push, fatigue or disposition
  (`SlapLeftToMod`; its positioner wants them where they stand, its handler
  costs them their liking). Logged "Shove: <ref> slapped in the face ...
  left to Put it in its Place: a grab tap". In 8e12223 the tap never
  reached the game: the grab key was set before the grab reach
  (`StepGrabReach`) decided the same key and overwrote it - the tester's
  "put in place script kam nicht", with no sound either, since OBVR's stays
  out of the mod's way. Fixed: the tap is OR-ed in after the reach. Not seen
  in the headset. What may still stop it there: the mod's 51 units between
  the origins (73 cm - a hand at a face from an arm's length may stand the
  body further off), and the NPC having to face the player; both are the
  mod's own checks and show as its messages. The tester's round of 86225fa:
  seven taps logged, nothing visible of the mod's. Each tap frame now logs
  the key's state and whom the crosshair has ("Shove: the grab tap, n
  frame(s) left - the grab down this frame, the key down before 1, the
  crosshair on <ref> (the slapped one <ref>), the player in combat n") for
  the mod's side.
  **The tester's round of 0987362 (the same day):** four taps logged, the
  crosshair on the slapped one from the tap's second pressed frame, "the key
  down before 0" on every frame - which was the log's own fault: it asked
  for the Z key while the scan code sent (0x2C) is the Y key on the
  tester's German layout (`keybd_event` with KEYEVENTF_SCANCODE takes
  the layout's key; the game binds the scan code, Grab=002CFFFF in its
  INI). Still nothing of the mod's. So the log now reads the mod's own
  state from its quest scripts' variables (`ReadModQuestVar`,
  `ModQuestActive`; xOBSE's TESQuest +0x58 ScriptEventList, m_vars at
  +0x0C; the quests' and variables' ids from its ESP) for 24 frames from
  each tap: its grab quest running or not, sNPCGrab (1 on the down-edge
  with an NPC under the crosshair, counted up while held, 0 once its branch
  ran), sIsGrabbing, rGrabbedItem, sEnabled, sSlapper - and the key by the
  layout. What the next log says decides whether the mod ever sees the tap.
  **Answered by that log (the same day):** the key went down (the layout's
  Y, three frames), the crosshair was on the slapped one from the tap's
  second pressed frame, the mod's grab quest ran with sEnabled and sSlapper
  1 - and sNPCGrab stayed 0 while rGrabbedItem went to 0: the mod took the
  grab's down-edge one game frame before the crosshair reached the NPC,
  found nothing under it, and started no count. So the tap aims for three
  frames before the grab goes down (`kSlapGrabAimFrames` 3, the press 3,
  six in all, within the mod's seven). Not seen in the headset. The hard
  shove on the same NPC knocked them down: "stärker haute den npc um lol
  geil".
- **Yielding by gesture** (`vr/Yield.h`, yield_test; the tester: "waffe
  einstecken ... mit offenen händen ... leicht von aussen nach innen wippen
  ... zählt das als ein yield"). The weapon away, both hands open (index
  curl under 0.5), someone in combat within 6 m and 30 degrees of the
  head's forward: both hands' sideways positions are watched, and four
  direction reversals after strokes of 5 cm or more within 2.5 s - two
  out-and-in cycles - are the yield. Then block is held for 30 frames and
  activate pressed from the 10th to the 16th of them (8 frames with
  activate down at once fired in the log on 2026-10-07 and the NPC fought
  on), and the pick runs from the head at that NPC: vanilla's yield (block
  and activate facing the attacker), which the NPC may still refuse. Not
  again within 3 s. Logged "Yield: the open hands rocked at someone in
  combat". The palms' facing is not checked - open hands rocking at an
  enemy with the weapon away is gesture enough.
  **Open (2026-10-07):** the tester's second round had no yield line at
  all - the gesture never counted, and the log did not say why. Now the
  rocking is watched on its own as well (`YieldBlockedBy`, yield_test):
  when it completes while the gesture is not allowed, "Yield: the hands
  rocked, but no yield - <reasons> (hand curls r/l; under the head: <ref>, in
  combat n)" names what stood in the way. The tester's round of 86225fa
  gave 24 of those and no yield: "no one in combat ahead of the head" on
  every one, and the index curl 0.6 to 1.0 with the hands held open - the
  index rests on the trigger - so a hand is open by its middle, ring and
  little fingers now (`YieldHandCurl`). The tester's "yield mit der geste
  scheint zu klappen" that round was not OBVR's yield: none fired; the round of 212fa21: "yield klappte einmal nicht, dann klappte es sogar mit ingame succces" - the miss had the hands at curls 0.54 to 0.64, so a hand is open under 0.7 now (a fist reads 0.9 and more); the round after (0987362) fired three times, "the player's action -1" (the action unreadable), the crosshair on them twice, and the NPC refused - the tester: "yield klappte paarmal, wurde aber vom npc rejected, was ja ok ist"; and when activate goes down, "Yield: activate goes down after 10
  frames of block - the player's action <n>, the crosshair on <ref>" shows
  whether the engine took the block and whom the pick had. A thing to
  settle with that line: vanilla's block is "with weapon or shield" (UESP
  Oblivion:Controls), and the gesture wants the weapon away - if the engine
  takes no block with the weapons sheathed, the key route cannot yield
  this way, and the yield would have to be called in the engine directly
  (the activate-while-blocking branch; not located yet). The button route
  of 2026-09-29 (raised hand and A) yielded with the weapon drawn.
  **Hardened (the tester, 2026-10-12: "teilweise kann es noch passieren
  dass ich ein yield event bekomme obwohl ich nur waffen oder hände
  schwinge. die mit offenen handflächen zum gegner (egal ob mit oder ohne
  waffe oder schild) und beide hände leicht nach rechts und links wippen
  für ein paar sekunden - geste, muss etwas mehr gehärtet werden damit es
  nicht einfach so im combat passiert").** The first version counted
  sideways reversals alone - of either hand, at any speed, in any pose -
  and the tester's logs of 2026-10-11 show the rocking "complete" twenty
  times a session from ordinary hand movement, held back only by "no one
  in combat ahead" or the weapon in hand. Now (`vr/Yield.h`, yield_test):
  - the weapon in hand or away is no longer a gate. The engine's yield is
    block and activate, and its block needs a weapon or the fists up: with
    nothing readied, activate on the attacker opened their pockets (the
    harness, 2026-10-12: "Container (0x3F0)" on the bandit right after
    "activate goes down ... the player's action -1" - which answers the
    open question above: the key route cannot yield with the weapons
    sheathed). So with the weapons away the fists are raised first (the
    ready-weapon click, followed until the game shows them up, `YieldDrive`
    in CameraHook.cpp), the block and activate follow, and the fists raised
    for it are lowered again after ("Yield: the fists raised first ...",
    "the fists are up after n s", "done - the fists raised for it are
    lowered again"). The pick's target follows the attacker while the
    block is held (one walking on in those 20 frames put the crosshair on
    nobody, harness run 4). The tester's earlier "yield klappte paarmal,
    wurde aber vom npc rejected" with the weapon sheathed was, by this,
    the pocket menu or the talk refusal, not a yield - unverified;
  - each hand has to be tracked and open (as before), held up before the
    eyes (no lower than 50 cm under them, ahead of them) with its fingers
    up (the pinned hand bone's knuckle direction at least 12 degrees above
    level - the palms out), not swinging (the hand mode's swing, and for a
    second after it), and slower than 0.9 m/s over the last 0.1 s;
  - a stroke longer than 30 cm is a sweep, not a rock; a reversal within
    0.12 s of the last is a tremor's and one after more than 1.2 s a
    drift's, neither counted;
  - four reversals within 3 s, at least two from each hand and at least a
    second from the first to the last, are the yield; a gesture complete
    within the 3 s cooldown is dropped, not kept to fire the moment the
    cooldown ends (the unit test found it firing with the hands at rest).
  Any gate failing resets the count. The log's "the hands rocked, but no
  yield - <reasons>" now names these (too fast, too low, not ahead,
  fingers not up, swinging, a stroke too long, one hand alone, over in
  under a second), with the curls, the knuckles' rise and the hands'
  height under the eyes; a hand script's mark logs the count and the
  last reset. Harness `yield.txt`: a bandit attacking an untouchable
  player, the gesture with the hands empty (the yield), fast sideways
  swings of both open hands (no yield, "too fast"), the sword drawn and
  the gesture again (the yield). The pose numbers (50 cm, 12 degrees,
  0.9 m/s) are this round's first guess, to be set from the headset.
  **The palm, not the fingers (the tester, 2026-10-12: "yield geht nun gar
  nicht mehr wenn ich es als geste mache").** His log: all twenty "rocked,
  but no yield" lines came before his first fight (the log budget spent,
  the fights after logged nothing - neither a refusal nor a yield), and of
  the thirteen rockings with both hands open and high enough, twelve failed
  the fingers-up gate on a hand (the knuckles' share up 0.12, 0.09, 0.11,
  -0.05, -0.16 ...; 0.2 needed): how a hand is held open round an Index
  controller does not lift the bone's knuckles. What happened in the fights
  is not in the log. Now:
  - the gate is the palm turned to the front, the way he describes the
    gesture: each palm - the controller's side the fingers close over, its
    -x on the right hand and +x on the left, as the hand HUD and the item
    pick read it - at least half along the head's forward (within 60
    degrees of it; `vr::YieldPalmAhead`, `kYieldPalmAhead`). A fist's
    knuckles, a blade's grip, a shield's strap and a slap's sideways palm
    face elsewhere. The log names it "the right palm not to the front" and
    gives "palms to the front r/l";
  - the log's budgets apart: "rocked, but no yield" 40 lines while
    fighting (the player in combat, someone in combat before the head, or
    the ring counting fighters), 4 outside a fight; the yields and their
    block and activate their own 40.
  - Harness `yield.txt` with the hands turned palms forward (right rot 90 0
    90, left rot 90 0 -90): PASS 2026-10-12 - the hands empty: "palms to the
    front 1.00/1.00", the fists raised, block, "activate goes down ... the
    player's action 6, the crosshair on" the bandit; fast swings: "the right
    hand swinging, the left hand swinging"; the sword drawn: the yield again.
  **Open bug (the tester, 2026-10-14 night): "yield gingen gar nicht mit der
  geste."** His session of 21:39 to 21:49 (Windows, the block reach build):
  44 "the hands rocked, but no yield" lines and no yield. What stood in the
  way, counted over the 44: the left palm not to the front 44 (its share
  -0.80 to 0.27, never near the 0.5 asked), the right hand closed 37 (curl
  1.00 - the longsword in that hand, the fingers round the grip), the right
  palm not to the front 35, too fast 17, swinging 16, the left hand too low
  12, the left hand closed 11, the right hand too low 9, no one in combat
  ahead 8. The one line with every other gate open (in combat, the right
  hand open at 0.44 and its palm 0.63 to the front, both hands 0.37 and
  0.15 m under the eyes, nothing swinging) failed on the left palm alone, at
  -0.12; where the right palm read 0.77 to 0.89 the left read -0.80, -0.48,
  -0.32 and 0.26. Two things to settle, nothing built:
  - *The left palm's side as read.* `vr::YieldPalmAhead` takes the
    controller's x axis from the hand's rotation relative to the head
    (`HandMode.cpp`, the same conjugate-and-convert for both hands), +x on
    the left, as the hand HUD's `PalmNormal` does on the OpenVR pose - the
    sides agree in the code, and the harness cannot tell them apart (its
    left roll of -90 was chosen to satisfy the code, "palms to the front
    1.00/1.00"). What a real left hand reads when he turns its palm to an
    enemy is unmeasured: a debug trace of both palms' shares every half
    second while both hands are up and open, done once on purpose, would
    settle whether the side or his pose is the cause.
  - *The weapon hand counts as closed.* The gesture allows a weapon in hand
    but still wants that hand open, and a hand holding a sword keeps its
    fingers round the grip (1.00 in 37 of 44). The HUD's look already treats
    the weapon hand as open (`MeleeInHand`, above); the yield should do the
    same, or leave the weapon hand's curl out of the gate.
  **Built (2026-10-16, the tester: "wir machen nun: Yield-Geste repariert").**
  - *The side read is not the fault - settled from his own INI.* The fit of
    his hands (`[Hands] LeftHandRoll=157.6445`, `LeftHandPitch=71.0317`,
    `LeftHandYaw=65.8923`; the right -10.2, 53.7, 86.2 - written by adjusting
    the hands on his controllers, `FitHandToPose`) is the rotation from the
    bone's axes to the controller's. The Bip01 hand bone's palm is its +y:
    in `skeleton.nif` the Weapon node hangs 1.37 units along +y of the right
    hand and the Torch node 1.37 along +y of the left, and in
    `handtohandidle.kf` (the fists) every fingertip moves to +y of its hand
    (the index 6.1 units back along the fingers and 4.1 to +y, on either
    side; read with pyffi, `tools/palm-axes.py`). His
    calibrations turn that +y to the controller's (0.99 -0.05 0.12) on the
    left and (-0.99 -0.08 -0.10) on the right: +x and -x, the sides
    `vr::YieldPalmAhead` reads, within 8 degrees. So the -0.80 to 0.27 his
    left palm read were the pose of his controller - the hand itself, or the
    controller turned on its strap in a hand holding nothing - which the
    code cannot tell apart; the gripped sword hand read true (0.52 to 0.69).
  - *The gates now:* a hand holding a drawn weapon
    (`YieldHandInput::holdsWeapon` - a blade, blunt weapon or staff in the
    weapon hand, the bow in the bow hand, from the equipped weapon's type
    and `ReadPlayerWeaponState`) is not asked to open, and it is the hand
    whose palm is asked to the front (its controller lies in the palm; a
    guard's palm faces sideways). An open hand is shown by its open fingers
    alone; its palm's share is read for the log, not gated. The gripping
    hand's cone is 73 degrees (`kYieldPalmAhead` 0.3, the first guess 0.5):
    his session of 2026-10-16 evening, on the build before this one, read
    the same way as the night's - 37 rockings, no yield, the left palm never
    past 0.38, the sword hand closed 31 times - and over both sessions his
    sword hand's palm read 0.31 to 0.46 in a third of the attempt-like
    rockings (the left hand open and high, in combat) and 0.62 to 0.96 in
    the rest; what a guard's palm reads is unmeasured. Either hand's
    palm sign follows the physical controller whatever role it plays
    (`right != leftHanded` - before this a left-handed player's weapon hand
    was read with the right's sign). yield_test: the gripped weapon hand
    with its palm to the front yields, to the side or the face it does not
    (named), the open hand's palm any way yields, the bow hand likewise, both
    gripping both asked, the margins. README and the controls card say the
    gesture as it is: both hands up and open, palms to the foe, rocked
    gently left and right.
  - *For the headset's numbers:* `Yield: the pose - palms to the front r/l,
    the fingers' share up r/l, under the eyes, ahead, curls; the right hand
    gripping a weapon / open ...` twice a second while both hands are shown
    and up before someone in combat, 60 lines a session. The fingers' share
    is the pinned hand bone's x in the world (`ReadHandBoneWorld`). With
    these the open hand's reading is settled next session: a left palm that
    reads to the front when he holds it so says the controller follows the
    hand, and the open hand's palm could be asked again; one that does not
    says it cannot be.
  - Harness `yield.txt` regenerated (`tools/gen-yield-script.py`)
    with two phases more: the hands empty and both open palms turned
    sideways (rot 90 0 0) still yield; the sword in hand with its palm turned
    sideways does not, "the right palm not to the front" at the mark; three
    yields counted.
- **The trigger never attacks with a melee weapon or the fists in hand**
  (the tester: "powerattacks mit trigger verbieten nun für alle waffen und
  ohne waffen"): `HandFrameInput::meleeInHand`, set whether or not the hits
  by motion are on (hand_mode_test). The bow keeps its trigger draw for the
  vanilla way; by hand its draw sets the attack itself.
- **The hand HUD with a weapon drawn** (the tester: "wenn waffe gezogen
  sehe ich das hud nicht mehr an der rechten hand"): the look at a hand
  wanted it open, and a hand gripping a drawn weapon is not; the weapon
  hand now counts as open for the look (`MeleeInHand`). And the first look
  at a hand stuttered: SteamVR makes an overlay's shared texture on its
  first SetOverlayTexture (~100 ms), so every hand HUD overlay is handed the
  atlas once, hidden, on the first submit after a load
  (`HandHudLayer::m_warmed`).
- **Open bug (2026-10-12): the take needs a firm grip.** The tester: "bisher
  braucht es ein festes grip bis die sich an der waffe einklinkt. das soll
  auch mit weniger druck möglich sein sodass man nicht "press" machen muss
  sondern einfach nur druck." The take is the grip's click, and on the Index
  the shipped binding clicks at a force of 0.3 (assets/input/knuckles.json,
  `click_activate_threshold`); the force itself is not read on the action
  path. Wanted: a force action with OBVR's own, lower threshold for the take -
  docs/free-weapons-spec.md section 4.1, with the rest of the tester's
  free-weapons wishes of that day (the weapon hand unlocked, world weapons
  equipped by their handle, half-swording) in section 4.
### 4.11 The bow by hand, as in Blade & Sorcery (built 2026-09-30)

The tester: "Pfeil und Bogen wie in blade and sorcery. Linke hand hat ja bereits den Bogen. Jetzt muss rechte noch auf rechter schulter den Pfeil bekommen und an den bogen führen. Beim loslassen Schuss. Zielen im groben mit links mit dem bogen, im feinen mit rechts dem Pfeil. Crosshair muss voll sichtbar sein wenn Optionen dafür an ist und man zielt damit."

**The shot, step by step** (`vr::StepArchery`, `vr/Archery.h`, `archery_test`; reworked 2026-09-30 evening after the tester's second round, see below):
1. **Take:** with the bow drawn (the bow hand already holds it, 4.2), the weapon hand's **trigger** pulled at the quiver takes an arrow (`ArrowWithGrip=1`, settings row "Arrow with grip": the grip instead). The quiver is over that hand's shoulder, behind it: `QuiverX/Forward/Up` (0.15, -0.12, -0.10 m from the eyes), `QuiverRadius` 0.20 m, mirrored with `LeftHanded`. **With no arrows left** nothing is taken - no sound, nothing to draw (the tester, 2026-10-01: "wenn ich keine pfeile mehr habe, dann kann ich auch keinen mehr graben und den bogen nicht spannen. also keinen sound beim grab"): the quiver has to hold an "Arrow:0" (`game::QuiverHasArrows`, `HandModeFrame::haveArrows`; the engine takes the quiver away with the last arrow, a harness run with none equipped found none). The game's ITMAmmoUp (0008B095) sounds at the take, ITMAmmoDown (0008B096) at the put-back (the tester, 2026-10-01, asked for a sound at the take; vanilla's draw has only WPNBowDraw and bowShoot).
   - A button already down when the bow comes out takes nothing.
   - From then on that button is the arrow's: it neither grabs nor attacks.
2. **Nock:** the arrow brought **from behind** within `NockMetres` (0.12 m) of **where the string rests** - `BowStringMetres` (0.15 m) behind the bow hand along the bow (`vr::NockPlace`; without the bow's axis, the bow hand) - is on the string. **Led there:** from `BowGuideMetres` (0.35 m) off that place the arrow in the hand and the hand are pulled towards the string, the more the nearer - with the square of the way come, so far off it is barely felt and near it pulls hard - all the way at the nock's reach (`vr::NockGuide`), and on the string the rest of the way; the arrow turns from where the fist points it onto the bow's line by the same share, not at the first pull (`vr::ArrowEasedOntoString`; the tester, 2026-10-01, of the even lead that turned it at once: "der pfeil am bogen spannt sich nun viel zu früh auf. dachte da eher an man nähert sich dem punkt und je näher man kommt umso mehr wird man geführt"); the distances are settings rows ("Bow: nock reach", "Bow: lead from" and the others under Hands); the pull follows at 5 a second (`vr::StepNockPull`), so neither the nock nor an ease or an arrow taken off jumps (the tester, 2026-10-01: "je näher ich zum anlege bereich komme je mehr gleitet die hand schonmal in diese richtung sodass ich geführt werde. das muss auch schon viel früher passieren als jetzt"). Before: the reach measured from the bow hand (0.15, 0.25, 0.20 m) with the hand sliding back from there onto the string; then 0.10 m around the string's place ("Ich muss sehr nah dran und suchen"). From behind: at least 2 cm behind the bow along it and no further across than behind - a 45-degree cone (`vr::FromBehind`), for the lead as for the nock. Harness `bow-by-hand` 2026-10-01 17:11: PASS - 20 cm off the string's place "led towards the string (0.27 of the way, the guide 0.68)", nocked 14 cm behind the bow hand.
3. **Draw:** pulled back **along the bow** past `BowStringMetres` (0.15 m, the string's rest; apart from the nock's reach, so the wider nock does not make the draw begin later) and `BowDrawStartMetres` (0.08 m) further, the engine's own draw begins: its attack control held.
4. **Loose:** the button let go lets the control go, and the engine looses as vanilla does on release - at once (below).
5. **Ease** (the tester: "oder wieder zurück wie am anfang"): the hand brought back to the bow while drawn (the pull along the bow within `BowStringMetres`) takes the draw back - no shot. **The arrow is back in the hand** (since 2026-10-01; before it stayed on the string): to be put back in the quiver, or nocked again once the hand has left the nock's reach and come back (`ArcheryState::awayFromNock` - at the bow it would go straight back on). When the cancel is through (`denockDone`) the quiver's arrows are shown again (`game::RefreshQuiverArrows`, below).
   - Vanilla cannot take a drawn arrow back (reddit.com/r/oblivion/comments/piwuhr: "You supposedly can't without a mod"). OBVR does what the DenockArrow mods do (github.com/dannywarren/Oblivion-DenockArrowToo, src/DenockArrowScript.txt): `player.playgroup unequip 1` while the attack control is still held, the control let go 0.3 s later (`kDenockHoldSeconds`), then `player.playgroup idle 1`. The lines run through xOBSE's console interface from an xOBSE task (`game/ConsoleLine.h`).
   - The interface answers false for both lines and they take all the same: the harness saw the player's action go 5 -> 12 (ScriptAnimation) -> -1, never 3 (the follow-through of an arrow that left); a real loose goes 5 -> 3 -> -1.
6. **Off the string:** nocked, the hand moved `BowUnnockMetres` (0.12 m) off the bow's line - and out of the nock's reach, or it would come off as it goes on - takes the arrow off the string, into the hand. **Put back:** let go at the quiver, the arrow is put back ("inklusive pfeil wegstecken"); let go elsewhere it is dropped. No arrow is spent either way - the engine takes one only when it looses.
- The bow put away with an arrow out: dropped, no shot.
- **The trigger no longer draws the bow** while `BowByHand` is on (settings row "Bow by hand", default on). Off, it draws as before.

**Block with the bow** (the tester, 2026-10-01: "dass man mit bow blocken kann wenn ich den senkrecht vor mir hochhalte"): the bow hand up and out in front (as the weapon guard: no lower than 20 cm below the eyes, at least `BlockMinForward` ahead) with the bow's limbs (its model's y, `game::BowLimbAxis`) within about 37 degrees of upright blocks (`vr::IsBowGuard`) - always, no setting, since it is vanilla's (the tester: "das ist vanilla funktionalität daher immer drin"). Vanilla blocks with a bow too, at half a shield's share (UESP Oblivion:Block: "Blade/blunt weapons or bows block 0.5% damage per skill level"), but "only if you don't have an arrow nocked" (UESP Oblivion:Marksman). OBVR goes one step further: no guard once an arrow is out of the quiver, because the bow is held just so to nock, and a block raised on the way to the string would still be coming down as the draw begins. With the bow by hand the empty weapon hand across the body no longer blocks. Harness `bow-block`: PASS 2026-10-01 - upright "blocking" (player action 6), laid flat released, upright again blocking, an arrow taken released.
 **Open bug (2026-10-01, test more later):** the bow's guard flickers - in the tester's log about half its blocks end within a few frames, so only some blows meet it; measurements and the proposed fix in `docs/next-up.md` ("Block with the fists and the bow").

**Stab with the arrow** (the tester: "dass ich mit dem pfeil rechts in der hand zuschlagen kann wie Legolas"): the arrow in the hand (not on the string) **thrust** into someone strikes - only a thrust, head first along the arrow within about 37 degrees (`game::IsArrowThrust`, measured once a frame by the bow's step; the tester, 2026-10-01: "Nur zustechen soll gehen, nicht schwingen wie eine klinge!"); a swing across is no stab. The blade is the arrow as the fist holds it (not as it is led towards the string), from nock to head, carried from the first-person skeleton into the world by the hand (`game::ArrowBladeInWorld`: that skeleton's world is shifted from the bodies' - the first version struck from there and met nobody in reach) (`vr::ArrowStabs`, `game::StrikeKindFor`, `game::ArrowInHandWorld`; settings row "Arrow stabs", `ArrowStabs=1`; needs strikes by motion). Vanilla has no such blow. The engine's hit function (AttackHandling 0x005FEBF0) does not turn a bow away (read 2026-10-01): with the bow drawn and no projectile it strikes for the bow's damage by Agility and Marksman (0x00484F80, type 5), the skill table 0x00B086A0 gives Marksman for the experience, and block, sneak, crime and the hit reaction are its own. So: the bow's damage, not the arrow's; no arrow is spent; the bow loses condition and its enchantment or poison applies; each stab is struck at once (no power attack). Harness `arrow-stab`: 2026-10-01 18:47 (before thrust-only) the engine's hit landed, health 26.0 -> 23.8; 20:38 the verdicts hold (the sweep across "swung, not thrust - no stab" at 0.13 along, the thrust "it stabs" at 0.99) but the beggar was out of reach - the headset was awake and tracked, so the harness anchored on its pose and the synthetic head stood 1.1 m too high (eyes at -28 instead of -140): the run FAILS on the hit until it runs with the headset asleep.

**Block with both fists** (the tester, 2026-10-01: "als h2h wenn ich beide fäuste vor mir hebe das muss auch als block zählen im unarmed combat"): with the fists up (hand to hand), both hands raised in front of the face - each as high and as far ahead as the weapon's guard, neither punching - block (`vr::IsFistGuard`). Vanilla blocks bare-handed (UESP Oblivion:Block: "using a shield, weapon, or your hands"; "Hand to hand blocks 0.25% damage per skill level, but only against unarmed opponents"). Harness `fist-block`: PASS 2026-10-01 - both up "both fists are up - blocking" (player action 6), one lowered released, both up again blocking. **Open (2026-10-01, test more later):** in the tester's fights no block was felt, though the engine counted 8 blows on the fists as blocked - vanilla's fists block nothing against weapons, the arms are hidden so nothing shows, and the guard flickers; see `docs/next-up.md` ("Block with the fists and the bow").

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

**The arrow back in the hand after an ease** (the tester, 2026-10-01: "hätte aber erwartet den pfeil zurückzubekommen und wieder in den köcher legen. ist so aber auch ok recht gamey"; analysed, then built the same day - step 5 above):
- No arrow is spent by an eased draw: the ammunition is taken only in the loose's path (0x005FD4A4, the actor's vtable +0x2E8, the player's 0x00662590); the Attach takes none (read 2026-10-01). So the arrow is still in the inventory - putting it "back" is OBVR's picture and state only: after the ease go to `InHand` instead of `Nocked`, the arrow in the fist, put back at the quiver as today.
- One clash found: the Attach hides one arrow of the quiver's model (`or word [node+0x18], 1` at 0x005FD0D6, "Arrow:0" or "Arrow<count-1>"); after an ease it stays hidden. 0x005F8300 (thiscall actor, root, 0; ret 8 - the player calls it with both roots at 0x00660C58/0x00660C68) shows the quiver's arrows again from the count; OBVR would call it after the ease. Not verified: whether the engine's clone on ArrowBone is taken off on the eased path (OBVR hides it anyway).
- Built: the state change in `vr::StepArchery` (archery_test, hand_mode_test) and the quiver's refresh at the cancel's end, 0x005F8300 checked by its first bytes before it is called. **Not run in the game yet** (the tester needed the monitor): `bow-by-hand` now eases, leaves the nock's reach, nocks again, takes it off and puts it back, and expects "the quiver shown again" and both quiver sounds. Not checked: the arrow count before and after an ease.

**The hand eased onto the string** (the tester, 2026-10-01: "wie beim 2 händer die hand progressive annähnern an die position statt direkt hinzuspringen"; `vr::StepNockBlend`, `vr::ArrowEasedOntoString`, archery_test): nocked, the drawing hand no longer jumps onto the bow's line. Over 0.2 s (eased in and out, as the two-hander's left hand onto its handle) the nock goes from where it lay in the fist to its place on the string, the arrow pointing through its rest on the bow so it swings onto the line, and the fist and the string's weight follow by the same share. Off the string it starts again from 0. Not seen yet.

**The bow hand's fingers** (the tester, 2026-10-01: "das die hand am bogen nicht ausgehöhlt ist wie sie jetzt ist sondern sich genauso verhält wie die leere hand"): while the bow is drawn by hand its hand's fingers follow the controller's (`game::FingerPose::Tracked`), as an empty hand's do; before, the bow on the hand's Torch node made it a hand holding an item (`game::HandHoldsItem`), which keeps the animation's hollow grip. Without finger curls from the controller they stay the animation's. Logged: "the bow hand's fingers follow the controller's, as an empty hand's". Not seen yet.

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

### 4.13 The tester's round of 331a7d9 (2026-10-07, evening)

- **The slap** "scheint nun zu gehen", at too much force: `ShoveSpeed` 1.3
  m/s now (was 1.5). The mod's "I can't slap %po from this side - I need to
  face %po": its check is the player's heading, which in Full VR is the
  walk's or the gaze's, not the hand's. For the tap's frames the body is
  turned to face the slapped one (`WritePlayerYaw`, Oblivion's heading:
  zero at north, clockwise). Not seen in the headset.
- **The dialogue's view** ("sobald der npc mit mir dialog startete ... die
  kamera wechselte runter zur hand ... während er die alte position
  anschaute die eigentlich richtig wäre"). The eyes the NPC looks at are
  the frozen headset eyes (DialogFocus) and were right; the view itself
  dropped. The menu frame's camera is built from `g_menuBasePos` (the
  engine's own camera, taken in the last camera pass) plus the head offset
  and the vertical offset - the same terms as a world frame - so where the
  drop comes from is not read yet. Logged once per conversation: "Dialogue
  view: the menu frame's camera at ... (base ..., offset z ..., vertical
  ...) - the last world frame's at ..., the eyes the NPC looks at ...". The
  next log decides. Open.
- **The pick caught on a small thing before a door** ("der pointer bleibt
  manchmal an objekten hängen obwohl ich direkt vor einer tür stehe"): a
  door, a container, an activator or furniture under the laser (within 3 m
  and 8 degrees, `game::ActivatorUnderRay`) takes the pick from a held item
  the way an NPC does (`NpcTakesPick`), unless the item is touched. Logged
  "Pick: a door or the like under the laser takes it from the items".
- **Taking by pointing and A** ("können wir ... per setting entfernen,
  default an"): `[Hands] TakeOnlyByHand` is that setting and is on by
  default now (it was off); `StowAtBody` was on already. A live INI that
  says `TakeOnlyByHand=0` keeps the old way until switched.
- **The crosshair quad over things** (CrosshairPlace=target): no higher
  than 10 units over a small thing's middle (`kTargetHoverCapUnits`; the
  bound spheres are generous - "schwebt der tooltip noch weit über den
  objekten"); it stays over the thing when the hand comes near instead of
  moving into the reach ring ("das sollte einheitlich sein"); and while the
  pick has moved on and the row's anchor has not followed (the 0.08 s
  settle) the quad is hidden rather than shown over the old thing ("für
  paar frames in der luft an einer falschen position"). The row's place
  (the name): `[HandHud] InfoPlace` - `target` in the world under the
  thing (the default), `view` the flat HUD, or a hand - in the HUD section
  of the menu. The "Crosshair place" row is in the Hands section (the
  Aiming section is VR View's and was hidden from Full VR, which is why
  the tester found no row).

### 4.14 Typing in the headset: SteamVR's keyboard (built 2026-10-07)

The tester: "für die wenigen stellen im spiel für die man ein keyboard zum
text eingeben braucht ... den standard best practice weg ... ein keyboard
zu rendern mit dem man mit dem laserpointer interagieren kann und text in
textfelder einfügen kann".

**Where the game takes text** (its menu XMLs, read from Oblivion -
Misc.bsa; the menu ids from xOBSE's GameTiles.cpp and UESP's entity list):
- TextEditMenu (0x41B): the character's name at the end of the tutorial, a
  custom class's name, anything else asked through CreateTextEditMenu - the
  `textedit_text` field with OK and Back.
- SpellmakingMenu (0x411) `spell_name_text`, EnchantmentMenu (0x412)
  `ench_name_text`, AlchemyMenu (0x410) `name_text`: a name field each,
  typed into once clicked.
- The RaceSexMenu (0x40C) has no text field (`race_name` is the race's).
- The console: not a place for the headset.

**The keyboard** is SteamVR's own (IVROverlay `ShowKeyboard`, entry 74 of
IVROverlay_028's table, counted from the header; the characters back as
`VREvent_KeyboardCharInput` through IVRSystem `PollNextEvent`, entry 30),
drawn by the runtime in the headset and typed on with the controllers'
lasers - the way every SteamVR title takes text. OBVR's part
(`vr/VrKeyboard.h`, vr_keyboard_test; `OpenVRBackend::ShowKeyboard`,
`PollKeyboard`; `game::TapKey`):
- it opens as the TextEditMenu comes up, and in the three naming menus
  when the laser's click lands on the name field or its background (the
  tile under the cursor, `game::ActiveTileName`), with a line over it
  naming what is being named (`KeyboardPromptFor`);
- each character the runtime sends becomes the key that carries it on a
  US keyboard, with Shift for a capital (`KeyStrokeFor`; letters, digits,
  space, `- _ , . ' "`; the runtime's `\b` is Backspace) - Oblivion reads
  its text from scan codes by the US layout - pressed one key a frame, a
  frame down and a frame up (`KeyTapQueue`): the game polls the keyboard
  once a frame, and a press with its release in the same frame is never
  seen. A character not on those keys (an umlaut) is left out and counted
  in the log;
- Done presses Enter for the TextEditMenu's OK; in the naming menus it only
  closes the keyboard;
- the keyboard goes with its menu, and with `[Hands] VrKeyboard=0` (Hands,
  "VR keyboard") it never opens.
Logged "Keyboard: SteamVR's keyboard opened for the ... menu", "Keyboard:
n character(s) typed ...", "Keyboard: closed with its menu". Not seen in
the headset; the harness cannot press the runtime's keys.

**The race and sex menu's view** (the tester: "beim char gen beim new game
die kamera kurz wie in vanilla vor dem player char setzen damit man sich
ansehen kann"): not built yet. The probe for it (`race-menu.txt`,
`showracemenu` from the console at a loaded save) crashed the game on the
menu's opening (c0000005 at 0x0051FE90, 2026-10-07, before any trace of
the menu in OBVR's log); whether OBVR's hooks are the cause is being told
apart with the hooks off (`race-menu-plain.txt`).
  **The probe's crash, read (the same evening):** the fault is in a
  four-instruction getter at 0x0051FE90 (`movzx eax, [ecx+0x48]; shr 3;
  and 1` - a TESHair's FixedColor flag, xOBSE GameForms.h), called from
  the race menu's own code at 0x005C350F, 0x005C5E31, 0x005C61A3 and
  0x005CBCB6 on the object at [player's virtual 0x170 result + 0x1C8] -
  TESNPC's `hair` (GameForms.h, 0x1C8) - which was not an object. The
  same crash with the hand mode off (`race-menu-handsoff.txt`), so not the
  hands'; whether OBVR's camera or render hooks are involved could not be
  told apart: with them off the script runtime does not run. The harness
  save was made from the player's own save (a character who went through
  the character generation), so a hair of none is not expected there
  either. Open: a run of `showracemenu` without OBVR at all, by hand, is
  the next evidence; and the chargen view itself is not built - it would
  be built blind otherwise, which the tester asked not to do ("mache
  keine fehler").

### 4.15 A tick at every button (built 2026-10-07)

The tester: "ein leichtes klicken der vibrationsmotoren der controller an
jedem button ... wenn man über dem hovert oder klickt, gleich wie beim
steamvr menü". `vr/MenuHaptics.h` (menu_haptics_test): a light pulse
(12 ms, 180 Hz, 0.25) as the laser comes onto a new thing it can press, a
firmer one (30 ms, 200 Hz, 0.6) as it clicks, through the hands'
vibration actions (`OpenVRBackend::Pulse`), in the hand that points. The
things: in the game's menus the engine's own tile under the cursor
(`game::ActiveTile`, InterfaceManager's activeTile, which the engine sets
for the tiles that take the cursor - the main menu's buttons, a list's
rows, the native settings' buttons included); OBVR's settings and
walkthrough panels by their row (`PointAtPanel`); the quick menu's ring by
its slot (the trackpad's hand). Leaving a button onto nothing pulses not.
`[Hands] MenuHaptics` (Hands, "Menu haptics"), on by default. Not seen in
the headset.

### 4.16 Open: the main menu's laser (2026-10-07, evening)

The tester: "kann im hauptmenü nicht mehr buttons anvisieren oder klicken
mit den laserpointer" (with 8716238). His log: the laser's hits on the
cinema picture are found and steps sent ("flat laser hit pixel 2138,88 -
cursor at 0,0, step 60,44", then 60,60 every frame), but the engine's
cursor reads 0,0 throughout, and its pick ("Cursor pick: (0, 0)") was at
0,0 from the main menu's first frame - in the session before (DLL 212fa21)
the same first pick read the middle of the screen (2020, 1107) and the
cursor followed the steps. So the engine's cursor never left the corner,
whatever OBVR stepped. Nothing in the commits since (331a7d9, b7e9d48,
8716238) writes the cursor or the mouse differently: the keyboard polls
the runtime's events only while its keyboard is up (never, in that log),
the key taps only with characters queued (none). Not pinned. Logged now,
once, when the cursor stays put through 90 frames of steps: "Menu cursor:
the engine's cursor stayed at x,y through 90 frames of mouse steps - the
moves are not reaching the game (its window not in front, or its cursor
clamped)". The telling test: whether the physical mouse moves the game's
cursor in that state - if it does, the injected moves are refused; if not,
the engine's cursor is stuck.

### 4.17 The tester's round of eafd1c3 (2026-10-07, late)

- **The main menu's laser works again** ("geht wieder. liegt vll daran das
  das game nicht im focus war"); his log of the round has the new line
  "Menu cursor: the engine's cursor stayed at 2013,1171 through 90 frames
  of mouse steps" once, later in the session, so the game's window not
  being in front is the reading for both. Not OBVR's.
- **The ticks** "müssen weniger stark sein ... nur ein tick wie in
  steamvr": 5 ms at 0.08 for the hover, 12 ms at 0.2 for the click (were
  12 ms at 0.25 and 30 ms at 0.6).
- **The slap** "geht nicht so toll, beim 3ten versuch"; the log has the
  mod's sNPCGrab count 1, 2, 3 on every tap - the mod sees the tap each
  time, so what refuses is its own checks (the NPC facing the player
  within 18 degrees, the origins within 51 units). The tester: "wäre doch
  besser wenn wir seinen sound nehmen statt unseren 2 und unsere slap
  mechanik und dann irgendwie die mod triggern". So: OBVR's own slap runs
  always (the stagger, the push, the fatigue, the liking), the mod's slap
  noise is played at the slapped one by the engine on every slap
  (`PlaySoundFormAt`: the sequence of the script command PlaySound3D's
  handler at 0x00509520 - made with flags 0x102, placed by 0x006B7360,
  attached by 0x006AC3E0, started and handed over; the mod's SOUN by form
  id), and the mod is set off on top by the grab tap as before
  (`SlapTriggersMod`); when its own sequence follows, its noise comes once
  more from it. OBVR's own noise stays out with the mod loaded.
- **The reach ring's icon is back** ("die tooltip an objekten zum aufnehmen
  ist eine regression, nun schweben die in der luft und nicht mehr im
  kreis"): within reach the icon goes into the ring again, over the thing
  only beyond it.
- **The row's first frames** ("der text kommt oft mittig oder in der luft
  für paar frames und dann springt"): while the settle still showed the
  thing before and the pick's hit was already the next thing's, the row for
  the old thing was placed at the new thing's hit. Now the row is hidden
  until its own anchor is known or the pick is on it.

### 4.18 The tester's round of dccfbb5 (2026-10-07, 20:00)

The round's own log was gone by the time it was read (two sessions after
it rotated it out), so this is built on the tester's words.
- **The ticks** "5 % stärker": 0.084 and 0.21.
- **The slap** "kommt. schubst aber den npc leider auch bisschen": a slap
  in the face staggers and is not pushed any more; the push stays the
  body shove's.
- **"der text must face usw kommt und die mod triggert nicht"**: the mod's
  one message for two of its checks (the player's heading within 25
  degrees, the NPC's within 18). The heading written in the camera pass
  was taken straight back by the gaze's own heading write the same frame;
  now both are written at Present for the tap's watch (24 frames): the
  player's heading to the slapped one (`WritePlayerYaw`) and the slapped
  one's rotZ (+0x28) to the player (`HoldSlapFacing`). Logged once per
  slap, before the writes: "Shove: facing for the mod - the player n deg
  off the slapped one, they n deg off the player, n units apart (the mod
  wants 25, 18 and 51)". Not seen in the headset; the distance the mod
  wants is the one thing OBVR cannot give it.
- **The laser and items** ("das können wir nun entfernen aus dem game mit
  einem toggle (default an) ... aufnehmen mit stow (default an)"):
  `[Hands] LaserPicksItems`, off by default - the laser's classes are never
  given to an item (`g_laserPicksItems`, pick_hold_test), and the engine's
  own pick on an item no hand holds is shown and acted on as nothing
  (`LaserItemFiltered`: no name row, no icon, no crosshair depth to it;
  `TakeOnlyByHand` already keeps Activate from it). Doors, people,
  containers and the rest take the laser as before. Reach, pull, touch and
  the stow at the body are the way to items.
- **"Text für paar Frames in der Luft: immer noch hier und da"**: the
  pick's hit read for the row can be the previous thing's for a frame
  (the hit lags the pick's reference); now a hit farther than the thing's
  bound plus 10 units from its middle is no hit, and the row waits.
- **"namensfeld und keyboard geht nicht"**: no keyboard line in any log
  seen; the click log now names the tile under the cursor and the menu on
  top ("Hands: click sent to the ... menu (on top: ...) ... the tile under
  it \"...\"") - what the next log needs for the field's real name.

### 4.19 The tester's round of 65e776a (2026-10-07, night)

- **The ticks** "15% stärker": 0.097 and 0.24.
- **The slap** "musste ihn ein paar mal slappen before die andere mod
  triggerte". His log's facing lines: the player 0 degrees off the slapped
  one, they 0 degrees off the player, 69 units apart - and the mod wants
  fewer than 51 (`getDistance < 51` in its tap check), so the facing was
  right and the distance was the gate, until a slap landed from closer.
  Now the tap's watch also pulls the slapped one's origin (+0x2C, x and y)
  to 45 units from the player's feet along the line between them, when it
  is farther (`HoldSlapFacing`, `kSlapModReachUnits`); the NPC steps the
  rest of the way itself. Not seen in the headset: whether the engine lets
  an actor's origin be written mid-frame without its Havok body snapping it
  back, or the actor jolting visibly.
- **The dialogue view** "sobald er mich ansprach war die dialog kamera
  wieder zu tief auf höhe der hände. immerhin sah der npc die höhe meines
  kopfes an". His log: the menu frame's camera base 108.3 with the eyes at
  150.7 - the engine's dialogue camera lowers its own camera base for the
  talk, and OBVR's head offset went on top of the lowered base. The base is
  now held where it was as the conversation began, for as long as
  `game::PlayerDialogActive` says it lasts, in the camera pass before the
  menu frames take their base from it ("Dialogue view: the camera's base
  held at ..." once per conversation, the first eight). The NPC's gaze at
  the eyes was right already (4.11) and is unchanged. Not seen in the
  headset.
- **The hand's tooltip over items** "Laser auf Gegenstände: ok ist weg
  aber nun ist auch die hand tooltip weg auf objekten": with the laser off
  items, the hand's own reach (`ReachingForWithHand`) was still asked for a
  palm that pointed at the item; now, with the laser off items, an item
  within 42 units of the hand's surface is the hand's whether the palm
  faces it or not (`kPickNearHandUnits`, pick_hold_test), so the ring, the
  name and the reach pull are back for the hand near a thing.
- **The keyboard** "immernoch nicht da": his log's click lines name the
  tile under the cursor in the race menu as "race_name" (the character's
  name at the top of the menu, not the race's - the earlier reading of the
  XML was wrong); a click on it now opens the keyboard with "Your name"
  over it (`NameTileTakesKeyboard`, vr_keyboard_test). The alchemy,
  spellmaking and enchantment fields still rest on the XML's names and have
  no click line yet.
- **The stow circle** "sollte eher ein kreis sein den man platzieren und
  auch vergrößern oder verkleinern kann im adjust setting. by default in
  der brust": the ring is a filled circle, the placing window sizes it with
  the right stick, Done keeps the size (4.6), and the default spot is 0.33 m
  under the eyes (0.30 before; the live INI had 0.29), still over the hand
  measured against the chest on 2026-09-27.
- **"text zeilen machen noch einen luftsprung manchmal ... erst mittig
  oder woanders"**: open after this round; see 4.20 for what its log
  showed. Read through this
  round: the crosshair quad over the thing is already hidden until the
  row's anchor is the pick's (`targetSettled`), the row's anchor snaps on a
  new thing (`game::StepAnchor`), and the overlay's transform is set
  before it is shown (`HandHudLayer::Submit`). The one path left that
  shows the text "mittig" is the Info element not lifted for a frame: its
  tiles give no rectangle (hidden or unsized) while the engine still
  draws HUDInfoMenu at its vanilla place in the middle of the view, and the
  next frame it is lifted and jumps to the thing. Two logs for the next
  round, both limited: "Hand HUD: the Info element lifted / its menu
  there, no rectangle / ..." at every change of that state with the pick's
  ref, and "Hands: row on <ref>, frame 1..3 - hit ..., wanted ..., hang
  ..., m from the head" for the row's first three frames on a thing. A
  jump in the headset now has its frames in the log.

### 4.20 The tester's round of f62ce58 (2026-10-07, 21:45)

"1) ja passt 2) jetzt gehts glaube ich 3) nein 4) keyboard ist nun da!!
aber egal was ich drücke auf dem keyboard keine buchstaben kommen ins
textfeld 5) ja das war die tür wenn ich von rechts mit pointer auf die
tür gehe springt ein textfeld". Read from his log (OBVR.log.prev, 21:46).

- **The stow circle** (1): placed and kept - "Stow place: kept at 0.07
  right, -0.00 forward, -0.05 up, 0.54 m across"; the live INI has them.
- **The slap** (2): the tap's facing lines say 8 and 0 degrees, 33 units
  apart - within the mod's 51 by itself this time, the origin pull not
  needed; the mod's counter ran 1, 2, 3.
- **The dialogue view** (3), still "zu tief": his log in order -
  "Dialogue focus: eyes frozen=1 at .../148.30" (the first SetDialogCamera
  call, the live eyes at 148), then "Dialogue view: the camera's base held
  at 2328.6 4127.6 107.0" on the very next camera pass. So the engine's
  base was ALREADY at 107 by the first pass after its call, and the hold
  of f62ce58 held the lowered base. The shim's own path ran ("nothing to
  do", the original not called), so what lowers it is the caller's doing
  around that call, within one frame; not found. The hold now takes the
  base from TWO passes before the conversation was first seen
  (`g_dialogBaseHistory`), and its line prints this pass's base with the
  two before it, so the next log says whether two is enough. The other
  two conversations of the log (the tester's own) began with the base at
  141 and 168.5 - at the eyes - and were not lowered.
- **The keyboard** (4): "Keyboard: 0 character(s) typed for the RaceSex
  menu, Done" twice - the Done event came, no character event ever did.
  Valve's own sample (openvr, samples/unity_keyboard_sample/Assets/
  KeyboardSample.cs) reads the whole text with GetKeyboardText on each
  character event and on Done when not in minimal mode; the keyboard's
  ordinary mode keeps the text itself. So the text is now read back from
  the runtime on a character event and on Done, and the game is typed what
  changed in it since the last read: backspaces for what went from the
  end, then the rest (`vr::QueueTextDifference`, vr_keyboard_test).
  Character events with an empty buffer (the minimal mode) are typed as
  they come, as before. The line now says how many character events came
  and what the buffer held. Whether the race menu's name field takes the
  key taps once the keyboard is up is not shown yet.
- **The door's text** (5): his log's row lines on the door (181976D4):
  frame 1 "hit the pick's", frames 2 and 3 "hit none (the anchor's)" with
  hits 27 units from the first - the engine's own hits on the door,
  refused by the bound-plus-slack test of 4.18, so the row froze at the
  first hit and caught up in one jump when a hit passed the test again. The
  test is now only for the pick's first two frames on a thing (where the
  lagging hit of the last thing is possible), `vr::PickHitTrusted`
  (hand_hud_test); from the third frame the engine's hit on that thing is
  taken. The row's frame lines now print the bound (centre, radius) and
  whether the hit was within its slack - what the door's bound is, and why
  it refused, the next log says. The Info element stayed lifted through
  the whole log (its state changed only at the start): the "mittig" path
  of 4.19 is ruled out for that run.

### 4.21 The tester's round of 7fa4771 (2026-10-08, 19:37)

"1) buchstabe kommt. aber der nächst gewählte ersetzt dann den vorherigen
2) dialogs starteten bisher super 3) ah muss ich noch testen".

- **The dialogue view** (2): "Dialogue view: the camera's base held at
  ... 149.5 ... (this pass's ... 130.2, the one before ... 149.5, two
  before ... 149.5)" - seven conversations, the pass that first sees the
  conversation already lowered by 10-20 units, the one before it not. The
  hold from two passes before (4.20) stands; one would do.
- **The keyboard** (1): the log's lines, one per character event - "the
  buffer "y"" four times over for four presses of y, then "g", "h", "j"
  ... and "g" alone at Done after a dozen keys - so the runtime's
  GetKeyboardText answered the LATEST key and never an accumulated text
  (an accumulating one would have been "yyyy" and a dozen letters at
  Done), and every character event came with cNewInput empty. The
  difference-typing of 4.20 therefore backspaced the last key and typed
  the new one: "der nächste ersetzt den vorherigen". Now the keyboard is
  opened in its minimal mode (KeyboardFlag_Minimal, openvr.h SDK
  1.10.30: "makes the keyboard send key events immediately instead of
  accumulating a buffer") and each character event's key is typed as new
  input: the event's own cNewInput when it has one, the runtime's text
  when it is empty (`vr::KeyboardNewInput`, vr_keyboard_test); Done is no
  input. The log line prints both.

### 4.22 The tester's round of 69811f8 (2026-10-08, 19:54)

"1) ja nun geht es wie erwartet 2) ja passiert noch + neu dazu: bücher
hatten nun zwar den kreis aber keine namen mehr und kein hand symbol BUG".

- **The keyboard** (1): words type now; the minimal mode and the key per
  event (4.21) stand.
- **Books** (2, the bug): with the laser off items (4.18) a book under the
  laser was filtered like any item - no name, no reading hand, only the
  plain reticle's circle - although its Activate still opens it to read
  (4.6). A book is now never filtered (`game::LaserPickFiltered`,
  pick_hold_test): the laser gives its name and the hand as before.
- **The door's text** (2), still jumping. His log's row lines on the door
  (1AC6CE50, bound radius 52) were all "within its slack" this time - the
  4.20 freeze is gone - and what the log shows instead is the pick leaving
  the door for a thing with a bound of radius 482 at the room's middle
  (1AC6CD90; 1C2396C8 the run before), hit at floor height all over the
  room (496,-6,-89 ... 853,7,-59), which the engine's own grab once took
  ("the engine TOOK it (target now 1AC6CD90 ...)"). Its row hangs under
  the hit and trails a fast sweep (710 to 770 in three frames, the hang
  713 to 719); coming from the right onto the door the row goes from that
  thing to the door, or the other way. What it is the log does not say
  yet: the row's first-frame line now prints the thing's form type and
  name, so the next log names it. The pick's one-frame flicker to another
  thing (frame 2 of 4227, "the pick's frame 1 on it") is held off by the
  row's settle as meant.
- **The Info element** without a rectangle while the pick was on 191936CC
  (bound radius 160), three times: the game drew no info for it in those
  frames; noted, not pursued.

### 4.23 Hands "komplett verzerrt" after loading a female character (2026-10-08)

The tester: "ich hatte new game getestet -> ging. aber dann einen anderen
spielstand mit nem weiblichen char geladen -> die Hände waren komplett
verzerrt! BUG".

- **His log** (OBVR.log of 19:54): the new game's hands were drawn as
  4047 and twice 2162 triangles; after the second load (the Load menu at
  5918, closed at 6183) the hand shapes were new ones ("Hand" at 1C81765C
  hidden, 2239 and 2214 triangles sealed) while the skeleton stayed - the
  same Torch, ForearmTwist and Weapon node addresses before and after, so
  the engine kept the first-person skeleton and replaced the body shapes.
  No "a glove's skin ..." line came for the new hand shapes, though their
  skins were new objects: their addresses were the old skins' (freed and
  made again at the same place), and `StepGloveElbows` found its old
  records for them.
- **The cause**: a swap record (`Swapped`) was keyed by the skin's
  address alone, and held the old skin's bone list. For a skin "already
  known" the step wrote `bones[i] = original[i]` - the male hand skin's
  bone pointers, in the male skin's order and count - into the female
  hand skin's bone array, whose weights index bones by their own order.
  Every vertex was then skinned to the wrong bone: "komplett verzerrt".
  The Arms skins (`StepForearmStumps`, `GiveAllBack`) had the same hole.
- **The fix**: a record is used only while the skin's bone array is still
  the one it was made from - the same count, every slot the recorded bone
  or one of OBVR's own nodes (`SwapRecordHolds`, arm_stump_test). A record
  that fails is dropped without a write and logged once ("Hand bones: the
  skin ... came back with another model's bones ... the old record
  dropped, the skin taken as new"), and the skin is recorded afresh from
  its own bones. **Confirmed** (the tester, 2026-10-08, 20:15: "hände sind
  nun fixed"): his log of a new game followed by the load - "Hand bones:
  the skin 131A2B2C came back with another model's bones (36 now, 6
  recorded) - the old record dropped, the skin taken as new" - the
  6-bone skin's address taken by a 36-bone one, as read above.
- **Left as is**: `PinHandBone` keys its bone on the first-person root; a
  skeleton replaced under the same root address would keep a freed bone.
  His log shows the skeleton kept across loads, so not reached here.

### 4.24 The tester's round of ebd89f3 (2026-10-08, 20:11)

"1. ja 2. ja jetzt schneller, ich denke das problem kommt wenn man von
einem objekt zum anderen springt. der übergang könnte generell weicher
sein."

- **The hands** (1): right again. His session loaded the female
  character straight from the main menu (the bare hands' lid from the
  first frames, no new game before it), so the stale-record path of 4.23
  had nothing to drop in this log; "came back with another model's bones"
  is still to be seen after a new game followed by that load.
- **The row from thing to thing** (2): the freeze of 4.20 is gone ("jetzt
  schneller"), and what is left is the snap itself - the row, the ring and
  the quad jumped to the next thing the pick settled on. His log of this
  session shows it on a bookshelf: the pick going between "Darkest
  Darkness" and "The Locked Room" (books 23 units apart) and to
  "Denyiir" (an NPC) over and over, a snap each time. Now the three glide
  (`game::StepAnchor` with a glide: a time constant of 0.12 s from where
  the last thing was, the ordinary 0.08 s again within two units of the
  new one; a thing taken up within 0.3 s of losing the last one glides
  from where that one was left; pick_hold_test). The row's text changes
  at the settle as before, its place slides. The quad's hover is eased
  on its own from the row's wanted hang (`g_hoverAnchor`), not built from
  the row's eased point - that way its height does not jump to the new
  thing's bound while sliding sideways. The pick's aim (`g_aimAnchor`)
  never glides: a ray aimed between two things hits neither.
- **Open**: the pick itself still alternates between two things the ray
  sits between (#69); the glide softens what is shown, the settle of
  0.08 s decides how often. Not seen in the headset.

### 4.25 Two slap noises with the mod on (2026-10-08)

The tester: "beim slappen von npcs höre ich 2 slap sounds whl unserer und
er vom 'put it in place'. das fixen i guess aber nur wenn dieser mod in
der load order da ist und das feature in seiner ini an ist."

- **What played**: with the mod loaded, OBVR played the mod's own slap
  noise (its SOUN, by the engine) the moment the hand struck, and set the
  mod off by the grab tap; the mod's sequence then played the same noise
  itself with its animation (zzPiiiPAnimTimerOS: "playSound
  zzPiiiPSlapNoise" at 1.02 s of a 1.6 s clock set in
  zzPiiiPzFunctSlapperInit - about 0.6 s after the slap). Two noises, the
  same one.
- **Now**: OBVR plays nothing at the slap with the mod on and waits
  through the tap's watch (24 frames). The mod's sequence begins with
  "modAV encumbrance 2000" on the player (zzPiiiPzFunctSlapperInit); a
  step of the player's encumbrance of 1000 or more within the watch says
  it has started, and the noise is the mod's with its animation ("Shove:
  the mod's slap sequence started ..."). The watch over without it - the
  mod's own checks refused the slap - OBVR plays the mod's noise then, for
  the slap that was dealt ("... did not start within the tap's watch ... the
  mod's noise played by OBVR"). `game::ReadPlayerEncumbrance` (actor value
  11, as the dodge's fatigue cost reads it); `game::StepSlapNoiseWait`,
  shove_test.
- **The mod's switches**: its part is taken only with the mod loaded AND
  its master switch (zzPiiiPVarsQ.sEnabled) AND its slap feature
  (zzPiiiPVarsQ.sSlapper, the INI's) set - both read from its quest
  variables at the slap (`SlapModOn`). Loaded but off, the slap is OBVR's
  own throughout, with OBVR's own noise and the game's gasp, as without
  the mod; the log line says so.
- Not seen in the headset.

### 4.26 The tester's round of 915c1dd (2026-10-08, 20:40) and the next asks

"getested: slappen geht gar nicht mehr", and before it: "wenn ich gekilled
werde dann bleibt mein char ... halben meter über dem boden schweben und
droppt dann nach paar sekunden erst zu boden", "waffen ziehen soll nicht
gleich als hit zählen", "wenn ich schwinge zählen oft mehrere swings".

- **"slappen geht gar nicht mehr"**: his log of that session (OBVR.log,
  20:28-20:40, the first with 915c1dd) has no "Shove:" line at all - not
  even the "Put it in its Place ... is loaded" line the first slap writes -
  so no slap reached the shove code; the session before (c392ccb) slapped
  as ever. What the log shows through his attempts: "both fists are up -
  blocking", "the weapon's body made", and every yield line saying "a
  weapon or the fists in hand" - the weapon or the fists were readied, and
  a shove or a slap wants the weapons away (`ShoveFor`: not with the weapon
  drawn, not with a fist, not with the grip held). Nothing in 915c1dd
  touched that path. Not proven, since a refused hand left no line: now it
  does - "Shove: the right hand at n m/s towards <ref> refused - the weapon
  or the fists are readied; / the hand is a fist; / the grip is held;
  (...)", once a second, twenty lines - so the next log says why.
- **The body half a metre over the ground when killed**: `[Look]
  DeathBodyUpMetres` is 0.50 in his live INI (the template and the
  default are 0), and the held death view draws the body that much above
  where it lies (game::ShiftDeadPlayerBody, "Death: the body is drawn 35
  units ahead of the held view" in his logs - 35 units, the offset's
  length, is that half metre) until the load prompt releases the view, when
  the body is drawn where it is: the "drop". OBVR did not change his INI;
  the settings row "Death body up" (or the INI key) at 0 is the fix he
  asked for: the body falls with its ragdoll, drawn where it falls.
- **A weapon drawn counted as a swing**: his log - "ready weapon done - the
  game shows the wanted state after 0.12 s" then at once "a light swing,
  2.7 m/s at its fastest, 0.08 m long". Now the frame a melee weapon comes
  into the hand starts a grace of 0.6 s (`vr::StepDrawGrace`,
  hand_mode_test) with the swing detectors kept idle and no strike; a
  weapon in the hand from the first frame of the mode starts none.
- **Several swings for one**: his log - swings of 0.24, 0.15, 0.19, 0.27,
  0.31 m ending frames apart, each striking the same man (health 34 -> 26.6
  in five), and "a light swing ... 0.03 m long". The detector ended a swing
  on the first frame under half the speed, and the rest of the stroke was
  the next swing; the way back another. Now (`vr::StepSwing`,
  hand_mode_test): a swing ends only once the speed has stayed under half
  the threshold for 0.06 s (a dip is the strike's own jolt); after a swing
  nothing starts for 0.25 s (the way back); a swing shorter than 0.10 m is
  a twitch with no verdict, and strikes and the swish wait until a swing
  has travelled that far (`SwingLongEnough`). Not seen in the headset.

### 4.27 Crouching in the room sneaks (built 2026-10-08)

The tester: "bau ein dass wenn ich in real life crouche dass auch ingame
als crouch/sneak mode on zählt und vice versa".

- `[Hands] CrouchSneak` (on by default; settings "Crouch to sneak") and
  `CrouchDropMetres` (0.30; "Crouch depth"). The head's height in the
  tracking space (the seated universe OBVR poses in, metres) against a
  standing height: the head's highest since the last recenter, followed
  slowly downwards while standing (30 s time constant - a player who
  settles lower), never while crouched, taken anew at a recenter
  (`HandModeFrame::recenterSerial`). Under it by the drop: crouched; back
  above it by half the drop: standing - a hysteresis against a head that
  bobs at the line.
- A crouch begun or ended asks the game's sneak to follow by its key, the
  way the hold mode taps it (`StepSneakTap`: once, then again after 0.5 s
  until the game shows the wanted state), and gives up after 2 s - a sneak
  the game refuses is not asked for all day. A sneak toggled by the stick
  while standing is left alone; only a change of the crouch speaks. Not
  judged in a menu or out of the world; the standing height is kept
  across. `vr::StepCrouchSneak`, hand_mode_test; the log says "Hands:
  crouched in the room - the game's sneak asked for (sneaking now n)" and
  "stood up ...".
- Not seen in the headset. Open: a player who plays seated has the seated
  head as the standing height; leaning down 30 cm would sneak - the depth
  is theirs to raise, or the switch to turn off.

### 4.28 A container's menu over the container, the world running (built 2026-10-08)

The tester: "bau mal ein dass wenn ich einen container markiere und A
drücke das inventory menü des containers geöffnet wird (und zwar als
overlay über dem container und nicht als cinema screen) aber mit dem game
unpaused (neue setting mit default on). ähnlich wie die fallout mods".

- `[Look] ContainerInWorld` (on by default; settings "Container over the
  chest"), `ContainerPanelScale` (0.6; "Container panel size"),
  `ContainerPanelRaiseMetres` (0.35; "Container panel height").
- **The place**: as the ContainerMenu opens (one episode, the dialogue's
  shape: `g_containerMenuEpisode`), the thing activate was last pressed on
  (`g_activatedRef`, within the last 180 world frames) gives its bound; the
  menus' room anchor is set the menus' distance short of a point the raise
  (0.15 m) over the bound's CENTRE - not its top: the bound is a sphere
  round the diagonal, a chest's 0.66 m, and over that the panel "schwebt"
  (the tester, 2026-10-08) - along the level line from the head, heading at it
  (`vr::ContainerAnchor`, dialog_panel_test) - so the panel, which hangs
  that distance ahead of its anchor, lands over the chest, facing the
  head, at the container scale of the menus' width. Needs the menus in
  the room (`Render.Menus=world`, `HudAnchor=world`), as the dialogue's
  panel does; a container menu opened with nothing activated lately (a
  script, a pickpocket) keeps its usual place, said once in the log.
- **The world running**: the IsMenuMode sites of the update step
  (game/MenuPause.cpp) answer "no pause" for the ContainerMenu on this
  switch alone, whatever `Render.UnpausedMenus` says
  (`WorldPausesForMenu(..., containerRuns)`, menu_pause_policy_test); the
  sites are redirected when either is on.
- The laser works the panel wherever it hangs, as on every menu. Not seen
  in the headset. Open: a body looted from close by puts the panel low
  over the corpse; the raise is the knob.
- **The crash of 2026-10-10** (fixed the same day): the tester dropped a
  weapon in the inventory and the game crashed in OBVR
  (CobbCrashLogger: EIP OBVR+0x71BAF, `game::RefWorldBound`, `movq
  xmm0,[edx+20h]` with edx 0x3F800000). The bound was read every frame for
  the 180 frames after an activation, whatever menu was up - and the thing
  activated was a weapon picked up: deleted as it went into the
  inventory, its memory handed to the next allocation (the drop's), so the
  node pointer read through the old address was a float, 1.0. Now the
  bound is read only while the panel waits to be placed
  (`DialogRecentreDue` wants nothing else), and only while the address
  still holds what was activated - the same vtable and form id, neither
  deleted nor disabled (`game::RefFingerprint`, ref_fingerprint_test). The
  same check guards the other references remembered across frames: the
  reach's target and the next one of a switch (4.31, 4.35: one gone is
  "gone" and never activated), and a thrown thing in flight (picked up
  again: no longer followed).
- **The crash of 2026-10-12** (fixed the same evening; the tester took the
  shield from the floor once more that night on the fixed build - "kein
  crash"): the tester pressed A on a Steel Shield lying in the world - TakeOnlyByHand
  off, so the game's own Activate took it - and the game crashed in OBVR
  (CobbCrashLogger: EIP OBVR+0x808AB, `game::RefBaseFormType`, from
  `UpdateHandMode` at Present; esi the shield's reference 1C8A325C, eax
  0x3F7E0010 - a float where its base form pointer had been). The same
  class as 2026-10-10, two holders further on: the pick's hold
  (game/PickHold.h, 2026-10-06) kept the held item through its 0.15 s grace
  once no hand reached for it - and the cell's walk no longer met the
  deleted shield, which to the hold looked like "not reached for" - and the
  ring's settle (`StepRefSettle`) kept the thing it showed 0.08 s more;
  the engine deleted the reference the frame it took the shield and its
  memory went to the next allocation. Three things changed. `FindNearestItem`
  says whether the held item was met in the cell at all (`keepAlive`,
  `KeepAliveVerdict`: gone only after a complete walk did not meet it) and
  the hold lets a gone one go at once ("Pick: ... is gone from the cell -
  let go at once"). The settles for the ring and the info row carry the
  fingerprint of what they show and drop it the frame it fails
  (`SettledRef`; a gone thing is nothing that frame even where the engine's
  slot or the pick still name it). And the engine's crosshair slot is taken
  for a reference only while what is there is one (`RefLooksLive`: a game
  vtable, neither deleted nor disabled) - whether the engine clears that
  slot itself the frame it deletes a thing is not known. Every item the pick
  finds now carries its fingerprint (`NearItem::print`), asked before the
  aim's ray and the harness's items line read through it. pick_hold_test
  and ref_fingerprint_test cover the new flows; `activate-takes.txt` is the
  crash's own path (the hand on a dropped sword, A pressed; PASS 2026-09-27,
  before the hold existed) and expects the "gone" line now - to be rerun.

### 4.29 The tester's round of 0bbd454 (2026-10-08, 21:18) - read 2026-10-09

"1) joa geht nun. slap sounds kommen manchmal nicht oder abgehackt ...
aber es ist okay 2) jap das geht. nur kam dann das Esc menü nicht musste
es mit B herholen 3) ja geht. etwas den wert verringern though 4) geht wie
erwartet. die 30cm sollten in den settings einstellbar sein 5) tafel
schwebt über der kiste ... das das menü wirklich auf dem objekt liegt
aber ausgerichtet zu meinem sichtfeld. außerdem muss noch der orange
shader weg."

- **The slap** (1): nine slaps in his log, all through the shove code
  ("staggered ... in the face ... the mod set off by a grab tap - its
  noise waited for"); five "the mod's slap sequence started (the player's
  encumbrance 39 -> 2039)" - the mod's noise - and four "did not start
  within the tap's watch (39 -> 39) - the mod's noise played by OBVR":
  the mod refused those (its own checks; a dialogue opened right after
  most slaps, the slapped one addressing him), and OBVR's noise came
  0.27 s late. "manchmal nicht oder abgehackt" fits a noise cut by the
  dialogue menu opening on it; left as is, as he said.
- **The Esc menu after death** (2): his log has "the death view is held
  still", then the Pause menu 729 scene calls later; the session before
  (c392ccb) 720 - the same pattern, and neither log can tell a menu he
  opened from one the game opened. New line: "Hands: the menu key n s
  after death - the game had shown no menu of its own by then / had a
  menu up already", so the next death says whether the game's own load
  prompt was late or never came. Oblivion.ini holds no death reload time
  (only bForceReloadOnEssentialCharacterDeath, bTrackAllDeaths).
- **The swing's values** (3): the rest after a swing 0.25 -> 0.18 s, the
  twitch length 0.10 -> 0.08 m, the draw's grace 0.6 -> 0.45 s.
- **The crouch depth** (4): it is in the settings already - Hands,
  "Crouch depth" (0.1-0.6 m), `[Hands] CrouchDropMetres`.
- **The container's panel** (5): it hung over the bound's top, and a
  chest's bound is a sphere round its diagonal (radius 46 units: 0.66 m
  over the middle, 0.35 m raise on top): "schwebt über der kiste". Now
  over the bound's centre plus the raise (0.15 m by default, -0.5 to 1 in
  the settings), facing the head as before - on the thing, as the ring
  and the hand's row sit on it. And no menu shade behind it: the world
  runs there and is meant to be seen ("der orange shader muss weg"). The
  switch he asked for is the "Container over the chest" row (Screen),
  `[Look] ContainerInWorld`.
- **Weapon weight**: a proposal, docs/physical-combat-spec.md section 6:
  the drawn hand and weapon pulled after the controller by a spring whose
  time constant grows with the weapon's weight, capped, the strike fed the
  drawn blade, one slider `[Hands] WeaponWeight` 1-100 %. Built the same
  day, 4.30.

### 4.30 The weapon's weight: the drawn hand trails the controller (built 2026-10-09)

The tester: "und setze jetzt um deinen vorschlag. Mir ist nur wichtig das
der ingame weight wert der waffe in die gewichtung mit einfliesst".

- The item's own weight, read from the WEAP form (`game::WeaponWeightOf`,
  `addr::kWeaponWeightOffset` 0x7C), sets the lag and nothing else does:
  the time constant is `0.12 s * clamp((weight - 3) / 40, 0, 1) *
  slider`, so an iron dagger (3) sits on the hand and a warhammer (42)
  trails a fast swing by a hand's breadth; both hands on the handle cut
  it to 0.4. A cap of 0.25 m and 35 degrees (times the slider) bounds the
  gap. `vr/WeaponWeight.h`, pure; the design, the decisions and what is
  open are in physical-combat-spec.md section 6, "Built".
- The lag is of the controller's pose in the room (tracking space), then
  put relative to the head: a turned head does not swing the weapon.
- Who takes the drawn pose: the weapon hand's pin, the two-hand grip,
  the strike by motion (the hit lands where the weapon is seen), the
  hands' Havok bodies and the push. The laser, the ring, the reach, the
  grab and the swing detector keep the controller's. Bows, staffs and
  the fists do not lag; nor anything while the hands are adjusted.
- `[Hands] WeaponWeight=40` (1-100 %; settings "Weapon weight (%)"), hot
  reloaded. 1 % is the controller.
- `[Hands] WeaponSwingThrough=1` (settings "Weapon swing-through"; the
  tester: "ja machen wir das nachschwingen auch aber hinter einem feature
  toggle (default on)"): the weapon trails a moving hand as the plain lag
  does - the weight - and has momentum besides: a hand that starts leaves
  it further behind for a moment, a hand that stops is overrun by it, by
  about half the trail, and it swings back (a spring, a drag against the
  room and a mass; the damped oscillator's closed form, damping ratio
  0.4, omega 2 x 0.4 / tc). Off, the plain lag that only trails. The
  first version damped relative to the hand and lost the weight; see
  physical-combat-spec.md section 6.
- The log: "Weapon weight: <name> <weight>, time constant n ms at n %,
  cap n m / n degrees" once per draw - the first headset run checks the
  figure against the inventory row - and the largest gap of each of the
  first six swings with a lag.
- Tests: `weapon_weight_test` and `TestWeaponWeightInMode` in
  `hand_mode_test`; 123 pass. Not seen in the headset.

### 4.31 Opening by reaching: chests, bodies, locks and pockets (built 2026-10-09)

The tester: "fang an. alles hinter einem feature flag. default an. Eine
offene, leere Hand nah am Container, aber auch nicht kurz gehalten, ich
muss mich nur mit der hand nähern. Verschlossene Container auch hier wenn
ich mich näher kommt dann das Schlossknacken minigame wie das neue menü.
wenn das bestanden ist das neue menü. stimmt dann bei taschendiebstahl wir
schauen mal." Feasibility in docs/container-touch-spec.md.

- `[Hands] ReachOpens=1` (settings "Open by reaching"), `ReachOpenMetres`
  (0.10; "Reach to open (m)"), `ReachCloseMetres` (0.45; "Away to close
  (m)", kept beyond the open one by `vr::ReachCloseMetresFor`).
- **Opening**: a free hand (`vr::HandFreeToOpen`: tracked, grip open, not
  a fist, holding nothing, no weapon or fists drawn) within the open
  distance of a container (base form 0x17), a body (a dead Character or
  Creature) or - with the player sneaking - a living one not in combat
  (`game::FindReachTarget`, the player's cell's references) activates it
  at once, no dwell: `TESObjectREFR::Activate(player, 0, 0, 1)` as the A
  button ends in (0x004DD260, `game::ActivateByPlayer`). Not while the
  stick walks the player (passing by opens nothing), not with a menu up,
  and not when an item the same hand is at is nearer than the container
  (`vr::ItemGoesFirst`: the bottle on the desk is taken, the desk stays
  shut).
- **The distance**: a container's, to its model's box in its own frame
  (`vr::DistanceToBox`; the vertices read the way NearbyItems reads them,
  checked against their bound) - a chest's bound sphere reaches 0.66 m
  from its middle; an actor's, to its "Bip01" bones less 8 units of flesh
  (`vr::DistanceToBones`); the bound at 0.6 of its radius when neither
  reads (said in the log).
- **What comes of it** (`vr::StepReachOpen`, phases Idle, Opening, Open,
  Lockpicking, Rearm): the ContainerMenu - its panel over the thing as for
  A (`g_activatedRef`), the world running; the LockPickMenu for a locked
  chest - over the chest too, the world running (`WorldPausesForMenu`'s
  `lockRuns`, `ContainerPanelUp`), no shade; when the lock gives with the
  hand still there, the chest is activated again and its menu comes, or
  the game's own opening is taken as open; any other menu (a
  conversation, a message, an arrest) is left to the game; nothing for 30
  frames (a key needed, no picks) is left too.
- **Closing**: the hand (any tracked one, free or not) beyond the close
  distance closes the container's menu or the lock's minigame the reach
  opened - the engine's close-all-menus (0x00579770, verified by its first
  twelve bytes; `game::CloseMenus`) - never under the quantity popup,
  never a menu the reach did not open. Then the reach waits for the hand
  to leave before the same thing opens again (closed with B while the
  hand is still there: not reopened).
- **The panel faces the eyes** (the tester, 2026-10-09: "achte noch darauf
  dass die overlays so geneigt sind dass sie direkt zum headset schauen
  also nicht in der luft schweben senkrecht"): `vr::ContainerAnchor`
  turns the container's and the lock's panel along the full line from the
  head to the thing - tilted back over a chest below the eyes, down under
  a shelf above them, its edge kept level (a pitch, never a roll) - and
  `HudLayer::AnchorAt(pose, keepTilt)` keeps that tilt (every other anchor
  is levelled as before). The tilted anchor is dropped as the container's
  menu goes, so the next menu is placed level where the head looks. The
  dialogue panel and the HUD already hang at eye height, facing the eyes.
  The log's panel line gives the tilt (88 degrees in the hand script: the
  chest lay under the player).
- **Episodes by the top of the stack**: the container's and the lock's
  menu episodes now also look at `TopVisibleMenu`, not only at
  ActiveMenuId (the menu under the cursor): a menu opened by reaching
  never had the cursor on it, and its panel was not placed (hand script
  reach-open, first runs).
- Log: "Reach: <ref> reached - activated / its menu opened / the hand left
  - closed / the hand is away - armed again ..." with the hand's and the
  target's distances and the top menu; at a hand script's mark a state
  line (free hands, weapon, grips, fists, moving, the nearest thing within
  2 m and its bound).
- Tests: `reach_open_test` (every phase and its exits, the distances, the
  hand and kind rules), `menu_pause_policy_test` (the lock's switch).
  Hand script `reach-open.txt`: PASS 2026-10-09 (an empty chest placed by
  the console - it landed under the player, not 60 units ahead - the open
  hand brought down onto it: activated, its menu up with the world
  running, the panel over it, the hand away: closed, armed again).
- **Not exercised yet**: the lock (`game::RefIsLocked` reads ExtraLock as
  xOBSE lays it out, vtable 0x00A357B8 checked - not read in a run), the
  minigame over the chest with the world running, the reopen after a
  picked lock, a body, a pocket. Headset test open.

### 4.32 The weapon stops at walls (built 2026-10-09)

The tester: "waffen kollisionen mit allem ... fang an", with the decisions
of docs/weapon-collision-spec.md section 8 (2026-10-09): past the cap the
weapon lets go and passes through ("Variante A"), the blade rests on a
person held there slowly, a parry stops the whole blow, a short hit-stop
on a landed hit. This is phases 1 to 3 of that spec - walls, the clutter a
fast swing passes, and the measuring; bodies, parries and shields follow.

- `[Hands] WeaponStopsAtWalls=1` (settings "Weapon stops at walls"),
  `WeaponLetGoMetres` (0.60, 0.30 until 2026-10-10 - the tester: "das waffen stoppen bei 0,3 ist generell zu früh"; "Weapon lets go at (m)"), `WeaponLetGoDegrees`
  (45, INI only); `game::BladeContactSettingsFor` holds them to 0.05-2 m
  and 5-180 degrees.
- **The step** (`game::StepBladeContact`, game/BladeContactLogic.h): the
  drawn weapon hand HandMode answers - behind the controller by the
  weapon's weight - is swept from where it was drawn the frame before:
  five points of the blade (guard, quarters, tip), each a ray of the
  world's pick reaching 1.5 units past its end. The first thing on a ray
  that is fixed (a fixed or keyframed body: layers STATIC, TERRAIN, a door)
  holds the blade 1.5 units short of it; what is left of the move along
  the surface is swept once more (a blade pressed to a wall scrapes along
  it); a ray goes on past clutter and the dead (dynamic bodies) and past
  water, triggers, controllers and the pick layers. A pose across
  something along its length (a post between two points) is not taken -
  the stop, then where it was, then letting go.
- **Let go**: held more than the cap from where the spring would draw it,
  or turned more than the angle: the weapon goes back to the hand and
  passes through. **Through, it strikes nothing** (the strike by motion is
  skipped) until the blade is free again: nothing across it along its
  length, nothing between the eyes and its guard. A blade taken up inside
  something (the first frame, a jump of the camera over 40 units or 15
  degrees: a teleport, a snap turn) passes through the same way.
- **Written back** into `g_hand.weaponHandRotation` and
  `weaponHandOffsetUnits` before the strike, the push and the Havok body
  read them, so the bones are pinned where the blade stopped; the two-hand
  grip follows it. The blade's span is the drawn weapon node's
  (`BladeSpanFromNode`) read against the pose the bones were pinned to,
  kept until it has stood apart for 90 frames (`StepBladeSpanCache`), anew
  for another weapon.
- **Clutter**: what a sweep's ray passed that moves is given the passing
  point's speed (`KickBodyByBlade`, WorldPush's push) - the cure for the
  open bug "the sword's tip sometimes passes through" thin things between
  two physics steps.
- **Felt and heard**: a pulse on the weapon hand at each touch by the
  speed into the surface (the lightest up to 0.45 m/s, full from 3 m/s), a light one
  every eighth frame while it scrapes; from 1.5 m/s the weapon's knock
  (Oblivion.esm's WPNHitBladeX 0000C3C4, WPNHitBluntX 0000C3C7), at most
  four a second.
- Log: "Contact: the blade met body <b> (layer, motion) at ... - its point
  0..1 of the way to the tip, m/s into it", "Contact: let go - ...",
  "Contact: the blade free again", "Contact: <n> frames with a blade drawn
  - rays a frame, ms a frame"; at a hand script's mark a state line.
- Tests: `blade_contact_test` (every kind, the ray past clutter, turns,
  the sweep and its margin, every flow of the step, the jump, the span
  cache, the settings, the feel). Hand script `blade-wall.txt`: PASS
  2026-10-09 - a longsword drawn by the holster, pointed down and lowered:
  it met the floor (layer 1 STATIC, motion 7 fixed) at -256 with the feet
  at -258.5, held its tip at -254 while the hand's went to -261 and -272,
  let go at 24.2 units, through, and was free again raised; 2.0 rays and
  0.004 ms a frame.
- **Not exercised yet**: a wall (only the floor was met), the slide along
  it, a fast swing's kick of a cup, the knock's sound and the pulse (the
  harness has no controllers to feel), a two-hander. Headset test open.

**The living (built 2026-10-09, the same day; spec section 4 C and decisions 2 and 4):**

- `[Hands] WeaponStopsAtBodies=1` ("Weapon rests on people") and
  `WeaponHitStop=1` ("Weapon hit-stop").
- **Capsules on their bones** (`game::CollectBladeBodies`, game/BladeBodies.h):
  the high-process actors near the blade (the strike's own walk), alive,
  not the player; 18 "Bip01" bones found by name through the root's
  GetObject once per actor and root, kept and checked by name each frame;
  13 capsules (`BodyCapsulesFromBones`: a head reaching 9 units past its
  bone, neck, three torso links, arms, legs; radii 3.5-10 units times the
  actor's scale - proposed for a human, not measured on the meshes); with
  fewer than six bones the bound's column.
- **Slow, the blade rests on them** like on a wall (`SweepBlade` with
  `BladeLiving`), sliding along; pressed on past the cap it goes into them
  - let go, but not "through": they are passed and the blade still strikes.
- **A swing goes in and passes** (PLANCK's rule): a swing into someone, or
  a blade that finds itself in them (they walked into it), passes them
  until it has been out of them for 0.22 s (`BladePassLedger`). The hit is
  still the strike by motion's, unchanged.
- **The hit-stop** (`HitStopShare`): from the frame a swing goes into
  someone, the blade goes a quarter of its way for three frames, then a
  half, three quarters, all - the weapon only, never the view. A strong
  pulse on the weapon hand as it goes in, a light one when it comes to rest
  on someone (no knock sound on a body).
- Log: "Contact: <actor>'s skeleton read - n of 18 bones", "Contact: the
  blade rests on <actor> ...", "Contact: pressed on into <actor> ...",
  "Contact: a swing went into someone ..."; at a mark "Contact: people - n
  near (capsules), passed, resting on, swinging, hit-stop frame" and the
  first three capsules.
- Tests: `blade_contact_test` (segment into capsule, segment distances, the
  capsules from bones, the column, the ledger, every flow with people: rest,
  press in, swing in, pass while in, free again, walked into, a wall behind
  someone, taken up inside; the hit-stop).
- **A limb between two points** (found in the hand script): held slowly,
  the blade crossed a thin arm between two swept points, and the next frame
  found it in them and passed them - it went straight through. A pose with
  the blade across someone not passed is now refused like a post
  (`PersonAcross`), and they hold it.
- **The rest found by sliding**: the blade held by the floor and slid along
  it into someone lying there now rests on them (the slide's contact is
  the one that holds it); "rests on" is said whenever the person it rests
  on changes, not only on a touch from the air.
- **Harness**: `blade-body.txt` PASS 2026-10-09 - a beggar placed and
  frozen (`tai`; PlaceAtMe put them behind the player, lying, 80 to 120
  units off), the blade turned slowly round the player at chest height and
  then level just above the floor: "rests on" the beggar, "pressed on into"
  them at 21.2 units, and in the fast circle "a swing went into someone"
  in the frame the strike by motion met them, the hit-stop running; at most
  4.4 rays and 0.008 ms a frame. `blade-wall.txt` PASS again on the same
  build.

**The living hold a swing and a punch (built 2026-10-15; spec section 17,
the tester: "mach es wie in blade and sorcery"):**

- `[Hands] SwingsStopAtBodies=1` ("Swings stop at people") and
  `PunchesStopAtBodies=1` ("Punches stop at people"): the living hold the
  weapon and the free hand at any speed, as the stoppers of section 15 do
  - a swing is held at the person it meets instead of going into them, a
  punch, a slap or any hand at the shove's speed is held too - for good by
  default (`[Hands] BodyLetGoMetres=0`, "Lets go at people (m)": Blade &
  Sorcery's "block" - the hand comes back, the shape comes free), or into
  them once the hand is `BodyLetGoMetres` past where they hold it (passed,
  the blade still striking) when that is set; walls and the stoppers keep
  `WeaponLetGoMetres`. The hit stays the strike by motion's and the
  shove's own; both read the controller, not the held pose, so the blow
  lands as before. Off, the old way: a swing or a fast hand goes through,
  the hit-stop slows the swing (`BladeLiving::hold`,
  `BladeContactFrame::bodiesHold`; blade_contact_test "the living hold a
  swing"). In B&S the body's colliders stop the weapon's and the hand's
  rigid bodies while spring-damper drives with a force cap pull them after
  the controller; the collision is the blow (spec section 17.1).
- Felt by the speed into them (a slow rest stays the light pulse); log
  "Contact: a swing is held by <actor> at N m/s ...", "Contact: a fast
  <hand> is held by <actor> ...".
- `[Debug] ContactTrace=N`: N lines "Contact: trace - ..." per step while
  people are near and the shape swings, is held or goes in (the tip from
  and to, drawn where, held by, entered, event, gap, through, passed).
- `[Hands] EnemyKnockback=0` ("Hits push enemies (%)") and
  `EnemyKnockbackPowerOnly=0` ("Only power attacks push"): the game's own
  knockback on the player's hits - every landed blow shoves the one hit
  through their Havok proxy, 19 units from a light sword hit - kept at this
  share (0 none, as a B&S body stays; 100 the game's own), or only for power
  attacks. game/HitKnockbackLogic.h, PlayerStagger.cpp (the proxy call
  through a stub with the attacker and the power flag, the push call scaled);
  spec section 17.3. Hits on the player stay `[Look] NoPlayerStagger`'s.
- Harness `blade-held.txt` (the defaults: held for good, the knockback
  off), `blade-held-cap.txt` (0.30 m, the game's knockback) and
  `fist-held.txt` PASS 2026-10-15 afternoon (docs/hand-script-harness.md).

**The hit by touch, the shallow band, the fist by the grip (built 2026-10-15;
spec section 19):**

- `[Hands] MotionHitsByTouch=1` ("Hits need the touch"): the strike by
  motion lands only where the drawn blade's own span (a fist: 12 units of
  itself) comes within 4 units (a fist 6.5) of one of the actor's capsules
  the contact steps collected this frame; an actor with no capsule in the
  list is not hit; no list, or off: the bound's ball (`HitBoundFactor`,
  `HitPadUnits`) as before. Log "Hands: the blade touched ...".
- The shallow band (`kBladeShallowUnits` 3, at most half a capsule's
  radius): a shape only just inside a person - their idle moved the capsule
  over it - is held at them, not passed. Log "Contact: <actor> walked into
  the blade where it was ..." whenever a person is passed by lying in.
- `[Hands] FistGripForce=0.6` ("Fist by grip (force)"): a grip squeezed
  this hard is a fist as well, for a controller whose fingers do not read;
  released 0.15 below. Log "Hands: the <side> hand's fist by the grip's
  force ...".
- The grab: a hand that outruns the held body at the shove's speed throws
  it instead of losing it; the pull four times stronger; the take's reach
  14 units (spec, physical-combat-spec 8).
- Harness left-fist, blade-held, fist-held, grapple-ragdoll PASS
  2026-10-15 afternoon.

**The parry (built 2026-10-09; spec section 4 E, decision 3):**

- `[Hands] WeaponParries=1` ("Weapon parries"), `ParryStopsAll=1`,
  `ParryFatigue=10` (INI only).
- **Their blades** (`game::CollectBladeBodies` with foes): each living
  actor near with a weapon in hand - their "Weapon" node, found once by
  name and kept with the bones, along its own y axis to its bound's far end
  (`TheirBladeFromNode`) - with the frame before's and their process action
  (HighProcess +0x1F4, only in high process).
- **Meeting**: while they attack (action 2 or 3), their blade and the
  player's, both moving, looked at in four steps of the frame, within 4
  units (`BladesMeet`); or their blade within the worn shield's ball (the
  bound of "Bip01 L ForearmTwist" times 0.8, only a shield's size,
  `ShieldBall`, `BladeMeetsBall`). They are then parried for 0.8 s
  (`ParryLedger`): a strong pulse on the hand that met them, the block
  sound (WPNBlockBladeX, WPNBlockBluntX, WPNBlockShieldLightX).
- **Their blow** in that window is blocked by the engine's own path: the
  hit handler's block check (0x005FF7DF) answers yes for the player when
  someone is parried, the cone (0x005FF83E, game/BlockCone) yes for the
  parried attacker and no for any other under that forced block, the share
  (0x005FF8C7) 1 - all of it - with ParryStopsAll. The engine then plays
  the block, gives Block its experience and makes the attacker recoil. The
  fatigue is spent in the next frame, outside the hit handler.
- Log: "Parry: the hit handler's block check (005FF7DF) and blocked share
  (005FF8C7) rerouted" at start-up; "Parry: the blades met - <actor>'s
  attack ..." / "their blade met the shield"; "Parry: <actor>'s blow
  parried - blocked 1.00 of it (the engine's own share ...)"; and for the
  first two dozen blows at the player, "Parry: a blow at the player - the
  engine says blocking ..., blades near: <actor> action <n> tip <d> from
  the eyes, <d> from ours" - the timing the parry depends on.
- Tests: `parry_test` (attack actions, their blade, two moving blades, the
  shield's ball, the ledger, every answer of the hit handler).
- **Hand script** `blade-parry.txt` (a bandit, 00069AD1, attacking a
  player made untouchable with tgm while the blade circles at the chest):
  PASS 2026-10-09 as a measurement: the hooks ran on 24 blows, no parry -
  at every blow the bandit's blade tip was 131-191 units from the eyes, its
  torso 140-165 away (fCombatDistance 128); see weapon-collision-spec 11.
  With CombatReach 85 (below): PASS with parries, "blocked 1.00 of it".
- **Combat reach** (`[Hands] CombatReach=85`, settings "Combat reach
  (units)", 0 the game's own; game/CombatReach.h): the game setting
  fCombatDistance (0x00B36F20, checked by its name) held while Full VR runs
  and given back after - fighters strike from about 1.3 m instead of 2 m,
  their blades within the player's reach (measured: the bandit's tip 24-55
  units from the eyes at each blow, from 131-191). Not kept in the
  savegame, not an INI setting. The player's strike reaches 85 units too.
- **Found in the tester's run of 2026-10-09:** a power swing struck an NPC
  by the strike by motion (the blade 52 units from its bound's centre,
  within the 71 the sphere test allows) with no "a swing went into
  someone": the blade never entered the capsules, so no hit-stop. The
  strike's sphere is far wider than the body; taking the hit from the
  capsules is the open step that would make the two agree.

**The hands and the shield (built 2026-10-11; spec section 14):**

The tester: "Gib nun einhand Waffe und Schild oder freier Hand Kollisionen.
Same für alle anderen Waffen mit der freien Hand."

- `[Hands] HandsStopAtWalls=1` ("Hands stop at walls") and
  `HandsStopAtBodies=1` ("Hands rest on people"); the weapon's
  `WeaponLetGoMetres` and `WeaponLetGoDegrees` are the hands' caps too.
- **Which hand** (`game::FreeHandShape`, game/HandContactLogic.h): every
  hand the weapon is not drawn in - the off hand with a one-hander, a
  two-hander (when it is off the handle), a staff, and both hands with
  nothing drawn or the fists up. The off hand with a shield on its arm is
  met as the shield. Left to what they hold: the weapon hand with a swung
  weapon drawn (the blade's own step), both hands with a bow drawn, a hand
  on a two-hander's handle or about to take it, a hand round a held object,
  one leading someone, one holding a fighter. A staff or a torch in the
  hand passes through: only the hand is swept.
- **The step** (`StepHandContacts`, CameraHook.cpp, right after the
  weapon's): the weapon's own `StepBladeContact`, run per hand with the
  hand's span from its drawn bones - the wrist to the fingertips, the one
  its Havok body has (`HandSpanFromBones` on "Bip01 L/R Hand" and "Finger2",
  read against the pose the bones were pinned to the frame before) - held
  2.5 units (its capsule's radius) short of a surface. Walls, the floor and
  furniture hold the hand; the slide, the let-go past the cap, the pass
  through until free, the jump of the camera: all as the weapon's. The
  living (`CollectBladeBodies` round the eyes, 130 units, once a frame for
  both hands): held slowly the hand rests on them; at the shove's speed
  (`ShoveSpeed`, 1.3 m/s - the speed that shoves or slaps them anyway) or in
  a fist's swing it goes through and they are passed (`HandSwinging`).
- **Written back** into the hand's pose - `leftHandRotation` and
  `leftHandOffsetUnits`, or for the weapon hand `weaponHand*` and
  `rightHand*` (the same thing with nothing drawn) - before the strike, the
  push, the Havok body, the shove, the grab and the pin read it, so the hand
  is seen where it stopped. The pick, the laser and the reach run earlier in
  the frame on the controller's own pose, as they do for the weapon. A fist
  that went through a wall strikes nothing until it is free again.
- **The shield** (`ShieldDisc`, game/ShieldShape.cpp): read from the shield
  model's vertices under "Bip01 L ForearmTwist" into the hand's frame - the
  principal axes of their scatter (Jacobi on the 3x3 covariance): the two
  directions of most spread are the disc's spokes, the least its normal; a
  second pass measures the reach along each. The step sweeps the two spokes
  end to end (`BladeContactFrame::span2`) and holds the disc half its
  thickness and a little short of a surface (1.5 to 4 units: a boss would
  hold the rim far off). Only a shield's size and flatness is taken (spokes
  6-60 units, the thickness under 0.6 of the short spoke); a model that
  cannot be read leaves the bound's ball (`ShieldBall`) to stand in, swept
  as one point with the radius as its margin - too early face-on, said in
  the log. The disc is read again when the node or its bound change, or
  every 90 frames. Measured on the Arena Iron Shield: 695 vertices, spokes
  20.9 units, 13.1 thick (the boss and the handle), the normal 0.95 -0.11
  0.29 in the hand's frame.
- **Felt and heard**: a pulse on that hand at each touch by its speed, a
  light one while it scrapes; the shield's knock from 1.5 m/s
  (WPNBlockShieldLightX), a hand's touch no sound.
- Log: "Contact: the left hand meets the world now (the hand's span from
  its bones)" / "the shield meets the world now (the shield's disc)";
  "Contact: the shield's shape - n vertices, its centre ..., spokes ...,
  thick; its normal ..."; "Contact: the <left hand|right hand|shield> met
  body <b> (layer, motion) at ... - its point 0..1 of the way along, m/s
  into it; held n units and d degrees from the controller"; "... let go -
  ..."; "... free again"; "... rests on <actor>"; "Contact: a fast <hand>
  went into someone"; "Contact: n frames with a hand or the shield met -
  rays a frame, ms a frame"; at a hand script's mark a state line per hand
  and "Contact: hands - n near (capsules), rays this frame".
- Tests: `hand_contact_test` (which hand, the swinging speed, Jacobi, the
  disc from a synthetic shield's vertices and what is refused, the spans and
  the margin, the ball, when it is read again, a hand and a disc lowered
  onto a floor flat and upright, the ball swept once);
  `blade_contact_test` (the sweep's margin, a one-point span, a shape of
  two spans at a wall ahead and to the side, across a post, taken up across
  it, the ledger with two segments, the second span on someone and through
  them).
- **Harness**: `hand-wall.txt` PASS 2026-10-11 - nothing drawn, the left
  hand pointed down and lowered: "the left hand met body 188401C0 (layer 1,
  motion 7)" at -256 with the fingertips held at -253 while the controller
  went to -288, "let go - 31.8 units (at most 21.0)", "free again" raised;
  then the right hand the same (met, let go at 33.2, free again); 2.0-2.9
  rays and 0.011 ms a frame. `shield-wall.txt` PASS 2026-10-11 - the Arena
  Iron Shield (000733ED) on the arm and a longsword drawn by the holster:
  the blade's step took the right hand ("state - active 1, the blade 67.2
  units"), the left met the floor as the shield's disc ("the shield met
  body 181CA4F0 (layer 1, motion 7) at -256 - its point 0.00 of the way
  along", the rim, held 1.6 units from the controller), was pressed on
  (gap 12.2), "let go - 22.7 units (at most 21.0)", "free again";
  2.0-3.7 rays and 0.005-0.012 ms a frame. The "13 m/s into it" of both
  runs is the harness's step (15 cm in one frame), not a swing.
- **Not exercised yet**: a wall (only the floor), a two-hander's free hand
  and a staff, the torch hand, the fists' through-gate, the grab with the
  hand resting on someone (the reach stays 6 units, the hand rests 2.5
  short). Headset test open: whether a hand on a table or a wall feels
  right, whether the shield's rim stops where it is seen, and whether 1.3
  m/s is the right speed to go through people.
- **The tester's round of 287bcd1 (2026-10-11)**: "hands in menüs sind da";
  the hand and the shield did not meet the own weapon - see the stoppers
  below.

**The stoppers: the blade at the shield, the hand at the blade, the foes'
blades and shields (built 2026-10-11; spec section 14):**

The tester: "schild kollidieren nicht mit schwert. hand nicht mit waffe."

- **What** (`BladeLiving::stoppers` and `::discs`, game/BladeContactLogic.h):
  shapes of OBVR's own the blade and the hands are held at whatever their
  speed - a swing stops at them as at a wall. The own shield on the left
  arm as its disc (`ShieldDiscInWorld`: the read disc carried by the left
  hand's pose, round at the longer spoke, half its thickness and a little;
  the bound's ball until the model is read); the drawn blade as a capsule
  2.5 units round for the hand that does not hold it (`AddOwnBladeStopper`);
  the foes' blades (BladeFoes, 2.5 round) and their shields (their
  "Bip01 L ForearmTwist" node's model read as the player's is,
  `BladeBodies.cpp`; `AddFoeStoppers`, ParryLogic.h). The blade's step takes
  the shield and the foes'; each hand's step the blade (the off hand), the
  shield (the weapon hand with nothing drawn) and the foes'.
- **How they hold**: a disc is a new shape of the sweep (`SegmentEntersDisc`:
  the slab of its thickness met with the cylinder of its radius, the face
  or the rim entered; `SegmentTouchesDisc` samples nine points of the
  segment). Pressed past the cap (`WeaponLetGoMetres`) a stopper is passed
  as a person is - through the ledger, by its id (`kStopperOwnShield`,
  `kStopperOwnBlade`, a foe's address with bit 1 or 2 set) - and the blade
  goes on striking; it is held again once out of it for 0.22 s. A pose
  across a stopper (the shield arm swung across the blade) is not taken,
  and a blade taken up inside one is passed, not "through". A swing into a
  foe's blade is held by it; their body behind it is not reached (the sweep
  meets the stoppers and the discs before the living, and a person past
  the point where it is held is left alone).
- **Felt and heard**: the pulse by the speed into it; from 1.5 m/s the
  shield's block sound on a shield (WPNBlockShieldLightX), the blade's or
  the blunt's on a blade (WPNBlockBladeX / WPNBlockBluntX); on a person only
  the light pulse, as before.
- Switches: the own shield and blade with `WeaponStopsAtWalls` /
  `HandsStopAtWalls`; the foes' with `WeaponStopsAtBodies` /
  `HandsStopAtBodies`. The parry is unchanged: its foes are the same list,
  with a foe listed for a shield alone (`bladeValid`) skipped by it.
- Log: "Contact: the blade rests on your shield / your blade / <actor>'s
  blade / <actor>'s shield ...", "... pressed on into your shield ...";
  "Contact: <actor>'s shield read - n vertices, spokes ..." once per foe;
  at a mark "stoppers n capsules and n discs" and "Contact: disc 0 (your
  shield) - centre ..., normal ..., round, thick"; the hands' mark line
  "stoppers a+b and c+d (capsules+discs)".
- Tests: `blade_contact_test` (the disc's geometry and every way in, the
  ids; the step held by a disc at any speed, no hit-stop, pressed past and
  passed - not through - and held again, a capsule stopper against a swing,
  the body behind it, across a thin stopper, taken up inside one, the
  ledger with discs, no stoppers), `hand_contact_test` (the disc in the
  world, the blade's capsule), `parry_test` (the foes' stoppers).
- **Harness**: `blade-shield.txt` and `hand-blade.txt` (see
  docs/hand-script-harness.md for the results).

### 4.33 Fighters walk near you (built 2026-10-09, harness PASS)

The tester: "baue es dass gegner zwar wie vanilla zum player rennen
dürfen, aber sobald sie im umkreis von 10 metern sind dürfen sie nur noch
langsam gehen, so wie in blade & sorcery. (neue settings, default an)
kämpfe in vr müssen entschleunigt werden."

- `[Hands] SlowApproach=1` ("Enemies walk near you"), `SlowApproachMetres`
  (5; "Walk within (m)", 2-50; 10 at first - the tester, having played it,
  2026-10-10: "Walk within (m) muss per default auf 5m").
- **How** (game/SlowApproach.h): an actor's movement flags (HighProcess
  +0x1FC; xOBSE: Walk 0x100, Run 0x200) are set by the AI through two of
  HighProcess's own virtuals, read in the binary - vtable +0x2C4
  (0x00A71AD8 -> 0x00631B90, SetMovementFlag(flag, on)) and +0x2C8
  (0x00A71ADC -> 0x00631B50, SetMovementFlags(flags)). Both entries are
  rerouted; for an actor in combat (Actor vtable +0x334) within the radius
  of the player a run asked for becomes a walk - the game's own walking
  legs and walking speed - and a flag already set is turned once a frame.
  A metre of slack on the way out, so one at the edge does not flicker.
  Beyond it, and for everyone not fighting, nothing changes; the player is
  never touched.
- Log: "Slow approach: <actor> is near - walks - ... moving N units/s",
  "... is no longer near - runs as it will"; a state line at a hand
  script's mark with the flags and the speed seen.
- Tests: `slow_approach_test` (the flag turn, who walks and the slack, the
  speed seen, the radius). Hand script `slow-approach.txt`: PASS 2026-10-09
  with the radius at 2 m (the scripted player only walks, so the bandit
  never fell past 11 m in the first run): the bandit walked within it
  (flags 0101, "moving 58-104 units/s"), ran again past 3 m ("0201 (run 1),
  moving 357 units/s"), and walked again when near.
- The tester's round of f76558f: "parade geht, wände geht, npcs geht";
  "gegner kommen jetzt sehr nah ran, die wollen einen wrestlen" - that is
  `CombatReach` 85 (4.32: they strike from 1.3 m instead of 2 m so a parry
  can reach their blade), and now they walk the last metres; "Combat reach
  (units)" is the knob (100 is 1.4 m, 0 the game's own 2 m).

### 4.34 The off hand strikes and grabs, as in Blade & Sorcery (built 2026-10-09/10, the ragdoll hold 2026-10-13, headset open)

The tester: "man kann mit der off hand gegner packen oder hauen wie in
blade & sorcery" - "ne starte jetzt". game/GrappleLogic.h (grapple_test),
the off hand being the one without the weapon (the left; the right for a
left-handed player, the roles swapped before anything here sees them).

- **The strike** (`[Hands] OffHandStrikes=1`, "Off hand strikes"): with a
  blade or a blunt weapon drawn the off hand, its grip loose and not on a
  two-hander's handle, driven fast into someone is the shove's blow
  (4.12): from ShoveSpeed they stagger and are pushed, from ShoveHardSpeed
  knocked down - a fist (a punch: the hand-to-hand hit WPNHitHandX
  0000C3CA, a pulse, never a slap) or an open hand alike. With the weapons
  away the hands are the shove's and the fists' already, so nothing there
  changes. Logged "Shove: the left hand at n m/s towards <ref> - light
  (the off hand's punch, a weapon drawn)".
- **The grab** (`OffHandGrabs=1`, "Off hand grabs fighters";
  `GrabHoldSeconds` 4, "Held for (s)"; `GrabFatigue` 15): the off hand's
  grip closed within 6 units of one of the capsules on a fighter's bones
  (4.32's capsules, each now named: head, neck, torso, arms, legs) holds
  them: they stagger, their legs are held still (the same two movement
  setters as 4.33: no step, no run, the turn kept), the hand drags them
  across the ground (placed where it wants them each frame, SetPos's own
  way, at most 2 m/s - the character proxy's knockback did not move them,
  harness 2026-10-10), and held by the head, the neck or the weapon arm (the
  right) their blows at the player do not land - the parry's forced block
  and cone (4.32), all of the blow, nothing owed for a parry. Let go with
  the hand moving at ShoveSpeed they are thrown - staggered and pushed
  along it; at ShoveHardSpeed knocked down along it. They break free after
  GrabHoldSeconds; a hand dragged 40 units from where it wants them (they
  are stuck) loses them; dead, knocked down, out of reach or the mode off
  ends it. The grip on someone not fighting is refused (leading by the
  hand, 4.12's neighbour, keeps those) and said once. The grab by reach
  (the Z key) is kept off that grip while someone is held.
- Hand script `grapple.txt`: PASS 2026-10-10 on the fifth run (docs/hand-script-harness.md):
  held by the torso, the legs held, kept within 0.3-12 units of the hand
  for 0.8 s against the stagger and the AI, thrown down at the open. The
  first runs taught two things: the bandit stands to the scripted player's
  left and behind (the grid was ahead), and the character proxy's knockback
  does not move a held actor (a frame's push: nothing; re-issued over 0.3 s:
  a few units while the stagger carried them off) - so the drag places them
  (game::PlaceActorAt). Not seen in the headset. What to watch: how the
  drag feels (a placed actor, not a pushed one), whether a held bandit
  still swings (held by the torso or a leg they do), and the punch.
- **The punch with a little finger that never reads (the tester,
  2026-10-12: "gegner mit linker hand slappen geht (cool!) aber mit faust
  hauen nicht (mein linker controller da geht der kleine finger nicht zu
  krümmen für die faust)").** His log: the left little finger read 0.00 in
  all 52 curl lines of the run while the left middle and ring closed to
  1.00 (the right little finger reached 1.00). A fist needs all four
  fingers past the close limit, so the left hand was never one and every
  blow was a slap; no "Fist at" setting helps a sensor stuck at 0. Now a
  finger whose sensor has never gone past 0.1 since the start is left out
  of the fist, closing and opening, as long as three of the four have read
  (`vr/FingersRead.h`; `game::HandBodyFist` and `vr::StepFist`); a working
  finger reads the first time it closes and from then on counts again, so
  the pointing hand and the middle finger stay no fist. Logged once "Hands:
  the left hand's fist goes by 3 fingers - one never read (index read,
  middle read, ring read, little never; ...)", and "the left hand made a
  fist" / "opened" with the curls. The fist decision also moved out of the
  hand bodies' block: with `BodyCollision=0` it was never made, and the
  off hand never punched. Harness `offhand-punch.txt` (a longsword drawn, a
  beggar frozen ahead, the left hand closed with `left little 0` and driven
  into them): PASS 2026-10-12 - "fist goes by 3 fingers", "made a fist
  (curls 1.00 1.00 1.00 0.00)", "the left hand at 54.4 m/s towards ... -
  hard (the off hand's punch, a weapon drawn)", the beggar knocked down.
  The script's second blow, the hand open, met nobody (the beggar lay
  where the punch put them), so the open hand's slap was not tried again
  here; it is the slap of 4.12, unchanged.

- **The ragdoll hold (`[Hands] GrabRagdoll=1`, "Grab takes them down";
  `GrabThrowStrength` 1.5, "Grab throw strength"; built 2026-10-13, the
  tester: "npc ragdoll halten und mit kraft wegschubsen oder sogar auf dem
  boden zu schubsen mit linker hand während des kampfes").** The off hand's
  grip closed on a fighter's torso, an arm or a leg knocks them down into
  their ragdoll - the engine's own knockdown, no push - and holds the part
  under the hand: its Havok body is driven to the hand each frame, the rest
  of them hangs from it (game/RagdollHold.h; physical-combat-spec section 8
  has the engine's side). One already lying knocked down is taken as they
  lie. Let go with the hand moving at ShoveSpeed - in any direction - the
  held part flies with the hand's speed times GrabThrowStrength: away, up,
  or shoved along the ground; a slow hand, or the strength 0, drops them.
  While held the engine is kept from standing them up (its get-up clock is
  stamped anew every half second); the hold ends when it stands them up
  anyway ("stood up"), after GrabHoldSeconds, with the body 40 units from
  the hand, or with them dead or gone. The head and the neck keep the
  standing hold above (the choke), as do creatures and a part without a
  Havok body on its bones. The player's fatigue as for the standing hold.
  Logged "Grapple: the off hand takes <ref> by the <part> (n units off it)
  - knocked down into its ragdoll by the engine's knockdown, no push (knock
  state 2); the <bone> bone's body <id> is held by the hand" (or "already
  down - taken as it lies"), each change of the knock state with its time
  ("knock state 2 -> 1 after 0.01 s"), "Grapple: holding <ref>'s <part> as
  a ragdoll n s - the body n units from the hand, driven at n m/s (motion
  type 4), moved n units so far, knock state 1, the engine's clock stamped
  anew n times, the drive refused 0 frames", and at the end "Grapple: <ref>
  thrown - the hand at n m/s, held n s as a ragdoll by the <part> ... the
  held part sent off at" with the units a second on the next line.
  Harness `grapple-ragdoll.txt` PASS 2026-10-13 (docs/hand-script-harness.md):
  two holds, by the left arm and the torso, the body 1-5 units from the hand
  through 0.8 s, thrown at 40 m/s; the bandit stood up between them.
- **The weight (`GrabWeight` 1, "Grab weight (x)"; built 2026-10-13 after
  the tester had the hold in the headset: "ja geht geben wir den gehaltenen
  gegnern noch gewicht").** The held part is no longer snapped to the hand:
  the change of its velocity each Havok step is cut to a pull from the
  player's Strength against the part's Havok mass (physical-combat-spec
  section 8 has the budget and the engine's side), so the body lags behind
  a fast hand, sags under a still one, and one too heavy to lift stays on
  the ground and is dragged; the throw is as before. 0 is weightless (the
  hold as first built), 2 twice as heavy. Logged at the take "Grapple: the
  pull on <ref> - the <bone> body's mass n, the ragdoll n in n bodies
  (Havok's masses), gravity n; Strength n, GrabWeight xn: a budget of n,
  which lifts n (0: no limit)", and the holding line carries "pulled at n
  m/s^2 (n of n changes cut to the budget)". Harness `grapple-ragdoll.txt`
  PASS 2026-10-13 15:54 with the weight (physical-combat-spec section 8 has
  the numbers: a Strength-25 character's budget lifts 75 of the ragdoll's
  530, the held arm 20 to 31 units behind the hand, every change cut, the
  throw as before); not seen in the headset.

### 4.35 Walking with a reached menu open, and switching containers (built 2026-10-10)

The tester: "es muss möglich sein weiterhin sich normal zu bewegen mit
stick links bei den neuen offenen menüs, wenn ich zu weit weg gehe
schließt es sich automatisch. und ich kann zwischen container und deren
menüs wechseln ohne probleme indem ich meine hand dem jeweiligen container
annähere" - with Loot Menu (Nexus 48027, a Fallout-4-style container panel
with no menu mode at all) as the picture.

- **The engine's gate.** The update step asks IsMenuMode once more at
  0x0040DC61 to choose between the player's own update (vtable +0x228, its
  input and walk in it) and a short menu-mode one (0x0065E900, which only
  stops an animation group) - the site deliberately left alone by the
  unpaused menus (GameAddresses.h). It is now redirected the first time it
  is wanted (`game::SetPlayerWalksUnderMenu`) and answers "no menu" only
  while the sticks move the player under a container's menu the reach
  opened (`PlayerControlsRunUnderMenu`, menu_pause_policy_test; logged
  "Menu pause: the player's controls run - the sticks walk under menu
  Container"). In those frames the laser's cursor steps and the click are
  held back - the player's input would take them as a turn and an attack
  (`PlanHandControls` with `walkUnderMenu`, hand_mode_test; the left
  stick walks and runs, the right turns, the right stick alone keeps the
  wheel). The walk-direction steer runs under it too.
- **Closing** (vr/ReachOpen.h, reach_open_test): once the player has
  walked with the menu up - the stick, or the feet 35 cm from where they
  stood at the open - the hand's leaving no longer closes it ("the player
  walks with it open - now only walking away closes it"); the player's own
  distance from the container does, `[Hands] ReachWalkAwayMetres` (1.5;
  "Walk away to close (m)"), measured from the feet lifted to the thing's
  height to its shape. Before any walk the hand away closes it as before
  ("hand weg -> menü weg", 4.31).
- **Switching**: a free hand within ReachOpenMetres of another container
  while one's menu is up closes it for the other (phase Switching) and
  activates the other once the menu is down ("a hand at another - closed
  for it", "the other one reached - activated"); if the menu never closes
  (30 frames) it is left to the player.
- Hand script `reach-walk.txt`: two chests a metre apart (the second placed
  after a walk), the second opened by reaching, the player walking back
  with it open, the hand onto the first (the switch), then walking off (the
  walk-away close). docs/hand-script-harness.md has its result.
- Open: the tester's two notes of 2026-10-09 for the reached menus - the
  hand not to vanish while the panel is up, and the panel touch-sensitive
  (docs/container-touch-spec.md section 6).

### 4.36 The laser as the cursor: placed, not steered (built 2026-10-10, headset open)

The tester: "wie kann man das hauptmenü noch verbessern damit die buttons
gut mit dem laserpointer funktionieren. bisher wirkt das nicht sehr
robust" - the analysis is docs/main-menu-laser-analysis.md; "setzt das
um". Items 1, 2, 3, 5 and 7 of it, nothing seen in the headset yet:

- **The cursor is written, not walked** (`[Hands] LaserCursorDirect=1`,
  "Laser places the cursor"; game/MenuCursor.h). The engine's tile search
  (0x00581390, the one that decides the hover highlight and what a click
  activates, called once an interface update) is detoured at its entry:
  with a laser or a finger on a game menu this frame, the two floats it
  reads as the cursor (InterfaceManager +0x2C/+0x34) are set to the beam's
  own pixel right before it runs. No gain, no step cap, no mouse route and
  no dependence on the window's focus - the 2026-10-07 class of "the
  cursor stayed at 0,0" cannot happen for the hover any more (the click
  still goes through the mouse button and its route). The mouse steps are
  not sent while a pixel is placed (until 4.37: they are again, for the
  drags); 0 restores the old steering. Logged
  "Menu cursor: the tile search at 00581390 takes the laser's pixel as the
  cursor" and, the first six times, "Menu cursor: placed at x,y before
  the tile search (the engine's own stood at a,b)".
- **A button, not a touch screen, where there are only buttons**
  (`game::MenuTakesTouchPress`, menu_type_test): on the main menu, the
  pause menu, a message box, the level-up, the credits and training the
  trigger's pull with the beam on the menu puts the mouse button down and
  holds it 0.12 s (`vr::StepButtonPress`, hand_input_test; the ready-weapon
  tap's lesson), the beam's wobble changes nothing, the trigger has to
  come up before the next; the cursor is frozen for the press. Everything
  with a list, a slider or a page to drag keeps the touch-screen press
  (the click on the release, a drag that scrolls, 4.4's neighbour).
- **The flat picture at a depth** (`[Render] FlatDepthMetres=2.0`,
  "Flat picture depth (m)"; render/EyeMirror): each eye's copy of the main
  menu, a loading screen or the cinema screen is moved towards the nose by
  the parallax of a point that far ahead (`FlatParallaxPixels`,
  `FlatShiftThatFits`, eye_geometry_test; about 13 pixels an eye on this
  headset's numbers), so the eyes fuse it where the laser's plane and its
  dot are, instead of at infinity with the dot two metres nearer. The
  laser's plane takes the same depth. 0 is the old infinity. Takes effect
  when the eye copies are made - at the start and again as the camera
  frustum arrives.
- **A dead band** on the laser's pixel (`vr::CursorHold`, 0.15 % of the
  layer's height, three or four pixels): a tremor smaller than that keeps
  the cursor where it is, a larger move takes the new point whole.
- **The flat picture is never taken away from under a laser on it**
  (`StepFlatFollow` with `pointing`, flat_follow_test): the follow's
  second starts over once the beam leaves.
- Not built: the direct tile click (item 4 - with the cursor placed, the
  engine's own click path lands on the same tile), one cursor route (6; the
  click's mouse button still goes by the window's state), a main-menu hand
  script (8; the runner loads the save first).
- To check in the headset: the main menu's buttons light where the beam
  is and take the click on the pull; an in-game list still scrolls by the
  trigger's drag; the flat picture reads at a comfortable depth and the
  dot sits on it; the log's "Menu cursor:" lines.

### 4.37 Drags with the cursor placed: the engine's own cursor leads them (fixed 2026-10-10, harness PASS)

The tester, on 4.36: "super fühlt sich besser an, aber: slider kann man
nicht ziehen. sidebar scrolls auch nicht. die map im Tab menü auch nicht."

- **Why** (read in Oblivion.exe): a drag never looks at the two floats the
  laser writes. While the left button is held the interface update calls
  slot +0x20 of the pressed tile's menu every frame (0x0058341A, the tile
  at InterfaceManager +0x98, its menu at +0x9C); the press itself went to
  slot +0x08. The map's pair (MapMenu, vtable 0x00A6CC9C by its RTTI:
  0x005B68F0 and 0x005B69B0) stores the cursor on the press and on every
  drag frame adds its move since to the world map's pan traits
  0xFB8/0xFB9 - read from the manager's +0x20/+0x28, the cursor sprite's
  own translation. Only the cursor update 0x0057E7C0 moves that, from the
  mouse's own deltas (0x00403190 axes 1 and 2), and only then derives
  +0x2C/+0x34 and sets the moved flag +0xB9. 4.36 sent no mouse steps
  while it placed the pixel, so the engine's own cursor stood still and
  every drag moved by nothing.
- **And the search**: the tile search runs only while +0xB9 is set
  (`je 005833FC` at 0x005823FD). The harness's control run (the 4.36 code,
  the map opened with F4) never reached it: no "placed at" line, the
  active menu "none", the laser dead on the map. In the tester's session
  the flag was set (his log has the pixel placed from the first menu on),
  so 4.36 worked there for everything but the drags; what set it there is
  not known - one writer is the manager's reset at 0x0058043E, another the
  cursor update itself.
- **Now** (vr/HandMode.cpp `PointAtMenu`, game/MenuCursor):
  - The mouse steps go out again in the placed mode too, reckoned from
    the engine's own cursor: taken in the search's entry before the write,
    whenever the floats hold something other than the pixel written last
    (`game::EngineMenuCursor`, `CursorReadIsEngines`, menu_cursor_test).
    The engine's own cursor follows the beam, the search runs, the pixel
    is placed as before.
  - While the touch-screen press holds the button (a sideways drag, a
    scroll bar, the map) nothing is written: the engine's own cursor -
    walked after the beam - leads the drag and the hover with it
    (`HandModeResult::menuDragging`).
  - While the press runs and has not become a hold, or while it scrolls,
    the point stays where the trigger was pulled (as the button press's
    did): the button goes down on the slider's marker, not the drag's 2 %
    beside it, and the wheel's notches go to the list the drag began on.
  - hand_mode_test `TestLaserDragDirect`: the slider, the scroll bar, the
    map, the list, and the same with the steering mode.
- **Proof** (`tools/hand-scripts/laser-drag.txt`, harness PASS): the map
  opened with F4, the trigger pulled on "map_world_map" and the hand turned:
  "the engine's own cursor went from 1323,961 to 1680,1349, the map's pan
  from 2037.7,1657.4 to 1886.7,1493.4". The control run on the old code
  did not even reach the search. The game window's screenshots kept the
  map where it was - the monitor's copy of the menu is not redrawn - so the
  pan is read from the map menu itself (`game::MapMenuPan`).
- Not run in the harness: a slider and a list's scroll bar (the save has
  two weapons, no list long enough to scroll; a slider needs the options
  menu or the quantity popup). Both go through the same per-frame call to
  the menu's slot +0x20 with the button held; their own slot +0x20 was not
  read.
- Logged, the first twelve times: "Hands: a drag held in the X menu - the
  engine's own cursor at a,b, the beam at c,d" and "Hands: the drag let go
  - the engine's own cursor went from a,b to c,d" (on the map with its pan).

### 4.38 The race menu's name empties on the next click: vanilla (read 2026-10-10)

The tester: "bei textfeldern ... man kann eingeben mit dem vr keyboard aber
wenn man dann accept drückt, und man erneut auf das textfeld drückt,
verschwindet der text den man vorher eingegeben hat. ist das vanilla
behaviour?" His log has it in the race menu (`race_name`): the keyboard
opened, letters typed, Done; opened again on the next click, Done with
nothing typed.

It is the game's own (read in Oblivion.exe): RaceSexMenu (vtable
0x00A6D38C by its RTTI) handles a click in 0x005CC200. A click while the
name is being edited ends the editing and keeps the text (0x0057DD90 with
0, then 0x005C30C0 writes it to the name tile and the player's name). A
click on the name (id 0x0A) while it is not being edited calls 0x005C2730,
which sets the name editor's text to "" (0x0057FF20 with 0x00A2F7EC, an
empty string) and only then starts the editing (0x0057DD90 with 1) - the
only place in the race menu where the name's editing starts. So every new
edit of the name begins empty, with a mouse as with the laser. Left as it
is, as he said.

Not changed, for later if it bothers: Done in the naming menus types no
Enter (4.14), so the game's editing is still running after it;
a click on the name then ends the editing (the text stays) while OBVR opens
the keyboard again - and what is typed then goes nowhere until the next
click starts a new, empty edit. Whether the click that ends the editing
came from the trigger still held as the keyboard closed (the controllers
are gone from OBVR while SteamVR's keyboard is up: "right controller not
tracked" in his log right after each opening) was not told apart.

### 4.39 Sliders by their knob, and the trigger near a menu (fixed 2026-10-10, harness PASS)

The tester, on 4.37: "slide gehen nicht ... wenn ich ans menü rangehe dann
ist es nicht mehr klickbar im hauptmenü".

- **The slider** (read in Oblivion.exe and measured in the harness): the
  options pages' drag handler (slot +0x20 - GameplayMenu, vtable 0x00A6BD0C,
  0x005A3460) moves a slider only for the pressed tile with id 2 (trait
  0xFA8): the knob, "horizontal_scroll_marker", 39 layout units wide. The
  wide "gameplay_difficulty_slider_marker" under it is the track (id -1),
  and the bar's sides and arrows are -1 too. Every pull in his log landed on
  the track, 20-40 pixels under the knob; a harness drag pressed on the knob
  moved it (x 0 -> 292, value -1 -> 0.6), one pressed 20 pixels under it
  moved nothing - on the 4.37 build and in the old steering mode alike. So
  it was never the drag: the knob is a small target.
- **The knob snap** (game/MenuCursor `DecideKnobSnap`, menu_cursor_test): a
  laser press whose first tile is a slider's or scroll bar's part other than
  the knob (`IsScrollPartName`) tries the engine's own tile search at rings
  round the beam - four rings 1 % of the layer's height apart, eight points
  each, up and down first (`KnobProbeOffset`) - and takes the first knob
  (`IsScrollKnobName`, a name ending in "scroll_marker"). The press and its
  drag carry that offset (`HandModeFrame::knobSnap*`); the knob under the
  press holds the button at once (`StepLaserPress`: a Pressed press over a
  knob becomes a hold). Logged "Menu cursor: a press on "..." takes the knob
  "..." dx,dy pixels off the beam" or "- no knob within n pixels".
  `tools/hand-scripts/slider-knob.txt`: the pull 20 pixels under the knob
  takes it 23 pixels up, the drag moves it (x 3.6 -> 292.4).
- **Near the menu** (hand_mode_test `TestNearMenuLaser`): with the tip within
  10 cm of the panel the finger took the cursor (the poke's hover, 4.4's
  neighbour), so a pull aimed with the laser clicked under the finger. Now
  the finger presses only while it is out - the trigger under 0.2 and, with
  the skeleton, the index not curled (`FingerPokes`); a pulled trigger is
  always the laser's. A hand pushed through the panel still reaches nothing
  (the beam starts behind it).
- The Pause menu's buttons take the laser's click at every distance short of
  that (`pause-near` probe, 2026-10-10). The main menu (a flat picture) has
  no finger and was not changed.
- Not seen in the headset.

### 4.40 The main menu's laser after a step (fixed 2026-10-10)

The tester: on the main menu at the game's start "gehen buttons nicht mehr
wenn ich nicht ganz im fokus bin, zb einem m vor gehe".

- The flat picture (the main menu, loading screens) is drawn into the eye
  images: its direction stays where it was anchored, but it goes wherever
  the head goes, `FlatDepthMetres` ahead of it. `LaserOnFlatPicture` put its
  stand-in plane that far ahead of the ANCHOR's position. Since 4.36 the
  anchor is no longer taken along while the laser is on the picture
  (`StepFlatFollow` with `pointing`), so after a step forward the plane stood
  a metre short of the picture the eyes saw, and the pixel the head saw the
  beam's end against slid by the hand's distance from the head - the
  buttons stopped taking the beam. Before 4.36 the follow re-anchored the
  picture after a step, which hid the same error.
- Now the plane stands that far ahead of the HEAD, along the anchor's
  forward (hand_input_test: a metre forward, the same pixel for the same
  hand, the beam still two metres long).
- Not run in the harness: the runner starts from a save, and the main menu
  reached by quitting from the Pause menu is shown on the room's quad, not
  as the flat picture (`mainmenu-step` probe, 2026-10-10; the hand script's
  marks now log the flat picture's anchor when there is one). Not seen in
  the headset.

### 4.41 The hands stay in the game's menus (built 2026-10-11)

The tester, 2026-10-11: "mach es sodass Hände in game in allen Menüs außer
ESC Optionen Menü erhalten bleiben und nicht ausgeblendet werden".

- Before: every menu, and a conversation from its first frame, took the
  hands away - "Hand,Weapon,Torch,Bip01 L ForearmTwist" hidden
  (`ComposeHandsHideList` with `handsAway`), and the pins stood down, since
  the camera pass that decides `g_handArmsWanted` does not run while the
  simulation is paused and the hands would have hung where the controllers
  last put them (2026-09-25).
- Now `[Hands] HandsInMenus=1` ("Hands stay in menus"): the hands stay -
  pinned to the controllers and drawn as in the world - over the inventory,
  the magic, map and stats pages, a container, a book, the quantity and
  magic popups, the lockpick, sleeping, a message box, a level-up and a
  conversation. They go only for the Esc menu and what opens from it (the
  options, saving, loading, a quit box over it), the main menu, a loading
  screen and the credits (`camera::MenuTakesTheHands`). The decision reads
  every entry of the interface manager's menu stack
  (`game::MenuStackEntries`, the ten slots GetTopVisibleMenuID scans), not
  the top alone, so the options or a message box over the Esc menu still
  find it beneath them (`camera::HandsAwayForMenus`). An empty stack read
  in menu mode keeps the hands. Off, every menu takes them as before.
- **How**: `HandsAwayNow` decides at Present (UpdateHandMode) and again at
  the render (`BeforeFirstScenePass`, for the first frame of a menu); the
  hide list and the forearm stump follow it. In a menu frame the render
  pins the hands itself when they stay (`pinHands`: Full VR, first person,
  the headset on, the arms valid) - the camera pass does not run there - and
  HandMode now gives the arms' placement in menus too (`armsValid`), so the
  arms are placed first and the hands under them as in the world. Where the
  camera pass does run under a menu (UnpausedMenus, the container panel),
  `g_handArmsWanted` keeps the arms placed while the hands stay
  (`g_handsStayInMenu`).
- Log: "Hands: menu <name> (0x...) - the hands stay - pinned and drawn as in
  the world" / "- the hands go", "Hands: no menu - the hands as in the
  world"; at a hand script's mark "Hands: at the mark - menu <0|1> (<name>),
  the hands <shown|away>, pinned <0|1>".
- Tests: `frame_logic_test` (which menus take the hands, the stack with the
  Esc menu beneath the options or a quit box, a message box in the world, a
  conversation, a container with its quantity box, the empty read, the
  setting off); `hand_mode_test` (the arms placed in a menu with the setting
  on, not with it off).

### 4.42 No black HUD frame while a game loads (fixed 2026-10-11)

The tester: "beim spielladen ist kurz das hud als schwarzer rahmen zu sehen,
vll können wir den entfernen."

- **What it was** (the tester's log of 2026-10-11): a loading screen is a
  flat picture (cinema delivery, the 2D layer's overlay hidden), but the
  first stereo world frames after a load - the cell loaded, the loading
  menu still on the interface manager's stack - captured the loading
  menu's 2D into the layer and showed it on the overlay ("Hud: live" came
  while "menu Loading" was still the top): the loading menu's frame on a
  black ground, hanging 1.2 m in front of the eyes until the menu closed.
- **Now** (`camera::HudOverlayShown`): the 2D layer's overlay and the hand
  HUD are hidden on any frame while the game loads - the loading menu on
  top of the stack, or the loading thread active - and shown again once
  the menu is gone. Logged once per load: "Hud: a world frame while the
  game loads - the 2D layer and the hand HUD stay hidden".
- Tests: `frame_logic_test` (shown on a world frame alive and not loading;
  hidden off a world frame, for death, while loading). Harness: in the
  `hands-in-menus` run of 2026-10-11 the line came between "menu Loading"
  and "menu none", and "Hud: live" only after. Headset: open.

### 4.43 No drop from the inventory with the left A behind another window (the tester, 2026-10-12 night; fixed 2026-10-13, unproven in the game)

- **Reported:** "kann im inventar nicht mehr items droppen mit links A" - on
  the builds of the evening of 2026-10-12. The drop by the left A over an
  item in the inventory worked on 2026-09-27 (`inventory-drop.txt`, PASS:
  Shift held, a click under the cursor, one item dropped).
- **Found 2026-10-13.** The harness reran `inventory-drop.txt` on the build
  of fd9ec72 and the drop worked: the items line at "held" names a new
  weapon reference with a dynamic form id 102 units from the eyes, the
  dropped sword. The difference to the tester's game is the window in
  front: his log says "Input: the game's window is behind another window -
  the controllers go straight into the game's input" before the inventory
  rounds, so his keys went through the engine route (4.36's background
  input of 2026-10-09), while the harness's drop went through Windows with
  the game in front. His log also holds three clicks on "Steel_Longsword"
  with the triggers at 0.00 - the drop's own clicks, the left A - and after
  each the sword's scabbard hidden: the item EQUIPPED, as a plain click
  does.
- **Why, read in Oblivion.exe 1.2.0.416:** the engine reads the keyboard
  two ways. Its state (GetDeviceState into CurrentKeyState, +0x18F4 of
  OSInputGlobals) is what the controls answer, and what the engine route
  writes. Its buffered EVENTS (GetDeviceData, one at a time, through the
  reader at 0x00403C90: 1 for a down, 2 for an up, the DIK code through a
  pointer) are what the interface manager drains each frame (0x005834B9)
  and keeps its modifiers from (0x0057F7A0: Left or Right Shift down sets
  bit 2 of its +0x118, up clears it; Ctrl bit 1, Alt bit 0) - and
  `InventoryMenu::HandleClick` (0x005ABBC0, the vtable's third entry) takes
  the item click for a drop by that bit (0x005ABD80: `[manager+118h] >> 2 &
  1`). With the game behind another window the keyboard device is not
  acquired and yields no events, so a Shift held in the state alone was
  never a modifier, and the click was a plain click. (The flag 0xB3B43D
  tested first in HandleClick is the quick-key wheel's, menu 0x416, not the
  drop's; and the game imports no GetKeyState.)
- **Fixed:** a second detour at the reader (`addr::kInputNextKeyEvent`,
  game/EngineInput.cpp): every change of a key's held state through the
  engine route queues an event (`EngineKeyEvents`, InputRoute.h - a down
  once, an up once, none for the mouse buttons, which the game reads from
  the state), the reader hands the queue out before the device's own, and
  letting everything go (the route changing, the hand mode off) queues an
  up for each key held. The same events carry a text field's letters, so
  the VR keyboard's typing is expected to reach a field with the game
  behind another window now - not tried. input_route_test covers the
  queue; the detour's install is logged ("Input: the key event reader at
  00403C90 hands out ...") and refused with a log line when the bytes
  differ.
- **Open:** not run in the game (no game starts on 2026-10-13); the
  scenario for it is `inventory-drop-background.txt` (its one run that
  morning was spoiled by the runner's console text, see the harness doc).
  The tester's headset is the proof: the left A in the inventory with the
  game behind another window.

### 4.44 A spell ring on the other hand (the tester, 2026-10-15; built 2026-10-16)

"neue hotkey für linken controller. linker stick press nicht mehr quests
sondern eine zauberbuch als hotkey wie wir das schon rechts haben." Wanted:
the other hand's press opens a ring of spells, chosen and used as the hotkey
ring is (4.4), in place of the journal (F1).

- **Built (2026-10-16).** The other stick's *short* click opens the player's
  spellbook on the quick menu's ring at that hand, latched, the way the right
  stick's opens the hotkey ring: the hand moves to a slot, the next click
  makes that spell the active one - `PlayerCharacter::SetActiveSpell`
  (0x00664700), what the magic menu's click ends in, handed the SpellItem's
  MagicItem (the form + 0x18) as OBSE's SetActiveSpell command hands it
  (xOBSE Commands_Player.cpp) - a click in the middle closes it. The stick
  *held* past `[Hands] StickHoldSeconds` stays the run: held, the run control
  is down from the press on (a short click runs for that moment, nothing
  more); with `[Hands] RunToggle=1` the hold reached switches the run, the
  click being the ring's now. A press the chord took is neither.
- **The slots: the spellbook, not a list of its own.** Every spell the player
  knows that the magic menu would cast - spells, greater and lesser powers;
  not diseases, not abilities (a SpellItem's type at +0x38) - read from the
  player's base form's TESSpellList (TESActorBase +0x54, its first node
  embedded at +0x58; xOBSE GameForms.h, OBSE's GetSpells reads the same) each
  time the ring opens, in the order of the names (the case left out, the same
  name by form id), eight a page, up to ten pages (eighty spells; more are
  left out and logged), the other hand's trigger turning the pages - kept
  from the rest of the hands while the ring is up, as the quick menu keeps
  the weapon hand's (`vr::QuickMenuKeepsTrigger`). The page shown is kept
  between openings. A gold dot at a slot's corner marks the spell that is
  active now (`GetActiveMagicItem` 0x0065D4A0, matched against the book by
  its MagicItem); "Spells" stands under the middle. The icons are the game's
  own (a spell's first effect's, 4.4) and `QuickMenuIcons`, `QuickMenuRingMetres`
  and `QuickMenuDeadZoneMetres` are shared with the quick menu. `[Hands]
  SpellRing` (default on; the settings row "Spell ring" under Hands: menus)
  gives the click back to the run alone.
- **One ring at a time:** neither ring opens while the other is up; neither
  opens in a menu (the spell ring sets no hotkeys - the quick menu's 4.5 does
  that for spells as well).
- **Where.** `vr/SpellRing.h` (pure: the book, its order, the pages, the
  active slot; `spell_ring_test`), `game/SpellBook.h` (the engine reads and
  the two calls), the hand mode's `spellRingTap` and the left stick's hold
  (`hand_mode_test`: the short click's tap, the hold's run, the chord, the
  ring off, the toggled run moved to the hold), the painter's active dot and
  title (`quick_menu_painter_test`, with the ten-page dot row clear of the
  side slots), `UpdateSpellRing` in CameraHook.cpp over the quick menu's own
  `StepQuickMenu` with its own state. Log lines "SpellRing: opened at the
  other hand ... page N of M: 1 Flare, 2 ...", "SpellRing: slot N used (...)
  - the engine took it; the active spell now ...", "SpellRing: closed without
  a spell"; budget 30 lines.
- **Harness** `spell-ring.txt`: Flare added by the console, a short left
  click, the hand up to slot 1, the next click; then the stick held 0.9 s
  (the run, no ring on its release). See docs/hand-script-harness.md for the
  run.
- **Not tried in the headset yet.** Whether the ring at the left hand reads
  as well as the right's, and whether the active spell's HUD icon follows the
  set at once (the engine's own hotkey path for a spell was not read; the
  harness reads the active spell back through GetActiveMagicItem).
- **Open.** The Index's left trackpad click is still the journal (F1); the
  controls card's other-hand page names the spell ring in its place and the
  README's table keeps the journal row. A spell cast by the trigger right
  after a set was not timed.
- **Open (the tester, 2026-10-16: "thumbstick drücken öffnet spell menü oder
  quickmenü NUR auf quest controller. wenn ich index controller habe dann
  nicht. dort geht beides über linkes oder rechtes trackpad").** Wanted: the
  stick clicks open the spell ring and the quick menu on a Quest's Touch
  controllers only, which have no trackpad; on the Index both rings are the
  trackpads' - the left trackpad's click the spell ring, the right's the
  quick menu - and the stick clicks there open neither. Today the Index has
  both: the right trackpad's click is the ring's pad (CameraHook's
  `UpdateQuickMenu`, `in.pad`, the 2026-09-27 design) and the right stick's
  short click its tap (4.4, the swap of 2026-10-15); the left stick's click
  is the spell ring on every controller and the left trackpad's click the
  journal. Not built ("notiere"). The way: a hand knows whether its
  controller has a trackpad - the controller type string of the device
  (`Prop_ControllerType_String`: "knuckles", "oculus_touch", "vive_controller")
  read once when the hand connects, or whether the two trackpad actions have
  an origin bound (`IVRInput::GetActionOrigins`); with one, the rings' taps
  are the trackpads' clicks and the stick clicks are left to the walk, the
  run, the sneak and the Rest menu's hold; without one, the stick clicks as
  now. An INI override (`[Hands] RingsOnTrackpad=auto|stick|trackpad`) for
  the harness, whose scripted controller reports every action bound and
  would otherwise count as an Index, and for a wand. Cost: small to medium -
  the ring and spell ring inputs exist and only their source changes, the
  controller type is one property read, hand_mode_test gets the two
  controllers' flows; the risk is the harness scenarios built on the stick
  (`quick-menu-stick.txt`, `spell-ring.txt`) needing the override, and the
  journal losing its only key on the Index (the Tab menus' B keeps it).

## Beta reports of 2026-10-17 (two testers, Quest 3 over Virtual Desktop; the small items built 2026-10-18)

What was built on 2026-10-18 from these, each marked **Built** below: the corpse panel's edge
(`vr::ContainerAnchor`), the stow zone at the drawn hand (`vr::DrawnHandInTracking`), the
hands' pitch rows and the stick's turn speed row in the settings menu, the controls card's
door and chest lines with the stow ring on by default, the engine's auto vanity camera held
off (`game/VanityCamera.h`), and a log line for actions that stay dead
(`vr/ActionsWatch.h`). Open: the analogue stick speed, the left-handed mirror, a Touch
preset, the shudder's measurement, the sprint latch's cause.

**Tester A ("yellow landen", 0.3.0 beta 4, Windows, OBSE 21 - not xOBSE 22 as the README asks; his
OBVR.log and OBVR.log.prev of 2026-10-17 in the tester's Downloads, a 148 s recording of the Imperial
Substructure at 68 to 71 fps of a 72 Hz target, 45 to 48 ms latency):**
- *"super shuddery movement"*: the recording's frame rate sits under the headset's; his log has the
  monitor present without vsync. What OBVR itself costs on Windows is measured only in part (13.6 %
  reprojection on the developer's machine). Theirs and ours; the `[Performance]` capture and the
  SteamVR frame timing would settle it. Cost of the measurement: medium.
- *"cant open doors not sure how to pick up items"*: `[Hands] TakeOnlyByHand` is on by default, so
  the activate button takes no loose item - the grip holds it and the chest stows it (vr/Stow.h); his
  log shows two stows ("Stow: the held item is at the body") and "Pick: a door or the like under the
  laser takes it from the items (the laser's pick, Activate opens)". A door is the laser and A. Ours as
  words: the controls card says neither; the stow spot ring (`StowSpotVisible`) is off by default.
  Cost: small (the card's lines, the ring on by default for the beta). **Built 2026-10-18:** the
  weapon hand's card says "A: open a door, talk, use what the laser points at - not pick up" and
  "Grip on an item: hold it. Let go at your chest: kept. Mid-swing: thrown" (ui/NativeControlsCard.h,
  README's table the same; native_settings_test checks the words), and the stow ring shows while
  an item is held by default (`StowSpotVisible=1`, vr/Stow.h, the template INI).
- *"ui of dead bodies is oftend flipped the wrong way"*: `vr::ContainerAnchor` (DialogPanel.h) takes
  the panel's right edge from the horizontal part of the line of sight; over a body at the feet that
  part is a sway of the head, so the edge turns round between frames and the panel shows its back. His
  log: "its face tilted 58 to 68 degrees" on the corpses. Ours. Fix: the head's own right edge once
  the line is steeper than about 60 degrees. Cost: small (pure, dialog_panel_test).
  **Built 2026-10-18** (`vr::ContainerAnchor`, `ContainerEdge`): two mechanisms, both turning the
  panel upside down to the viewer, since the geometry allows both and his log (41 to 84 degrees
  steep, placed over the body's bound centre, which for a ragdoll is where the body's middle
  lies, not where the hand is) does not say which he saw. Over a line steeper than 60 degrees
  the edge is the head's own right, levelled and squared to the line (the level part of such a
  line is a residual of where the head stands over the body). Over a shallower line the edge
  stays level across the line of sight, but an edge that points against the head's right - the
  body's level direction behind the head's facing, as when one stands over or beside a corpse
  and reaches it - is turned round. dialog_panel_test covers both, the chest rule unchanged.
  The Container log line names the edge taken. Harness: `reach-open-body-behind.txt` PASS
  (a corpse 0.8 m ahead, the head turned round over it, 46 to 49 degrees: "turned round to the
  head's right"); `reach-open-body.txt` for the steep case: not produced in five runs (the corpse lies 0.8 m
  ahead whatever placeatme is asked, a line of 56 to 58 degrees, and walking onto it shoves
  it) - docs/hand-script-harness.md; the rule is covered by dialog_panel_test. The headset
  check is open.
- *"smooth turning is waaaay too slow"*: `[Hands] TurnSpeed` (12) is mouse pixels a frame at full
  deflection (game/HandControls.h), so the turn rate hangs on the game's mouse sensitivity and the
  frame rate, and the row is not in the settings menu (only `[Look] TurnSpeed`, the eased turn).
  Ours. Fix: the row (small); the turn in degrees a second through the camera's yaw as the snap turn
  does (medium). **Built 2026-10-18:** the row "Stick turn speed" under Hands: moving, 1 to 60
  (settings_list_test). The degrees-a-second turn stays open.
- *"left handed mode inverts hands to face the wrong direction"*: `LeftHanded=1` swaps the
  controllers' roles as a whole and the pin puts the game's right hand, with the right hand's
  calibration, on the left controller - a mirror image. Known as a limitation (section 4; the body
  keeps its own arms). Proper support pins each physical hand as itself and moves the weapon to the
  other bone: medium to large; not now, play right-handed.

**Tester B (avolpe, Quest 3 over Virtual Desktop, 120 Hz, RTX 5070 Ti; his 15 s clip shows the
Hands page with Right hand roll 90, yaw 175, Left hand roll 95, Hand forward 15):**
- *the joystick emulates WASD, no analogue speed*: true - the stick presses the move keys
  (game/HandControls.h, HandKeyMap) and the direction is continuous only because the body turns to
  the walk direction (vr/WalkDirection.h); the speed is walk or run. Analogue speed needs the engine's
  movement speed set each frame from the deflection: medium, unmeasured in the engine.
- *no pitch row for the hands*: the settings menu carries roll and yaw rows only
  (ui/SettingsList.cpp); `RightHandPitch`/`LeftHandPitch` exist in the INI, and the fit (Adjust hands:
  on, close a grip to hold the hand still, move the controller to where the hand should sit, open the
  grip) writes all three. His Touch numbers: pitch 50 both, `HandGripUpMetres` 0.04,
  `HandGripForwardMetres` -0.23. Ours. Cost: small (two rows); a Touch preset by controller type: small.
  **Built 2026-10-18:** "Right hand pitch" and "Left hand pitch" beside the roll and yaw rows
  (settings_list_test). The Touch preset stays open: the controller type is read nowhere yet.
- *robes and the Shrouded Armor show two sets of hands*: known (player-body-spec: robes that carry the
  hands in the upper body cannot hide the body's hands, they sit inside the first-person ones).
  Workaround `[Body] Visible=0`. Fix idea: the body's hand bones shrunk to nothing under such a robe,
  the way the bare wrist is shrunk (BonePin.h). Cost: small to medium, a robe in the harness.
  **Fixed 2026-10-18** with the third tester's fingers (below): the suit's hand is a shape named
  plainly "Hand" under the suit's node, hidden now (`BodyNodeHiddenOnArm`); the Shrouded Armor is
  cut like the Black Hand Robe.
- *performance drops with human NPCs in view, not with creatures*: unmeasured. The humanoid-only work
  is the contact capsules (`HandsStopAtBodies`, `SwingsStopAtBodies`, `PunchesStopAtBodies`,
  `WeaponStopsAtBodies`), `SlowApproach`, `CombatRing`, the attack tempo, the enemy bars and the NPC
  look-at. The `[Performance]` capture (docs/performance-profiling.md) names the cost; the toggles
  find it by halves. Cost to find: medium.
- *cannot put a held item into the inventory, letting go at the chest drops it*: the stow zone is
  measured at the controller (`BodyRelative(f.head, f.headPosition, cr.position)` in HandMode.cpp),
  0.17 m ahead and 0.33 m under the eyes, 0.16 m round; with his `HandGripForwardMetres` of -0.23 the
  drawn hand is 23 cm behind the controller, so when the drawn item touches his chest the controller
  is 0.40 m out - outside. Ours. Fix: the zone at the drawn hand (the controller plus the grip
  offset). Cost: small (stow_test, the harness stow scenario). The tester's call (2026-10-17):
  first point him to the settings - "Place the stow spot" and "Adjust hands" under Hands, and
  MenuQue for the menus - before anything is built. **Built 2026-10-18:** the zone and the
  ring's placing measure the drawn hand - the controller plus the shared grip and the hand's own
  (`vr::DrawnHandInTracking`, `DrawnHandTracking` in CameraHook; stow_test) - so the item that
  touches the chest is at the spot whatever the fit. Harness `stow-drawn-hand.txt` PASS with his
  -0.23: the controller 0.40 m out, the drawn hand 0.17 m out, "at the body", taken; the same
  script measured at the controller reads "away from the body" and drops the sword.
- *sprint and jump stop working, a restart helps, maybe the arms away from the chest*: the gesture
  block holds the engine's block key - the fist guard (fists readied, both hands no lower than 20 cm
  under the eyes and 12 cm ahead, `IsFistGuard`) or the bow guard (the bow upright; tester A's log:
  "the bow is held upright - blocking" 12 times). Blocking walks; whether it also refuses the jump I
  could not verify (UESP names a dodge jump while blocking at Journeyman Acrobatics). The "until a
  restart" part is unexplained - a guard that never let go, or a readied weapon he did not notice.
  His log would show "blocking". Cost: a toggle for the fist guard small; the cause of the latch
  needs his log. **Built 2026-10-18** for the log he will send: a tester's log of 2026-10-17 (OBSE
  22, the one with `[Performance]` on) read "bound mask 00" four times - every action of the set
  inactive on both hands, the sticks and buttons dead meanwhile, SteamVR giving the input to
  something else (its dashboard, an overlay, the headset put down) - and the one line for it was
  written on the change alone. `vr/ActionsWatch.h` (controller_actions_test) now counts the
  seconds and the log says "the right hand's actions have been inactive for N s" at one second
  and every ten after, and "are back after N s" - whether the sprint went with such a stretch
  will be readable. The fist guard's toggle stays open.


## Beta report of 2026-10-17 (a third tester, artemis, Quest 3; measured 2026-10-18)

The report: "1) A lot of fingers in some suits (looks funny) 2) Every time I get on a horse, I
end up hovering about two meters above it and start swaying from side to side. Furthermore,
while riding, if I turn my head to the left, the camera turns to the right, and vice versa.
3) During every vampire feeding, I just drift away. 4) Some outfits are not rendered in full
detail (though certainly not all of them)." Three clips and a picture of the inventory, no
log.

- *the fingers*: reproduced in the harness (`robe-hands.txt`, run 1): the Black Hand Robe
  (DBBlackHandRobe 000651D2, biped slots upper body, lower body, hands, feet - 0x3c - in one
  mesh; the Shrouded Armor, DBLeatherCuirass 000347F7, is cut the same) showed the body's
  gloved hand inside the first-person one with other fingers, the picture at "robe-closed".
  The cause, read off the body tree (Debug.BodyTreeProbe at the mark): the hand slot's model
  is a node "Hand  (<form>)" under "Player", which the body IK's hides match; such a suit builds
  no hand node - its hand geometry is a NiTriShape named plainly "Hand" under the node
  "UpperBody  (000651D2)" (the nif's shapes: five "UpperBody", "Arms", "Foot", "Hand" with 36
  bones), and the walk passed it by, two nodes hidden instead of three. Fixed: `BodyNodeHiddenOnArm`
  (BodyIkLogic.h, body_ik_test) hides a "Hand" under any slot model's node too; the scenario
  expects the hide line. Cost was small.
- *the horse*: reproduced in the harness (`horse-ride.txt`, run 5 of 2026-10-18, the mount by
  the crosshair on the horse and A - the engine's activation ray is the head's, and
  "0002008C.Activate player" from the console mounts nobody): the engine switches to third
  person at the mount (Enhanced Camera's source hooks that very switch, its POV02 site
  0x0066CEEE after the mount zoom at 0x0066CED2; the dismount 0x0066CF5D, the vampire feed
  0x0066CCBD) and sets the player's byte at +0x71C ("Sit Down, Mount Horse, Vampire Feed" in
  Enhanced Camera's reading; verified: 1 from the mount's switch, 0 again 116 frames later,
  when the ENGINE switches back to first person by itself). Full VR put the player back into
  first person one frame after the engine's switch (`HandModeForcesFirstPerson`), and the
  mounted first-person camera the engine then built stood 245 units over the horse against 118
  on foot - the picture at "mounted" looks down on the horse's ears from under the stable's
  roof, the tester's two metres - and swung 53 to 59 units sideways and back with the head and
  the horse still (the sway). Not the visible body: `horse-ride-nobody.txt` ([Body] Visible=0)
  read the same 245 and the same sway. Left to the engine (`horse-ride-third.txt`,
  ForceFirstPerson=0), its own first person after the animation stood 175 units over the horse
  and still. The turn's sense: the view's heading follows the head on the horse as on foot (a
  head 45 degrees left turned the view 45 degrees left in every run); what the tester saw as
  "turn left, the camera turns right" is the camera's PLACE moving - on a head turn the
  engine's camera dipped 110 units and swung 90 to 220 units off the horse, the world swinging
  the other way. Fixed (small): `HandModeForcesFirstPerson` takes the engine's animation (the
  bytes at +0x71C and +0x71D) and forces nothing while one plays, the engine switches back
  itself; the final run (`horse-ride.txt`, run 18) reads 175 over the horse, the camera riding
  on the horse's own position through its walk (no sway), the head turns right clean and the
  head turn left still dipping 64 units - the remainder is open: the engine's mounted
  first-person camera on a turned head, with the horse itself turning in that run (medium: the
  engine's mounted camera unread, REA on 0x0066C580's callers; the look-at gate tried and
  reverted, unproven). **Measured further 2026-10-18 afternoon:** not the body - the same ride
  with `[Body] Visible=0` (`horse-ride-nobody.txt`, with the fix) dips MORE: at the mark with the
  head 45 degrees left the engine's camera stood 52 units over the horse against 175 ahead, 80
  units off it sideways, and the horse itself had moved 110 units with no stick input; with the
  head 45 degrees right the horse's heading had turned 24 degrees (4.368 to 4.794), the camera 53
  over it; with the head ahead again 175 over it, on its position, whatever the heading. So the
  dip is the horse moving and turning while the head is turned, and the camera is the engine's
  own on the moving horse. Read with REA (Ghidra): the engine's camera update is
  `FUN_0066b710` (0x0066B710..0x0066C1FA). In first person it builds the camera's rotation from
  the player's heading (vtable +0x1E0) and pitch (0x004A9720), adds the first-person skeleton's
  own camera node's bob (the node at [0x00B3BB0C]: its local rotation's +0x3C and +0x40 taken as
  angles times pi/2, unless 0x005E6C10 says no), and takes the camera's POSITION from that node's
  world position (+0x88), blending it towards a second node [0x00B3BB10] by
  [0x00B36BD0]*t while a transition runs; third person and the vanity orbit are the other
  branch. The first-person skeleton's root is what OBVR turns and moves with the hands
  (game::PlaceFirstPersonArms, the root's rotation = the controller's relative rotation times
  the base), and its camera node hangs under that root - so on the horse the engine's camera
  position is wherever OBVR's arms root puts that node, and what steers the horse when the head
  turns is still unread (OBVR writes the player's yaw only for the aim and the walk steer,
  neither active; the mounted yaw at +0x61C stayed 0.0000 in every run). Next (medium): log the
  first-person camera node's world position against the arms root at the riding marks, and read
  the mounted steering - the sites that touch +0x61C at 0x0065DC10 (a save/restore through
  0x00B3BB7C), 0x00671CD0 (the per-frame add of the mouse's turn, clamped to +-pi/2 when the
  process state (vtable +0x36C) is 4) and 0x0066CAFD. The camera node stays under WorldRoot on the horse (the ancestry logged
  at the mount, `CameraLineageChanged`); "Camera: riding at the mark" logs the engine's own
  camera, the player's rotZ and rotX, the mounted yaw (+0x61C), the horse's heading, the bytes
  and the third-person zoom (300 throughout).
  **In the headset (Nadi, 2026-10-18 evening, the build of fc2de59):** the ride works - the
  third-person mount animation "maybe not quite centred", then first person on the horse.
  Two things seen on the horse, open: (a) the view slides a little to the left or right
  (the dip and drift measured above, the horse moving and turning under a turned head);
  (b) **no snap turn on the horse** - the right stick does nothing, only the left stick
  turns, and smoothly. Vanilla turns a mounted player's heading by the mouse through the
  mounted yaw at +0x61C (0x00671CD0, the per-frame add clamped to +-pi/2 while the process
  state is 4); OBVR's snap turn writes the player's yaw, which the horse's steering reads
  through another path. Next: the snap turn on the horse as a write of +0x61C, or of the
  horse's heading itself (medium, the mounted steering still unread).
- *the vampire feed*: not reproduced (a sleeping victim and vampirism are a scenario of their
  own). What the data says: the feed's idles are third-person animations only
  (`vampirefeedbedleft.kf`, `...right`, `...bedroll_crawl`, `...bedroll_side` under
  characters/_male/idleanims; nothing under _1stperson), and the engine forces third person for
  them the way it does for the mount (Enhanced Camera's POV04 site 0x0066CCBD after the feed
  zoom at 0x0066CCA0; it reads the feed from the actor virtual at +0x358, which xOBSE names
  HasVampireFed, and names the feed among the animations of the +0x71C byte). Full VR put the
  player back into first person at once, so the feed played with a first-person camera the
  engine had not built for it - the horse's 245 units, and "I just drift away" most likely the
  same camera carried by the feed animation. The mount's fix covers the feed on Enhanced Camera's
  word for the byte: no first person forced while the byte is set, the engine's third person for
  the animation's seconds with OBVR's third-person camera, the engine's own switch back after.
  Unverified for the feed itself: his next feeding, and its log ("Camera: the player's
  sit/mount bytes 0001 ... third person" at the feed, the switch back after), will say.
  **2026-10-18 night, his feed log (beta 4), in the Finery:** the Message menu's button
  clicked, the menu closed, then "Camera: switched to third person (frame 238)", "switched
  to first person (frame 239)" and "Hands: the player was in third person - put back into
  first" one frame later - beta 4 forcing first person a frame after the engine's own
  switch, exactly the mount's case. Beta 4 has no sit/mount line, so the byte itself is
  still unread for the feed; what beta 5 does there rests on the feed setting +0x71C as
  the mount does (Enhanced Camera's reading). His beta 5 feed log will say.
- *outfits not in full detail*, his inventory picture (a robe, the fists up, bare feet
  hanging under the hem with nothing between): not reproduced - the harness paperdoll with the
  Black Hand Robe and the Black Robe (`robe-hands.txt`, "robe-inventory", "black-inventory")
  shows a whole body. The fists up are vanilla's hand-to-hand stance for the paperdoll; the
  missing shins are not understood. Needs his item name, whether every time, and the log.
  **2026-10-18 evening, the item named:** "The Deceiver's Finery", the Dark Brotherhood's
  reward of the Bad Medicine quest - DBUpperShirt01 to 30 (0003489F..000348A5, levelled),
  BMDT 0x0c (upper body and lower body in one), Clothes\UpperClass\02\M\Shirt.NIF and
  F\Shirt.NIF. Read with pyffi: the male shapes are "Arms" (the arm bones to the hands),
  "UpperBody:0" and "UpperBody:2" (the trunk, with the thighs, calves and feet skinned into
  these two), "UpperBody"; the female "Arms", "Upperbody:0/1/2" the same way. No shape
  named "Hand", so the suit-hand hide is not what takes the shins; what does is open.
  **In the headset (Nadi, 2026-10-18 evening):** the Finery on, "nur 10 Finger" - one hand
  a side, the suit-hand hide (BodyNodeHiddenOnArm) confirmed on this outfit; the shins not
  looked at.
  `finery-paperdoll.txt` ran the same evening on a woman and on a male Altmer (the tester's
  race and sex): OBVR hides none of the shirt's shapes - every one reads flags 000E in the
  body tree at every mark, the inventory open as well, only the hand slot's node is hidden -
  so the gap in his picture is not a node hidden by the body IK. The harness pictures cut
  the paperdoll at the hips, so whether the shins are drawn is still unseen here. Asked of
  him: the same inventory picture with the body switched off ([Body] Visible=0, the row
  "Your own body" under Body) - the shins back means the body code, still gone means the
  engine or the mesh - and his log with `[Debug] BodyTreeProbe=1`.
- *the sun* (2026-10-18 evening: "daylight, especially the sun, is extremely bright ...
  looking directly at the sun is blinding"): new, unmeasured, nothing in OBVR touches the
  sun or the HDR; whether it is the launcher's HDR against Bloom, and whether 0.2.2 looked
  the same for him, is asked. Open.
  **2026-10-18 night:** solved on his side - "disable HDR or Bloom in options". Nothing of
  OBVR's; worth one line among the README's tips (small).
- *water reflections distorted* (the same message): known and open since the rendering
  checkpoint of 2026-09-20 (reflections missing and the body displaced in the right eye
  with live reflections on; docs/rendering-checkpoint-2026-09-20.md). Cost unestimated.
  **2026-10-18 night, his video (Reflections.mp4, 11 s, Lake Rumare from its shore towards
  the city):** one part of the water reflects the sky and the city, the rest is a flat teal,
  the two met by a straight seam that stands still in the world while the head turns, a
  dark or a bright streak fanning out along it. Read against the code: the stable capture
  (`[Render] WaterReflectionMode=2`, the default) renders the reflection once per stereo
  pair from a camera that faces the game's own heading - camera::GetHeadIndependentWaterCameraTransforms
  builds it on g_menuBaseRot, the rotation before the headset's - with the frustum three
  times the eye's across (about 76 degrees to each side) and 1.5 times up and down
  (render/WaterReflectionHook.cpp); each eye then samples that one picture through the
  capture's projective matrix (c13-c16). Water the eye sees past the capture's edge samples
  the edge column (the streak); past 90 degrees off the capture's axis the projection's w
  turns negative (the flat colour). In Full VR the heading does not turn with the head, so
  a look round at a lake brings the seam into the picture, and a steep look down at the
  near water runs out of the 1.5x rows the same way. **Reproduced 2026-10-19** in the lake
  under the Talos Bridge (`water-seam.txt`, `cow Tamriel 1 15`): the seam at yaw 225, the
  bridge tower's reflection as long streaks at 270 and 315; the control with
  WaterReflectionMode=0 (`water-seam-vanilla.txt`, the engine's own reflection from each
  eye's camera) shows none of it. Options: (A) the capture turned with the head's yaw each
  frame, its position left head-independent - the seam gone whichever way one looks; small
  to medium: one rotation in BuildHeadIndependentWaterCamera and its unit flows, a
  water-seam run; the risk is the engine's reset-on-move coming back with every head turn,
  which the head-independence was there to avoid. (B) WaterReflectionMode=0 as the
  default - no seam, each eye its own reflection, the engine's second reflection render per
  frame and its resets as the head moves; small, a default and the INI comment. (C) two
  captures, one per half space, blended at the seam; large. His quick way out, told him:
  `WaterReflectionMode=0` in OBVR.ini.
- *the torch's flame* (2026-10-18 night, his second video, 13 s): the flame over the held
  torch keeps one shape - neither the flipbook (Fire\FireTorchLarge.nif, which the engine
  hangs on the torch's FlameNode1: a NiFlipController over 20 textures a second on
  "FireTorchLarge:0") nor its Fire and Sparks particle systems move; the torch's own mesh
  (Lights\Torch02.NIF) carries only the static FlameCap. **Reproduced 2026-10-19**
  (`torch-flame.txt`): the same shape in four pictures two seconds apart - and the same with
  the hand mode off, with one render a frame (Stereo=none) and with the body and the
  POV-switch patch off, so neither the hands, the dual pass nor the body stop it. Not
  OBVR's node updates either: NiNode's downward pass (0x0070A0D0, vtable 0x00A7E38C slot
  0x60) runs UpdatePropertiesAndControllers (0x0047C930) only when Update's second argument
  is set, and OBVR's kUpdateNodeTransforms passes 0 everywhere (objdump 2026-10-19; xOBSE
  names the argument updateProperties). Whether a fire of the world animates under OBVR is
  unanswered (`fire-world.txt` and `fire-placed.txt` had the fire out of the picture,
  `fire-placed2.txt` was stopped before its pictures). Open. Next: a probe of the flame's
  controllers at the marks - class, the flags byte at +0x08 (bit 3 active), the last and
  scaled times at +0x20 and +0x28, the flip index at +0x50 - which says whether their time
  stands still or they were never started (small); then who calls Update(time, 1) on
  "Player1stPerson" and with what time, by the caller chain of 0x0047C930 (medium).
