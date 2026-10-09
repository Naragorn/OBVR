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
  `WeaponLetGoMetres` (0.30; "Weapon lets go at (m)"), `WeaponLetGoDegrees`
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
