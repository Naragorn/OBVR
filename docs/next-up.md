# Next up

Agreed with the tester on 2026-09-25 as the next things to build in Full VR.
Each entry says what is wanted, what is known, and what has to be found out
first. Nothing here is started.

## 1. Block only at 90 degrees to the attacker's blade

**Wanted.** A block with the weapon counts only when the player's blade meets
the attacker's roughly at a right angle. Default on, can be switched off.
Shields always block when raised (the left-hand gesture, unchanged).

**Known.**
- The weapon guard (`vr::IsWeaponGuard`) already decides when the raised
  weapon blocks: hand up and ahead, blade across the body, nearly level.
- The blade direction is the controller's forward axis, the one the strike by
  motion uses (`game::BladeInWorld`).

**To find out.**
- The attacker's blade direction during its swing: the NPC's `Weapon` bone
  world transform (the same node name the player's first-person skeleton has;
  the NPC's third-person skeleton not yet checked).
- Which NPC is attacking the player at that moment (its process's current
  action is an attack, `kActionAttack`, and its combat target is the player).
- Whether holding the block key only while the angle is right is enough, or
  whether the engine's "blocked" decision has to be hooked to veto a hit.
  Since 2026-09-26 the decision is known (docs/combat-comfort-spec.md, "How
  vanilla decides a block"):
  - A hit is blocked when the target is in the Block action and the attacker
    is within `fCombatHitConeAngle` of the target's body heading
    (0x006131D0).
  - That check is one call with a yes or no answer at 0x005FF83E, the place
    for a veto per blow.

## 2. Directional power attacks

**Wanted.** A heavy swing performs the power attack of its direction -
forward, backward, left, right, standing - chosen by the swing's direction
relative to the body, so the Blade, Blunt and Hand-to-Hand perks that hang on
them (knockdown, disarm, paralysis, ...) apply.

**Known.**
- In the game the direction comes from the movement key held while the power
  attack is made (UESP, Oblivion:Combat: "The attack you perform depends on
  which way you move as you attack").
- The strike by motion calls `AttackHandling` (0x005FEBF0) directly with the
  power-attack flag; no animation runs, so the direction the engine would
  read from the animation group does not exist. Whether the perks are applied
  on that path at all: not verified.

**Built 2026-09-29 (the tester: "die entsprechende richtung die man macht
können wir dann an oblivion übergeben").** Read: AttackHandling takes the
direction from the attacker's current attack group, the low byte of
AnimData+0x42 (GetAnimData at vtable +0x164; read at 0x005FF355 and
0x0060028E), and keys everything on it - the damage bonus by mastery
(0x00546BA0: standing Apprentice, sides Journeyman, back Expert, forward
Master; a group outside 0x16..0x1A is 1.0, so the strikes by motion so far
had no power bonus above Novice - derived), knockdown on backward and
paralysis on forward power attacks (0x006002C3..0x0060040B), disarm on the
sides (0x005FC090). OBVR now sets that byte for the strike and puts it back
after: 0x16 standing, 0x17 forward, 0x18 back, 0x19 left, 0x1A right for a
power swing, 0x14 (AttackLeft) for a light one. The direction is the
hand's way since the swing began, in the head's frame: mostly up or down is
standing, else the stronger of across and ahead (`ClassifyPowerSwing`,
hand_mode_test). The swing log names it ("a left power swing"). Not seen
in the headset: the perks' effects on a target.

**Headset (2026-09-29):**
- The tester saw a weapon fly away once, which is the sideways disarm.
- The log had power swings left, right and standing, never forward or back.
- The reason: `PowerSwingMetres` is 1.2 m of hand travel, and a thrust ahead
  or a pull back cannot reach that; an arm reaches 50–70 cm.
- The fix: forward and back now have their own length, `PowerThrustMetres`
  0.45 (`vr::PowerMetresFor`). The direction so far decides which length the
  running swing needs.
- **The blow's own direction (2026-09-29).** The tester asked for it:
  "powerattacks brauchen in full vr mode keine bewegungsrichtung mehr.
  stattdessen einfach die schlagrichtung". A pull back to the body was "dumm
  und nicht machbar in VR". The mapping (`vr::ClassifyPowerSwing`):
  - chop down: standing (the damage bonus);
  - strike up from below: the engine's backward one (knockdown, Expert);
  - slash across, left or right: sideways (disarm, Journeyman);
  - thrust ahead: forward (paralysis, Master), at the thrust's 0.45 m;
  - a pull back: standing.
  All five of the engine's power attack groups (0x16..0x1A) are reachable.

**To find out.** Where the engine keeps the current power attack's direction
(the process's animation group or a field beside the current action), and
where the perk effects are applied - so a motion strike can set the direction
before the hit, or apply the effect itself.

## 3. Fatigue for normal and power attacks

**Wanted.** Every strike by motion costs fatigue as a real attack does, a
power attack more.

**Known.** The strike by motion has no animation, so whatever the attack
animation charges is not charged. Not verified that nothing is charged.

**To find out.** The game settings behind the attack's fatigue cost (the
fFatigueAttack... family) and the call that applies it, so the same cost is
taken from the player's Fatigue actor value per strike.

## 4. A held object collides with the world

**Wanted (2026-09-26).** An object in the hand collides with other objects
while it is held, as it already does once it is thrown.

**Known.**
- A held object that is attached in the hand is placed every frame with
  `SetTranslationAndRotation` (bhk vtable +0xA0, inside the Havok lock), as
  a teleport, not as a motion. Havok does not resolve a teleport against what
  it lands in, so the object passes through others. This is derived from
  how it is placed. I have not verified whether Havok pushes the others aside
  or ignores them entirely.
- While held, the body also carries the player's collision group
  (`GrabPhysics`) until it is clear of the capsule; that group does not
  collide with the player.

**To find out.** Whether driving the body by velocity towards the hand
(setLinearVelocity, hkMotion vtable +0x54, with the same target point) keeps
the feel of the hand hold while letting Havok stop it at other objects, as
Alyx-style hands do. The price is a lag behind the hand when something is in
the way, and the throw has to come from that motion.

## 5. Thrown objects deal hand-to-hand damage

**Wanted (2026-09-26).** An object thrown at an NPC deals the player's
hand-to-hand damage, scaled by how hard it was thrown:
- up to a throw speed that can be set, the damage of a normal hand-to-hand
  strike, rising with the speed;
- above it, the damage of a power hand-to-hand strike.

**Known.**
- Vanilla hand-to-hand damage (UESP, Oblivion:The Complete Damage Formula):
  Health = fHandHealthMin + (fHandHealthMax − fHandHealthMin) ×
  (Strength/100 × fHandDamageStrengthMult) × (skill/100 × fHandDamageSkillMult),
  by default 1 + 10.5 × Strength/100 × skill/100. The skill is luck-modified.
  A power attack multiplies it by 2.5, or by 3 for a standing one from the
  Apprentice perk. It also damages fatigue by 1 + 0.5 × Health.
- The strike by motion already calls `AttackHandling` (0x005FEBF0) with an
  NPC as the target and a power-attack flag. That path computes the damage
  from what the player has equipped, so with a weapon drawn it would deal
  weapon damage, not hand-to-hand damage.

**To find out.**
- How to notice that a thrown object has hit an NPC:
  - a Havok contact between the thrown body and the NPC's character proxy;
  - or a test along the flight, as the strike by motion does along the blade.
- Whether `AttackHandling` can be made to take hand to hand for this one
  call, or whether the damage should be computed from the formula above and
  applied directly, with the hit reaction, crime and aggression the engine's
  own hit brings.

## 6. Locomotion: room-scale body, teleport

**Scope: Full VR only** (the hand-tracked mode, `[Hands] Enabled`). The seated
mode stays exactly as it is:
- played with a gamepad or keyboard, VR on top, Luke Ross style;
- aiming follows the head;
- the character does not turn with the head;
- the head moves only the camera.

Every change below is switched by the mode.

**Wanted (2026-09-27).**
- Walking and turning in the playspace moves and turns the character the same
  way, in all six degrees of freedom.
- The stick walking and turning stay (snap or smooth).
- A teleport as in Half-Life: Alyx and Gunman Contracts - Stand Alone.
- The onboarding and the settings choose between instant and fast.

**Built.**
- Head tracking in 6DoF moves only the camera.
  - The offset is taken from the recentered reference.
  - It is clamped by `MaxLeanUnits` (120 units, about 1.7 m) about the game
    camera.
  - The character's capsule stays where it is, so a step through the room is
    a lean, and nothing stops the view at a wall.
- The character does not turn with the head. That was the tester's decision
  for the seated mode ("character bleibt", docs/vr-modding/
  camera-tracking-and-aiming.md), since kept by aiming in place
  (`AimAtSource`). It stays in force there. In Full VR the body follows the
  playspace instead.
- The left stick walks along the body's heading (rotZ). The right stick turns:
  smooth at `TurnSpeed`, or snap (`SnapTurning`, instant or eased,
  vignette).
- Nothing of room-scale or teleport:
  - A first attempt (September 23) replaced the head offset, used a
    tracking-space point as a world position for its teleport, and turned
    without booking the turn.
  - It was taken out in 3aaf4a0 and deleted in 0f88c21.

**To build.**
1. **The capsule follows the head.**
   - The horizontal part of the head's motion becomes the character's motion,
     through the character controller, so walls and slopes stop it as they
     stop walking.
   - The head offset keeps only what the capsule could not follow (a lean
     over a table, a head through a wall shows the wall).
   - To find out: how to move the player's controller by a given distance in
     a frame. Options are the proxy's velocity, the engine's own move, or a
     position write followed by the controller's update.
2. **The body turns with the head.**
   - rotZ follows the head's yaw, with a dead zone or while walking, so a
     glance over the shoulder does not spin the body.
   - The turn has to be booked like the snap turn's, into the reference, or
     it feeds back.
   - Aiming already works from the gaze, so it is not affected.
3. **Walking direction** for the stick as a setting: the head, the left hand
   (Alyx's Continuous / Continuous Hand), or the body.
4. **Height.** Crouching in the room could sneak, as a setting. The head's
   height already moves the camera.
5. **Teleport: built 2026-09-27, not yet seen in a headset.** See the next
   section.

### The teleport as built

- **Controls.**
  - Right stick pushed up, past `TeleportStickStart` (0.8) and within
    `TeleportStickConeDegrees` (30) of straight up.
    - **Flicked and let go** before `TeleportHoldSeconds` (0.2): a jump.
    - **Held** that long: it aims - an arc from the right hand's laser and a
      ring where it lands. Let back under `TeleportStickRelease` (0.3), it
      goes.
    - The jump comes on the release, a fraction later than a key would.
    - With the teleport switched off, the push jumps at once.
  - A right grip while aiming cancels.
  - For a quarter second after the release the stick does not turn.
  - The history (2026-09-27): first jump on the left A, then on the right A;
    the tester found both awkward (the left thumb walks, the right A was
    activate). Skyrim VR puts the jump on the right stick up, in the standard
    Index layout and in VRIK's (reddit r/ValveIndex f1nqvd, r/skyrimvr
    clsav7), so a flick up jumps here too and a hold teleports. Skyrim VR's
    own teleport replaces walking (`bDirectMovementWithWands`, one or the
    other, UESP Skyrim:Skyrim VR); here both work together.
  - Right A activates again. The left A drops the item under the cursor in
    the inventory (vanilla's Shift + click, help.bethesda.net answer 9938)
    and does nothing in the world - it never takes an object, the grips do
    that.
  - Logic: `vr::StepTeleportStick`.
- **Arc and ring.**
  - The arc is a throw at `sqrt(g R)`, walked in 31 segments. Each segment is
    tested against the world with Havok's pick (`game::PickWorldSegment`,
    layer 31 `OL_DROPPING_PICK`, the player's group excluded).
  - Both are SteamVR overlays in the reach ring's light brown
    (`render::TeleportArcLayer`), grey and faint when the landing is refused.
- **What a landing may be** (`vr::JudgeLanding`):
  - ground no steeper than about 45 degrees;
  - within the range (4 m, 5 % slack);
  - without Blink: no higher than the player's jump apex (the controller's
    own), no further down than the range, and a clear straight line at waist
    height to it;
  - enough fatigue.
- **The price.**
  - The dodge roll's, which is the jump's: vanilla's formula read from the
    game, 30, or 15 from Acrobatics Expert.
  - Times `TeleportFatigueMult`, plus with Blink `TeleportBlinkFatiguePerMetreUp`
    per metre climbed.
  - Paid through the engine's own fatigue call before the move.
  - Too little fatigue locks the teleport until there is enough; nobody
    collapses.
  - Confirmed in the headset on 2026-09-27: 30 a teleport, locked when short.
  - **Why the dodge's price** (decided with the tester on 2026-09-27):
    - the default teleport is a dodge step, and the game already prices one;
    - nothing new to learn or tune;
    - mods that change the jump's or dodge's cost change the teleport's too;
    - Acrobatics Expert halves it, as it halves the jump.
  - Where vanilla ends: the jump formula (`fFatigueJumpBase` 30 +
    `fFatigueJumpMult` x load, x `fPerkJumpFatigueExpertMult` 0.5 from
    Expert) is read in the exe (0x00672AEF). That the dodge roll pays it is
    read there too: its action (0x005F5050) has no fatigue code of its own.
  - **The alternative, not built** (proposed first, set aside for the above;
    kept in case the vanilla price feels too cheap or too flat):

    ```
    cost = (Base + PerMetre x distance + PerMetreUp x rise)
           x (1 + Load x weight / max weight)
           x (0.5 from Acrobatics Expert, else 1)
    ```

    - Base 30, a jump.
    - PerMetre 7.5: a full 4 m costs 60, two jumps or a power attack
      ((7 + 0.1 x weapon weight) x 5, cs.uesp.net Fatigue Game Settings).
    - PerMetreUp 10, climbing only.
    - Load 0.5: fully loaded costs half as much again.
    - Against a starting maximum of 150 to 200 (Endurance + Strength +
      Agility + Willpower, UESP Oblivion:Fatigue) and 10 a second back, that is
      two or three full teleports in a row, one back in about 6 s. The vanilla
      price gives five or six.
    - The multiplier (`TeleportFatigueMult`) already covers "more expensive";
      the distance term is what the alternative would add.
- **The move.**
  - Glide (default): the player placed along the line every frame at
    `TeleportGlideSpeed` (15 m/s, 5 to 40), by the same sequence the SetPos
    command runs (`game::PlacePlayerAt`).
  - Instant: the compositor's FadeToColor to black, placed, and back
    (`TeleportFadeSeconds` each way); with `TeleportInstantFade=0` no black
    at all, there in one frame.
  - A menu opening mid-move ends the move at once at its target (see the
    stuck message box below).
  - Untouchable from the commit to the end through the console's god mode
    flag, and restored to what it was.
  - The vignette while gliding (`TeleportVignette`).
- **When it is allowed.**
  - First person, in the world, no menu, alive, not riding.
  - In combat only with `TeleportInCombat`.
- **Settings.**
  - Settings menu, category Teleport; INI `[Locomotion]`.
  - The onboarding page "Teleport" sets on/off, the mode, the range, combat,
    the vignette and Blink.
- **Tests.** `teleport_test` (stick, arc, landing, price, glide and fade
  flows, overlay geometry), `hand_mode_test` (a flick jumps, a hold aims and
  goes, a grip cancels), `onboarding_test` (the page).
- **Not verified, to watch in the headset:**
  - whether layer 31 hits ground, statics and clutter as its name suggests;
  - whether the glide's per-frame placement feels smooth or stutters, and
    whether the capsule keeps any fall speed at the end;
  - whether god mode also covers spells and traps (weapons and falls go
    through the path it blocks);
  - the pick's normal (if the ring lies wrong, that is where to look).
- **`TeleportMakesNoise` (was `TeleportQuietWhenSneaking`, 2026-09-27).**
  - The engine hears a moving player through the movement flags, which
    HandleInput rebuilds every frame (0x00671620, written through HighProcess
    vtable +0x2C8, 0x00631B50). A placement sets none, so by default a
    teleport makes no movement noise at all, sneaking or not.
  - With the option on (the tester: "soll es Laerm machen wie laufen"), the
    setter slot goes through OBVR and adds walk-forward (0x0101) to the
    player's flags for the move, at least 0.5 s. Sneaking stays sneaking.
  - Harness: `teleport-noise.txt` PASS, "the movement flags read 0101".
    Whether an NPC then notices the player has not been tested.
  - **Fixed (the tester, 2026-09-28): the extra step after the landing.**
    "kommt der teleport aber dann noch ein zusätzlicher schritt des chars
    nach vorne ... das erzeugt nausea". The same flags move the player, so
    for the half second of noise after the landing it walked on by itself.
    Measured with the new drift line (`Teleport: a second after the landing
    the player stands ...`): 21.7 units across the ground with the setter
    hook, 0.0 now. Now only the detection's two reads (the getter at
    vtable +0x2C0, 0x006285A0, from 0x005F66DD and 0x005F66FD) answer
    walk-forward; the flags themselves stay as the controls set them
    (game/TeleportNoise.h, teleport_noise_test). Whether an NPC then hears
    it is still not tested: the harness cell has no one to hear.
  - **Headset (Nadi, 2026-09-29): no extra step any more.** Next wish:
    "fügen wir nun einen sound hinzu. whl den von der rolle". Oblivion.esm
    has `FSTDodge` (000CBA79, fx\fst\dodge\fst_dodge.wav) and
    `FSTDodgeBackward` (000CBA7A); other candidates are the landing sounds
    per ground (`FootSound*Land`, 00000219-0000021E).
  - **Built (2026-09-29): `[Locomotion] TeleportSound`**, the tester: "wir
    bieten in den settings alle 4 an. mit default auf fstdodge". With
    TeleportMakesNoise on, the landing plays `dodge` (default),
    `dodgeback`, `landing` (the engine's own landing after a jump,
    0x006B1900, picked by the ground under the player) or `swish`
    (`WPNSwishHand`, 00088834). Settings: Teleport, "Sound". The calls
    are the script command PlaySound's (game/GameSound.h,
    teleport_sound_test). Harness: `teleport-noise.txt` (dodge) and
    `teleport-sound-landing.txt` PASS, both "played". Not heard yet: the
    harness cannot listen, and `dodgeback`/`swish` ran only as the same
    call with another form.
  - **Headset (the tester, 2026-09-29):** "swish macht keinen sound,
    setzen wir als default: landing". Cause: the swish first played was
    `WPNSwishHandX` (0000C3D1), whose file lies under
    `fx\wpn\360lofi\` - the PC's `Oblivion - Sounds.bsa` has no
    `360lofi` folder (its names searched), so the game made the sound and
    played nothing, and OBVR logged "played". Now `WPNSwishHand`
    (00088834, `fx\wpn\swish\hand\`, whose `wpn_swishhand_01..03.wav` the
    BSA has). The default is `landing` now. "played" in the log says the
    engine made the sound, not that its file exists.
- **An in-game test** is feasible like the water test: a `[Debug]` switch
  that feeds a scripted right controller into the hand mode and logs `VRTEST`
  lines for position, fatigue and refusals. Not built.
- **The stuck message box (2026-09-27, first headset run).**
  - A game message (the Generic menu, 0x3F3) opened about five log lines
    after a glide began, and the trigger could not click it away. The game
    had to be ended.
  - Read in OBVR.log.prev: the laser hit the box and the trigger was pulled
    three times; whether a click was sent is not in the log.
  - Every other message box that session closed with a click.
  - The only thing that differed: a teleport was mid-move. Its move paused
    under the menu and the god mode flag stayed on.
  - The cause is not proven. Since then a menu that opens mid-move ends the
    move at once at its target and drops the god mode, and every click the
    laser sends into a game menu is logged ("Hands: click sent to the ...
    menu"), so the next occurrence shows whether the click left OBVR.
- **Floating corpse (2026-09-27).**
  - The body hovered for a few seconds after death before it lay on the
    ground.
  - The log shows the third-person head and spine visuals switching on at
    the death ("Bip01 Head follows the HMD after animation"), so the ragdoll's
    head and spine were turned towards the headset every frame.
  - They are now off while the player is dead.
  - That they held the ragdoll up is a hypothesis; vanilla slow ragdolls are
    the other candidate (combat-comfort-spec.md, Death).
  - Still floating after that change (tester, 2026-09-27). So the visuals were
    not the cause, and the vanilla ragdoll is what is left.
  - Modern Engine Fixes (nexusmods.com/oblivion/mods/56116) is installed here
    and its list of fixes has nothing on ragdolls; neither has EngineBugFixes
    (mods/47085; its SavedHavokDataFix is about saving ragdoll data).
  - What names the symptom: Ragdolls for Oblivion ("corpses no longer fall
    slowly", reshaped collisions, mass, friction and constraints on every
    skeleton; nexusmods.com/oblivion/mods/51844, described on
    ggmods.com/game/the-elder-scrolls-iv-oblivion/mod/50), and Duke Patrick's
    Melee Combat ("dead bodies ... instead of floating down thru the air like
    a feather"). Both treat it as vanilla behaviour, fixed in the skeletons'
    data, not in the engine. Not installed here.

**Buttons in the hand-tracked mode now** (`PlanHandControls`):
- Right hand:
  - trigger: attack;
  - A: activate;
  - menu button: escape;
  - stick: x turns, flicked up jumps, held up teleports, flicked down sneaks,
    the click readies the weapon.
- Left hand:
  - trigger: cast;
  - A: drop the item under the cursor in the inventory; nothing in the world;
  - menu button: OBVR's menu;
  - stick: walks; held in, it runs;
  - trackpad click: the quick menu.
- Either grip grabs.
- In Alyx the teleport is on the right stick pushed forward, and players
  report teleporting by accident when turning (reddit.com/r/ValveIndex/
  comments/ikzuca). That is why there is a cone and a threshold.

## Beta tester round (2026-10-03), built 2026-10-05

A beta tester's first notes; the tester decided what to build. Built and
checked by tests and the harness as far as said; **none seen in the headset**.

1. **Which item a grip takes, and showing it.** Before several items the
   nearest in reach was taken even when the laser pointed at another
   (`game::ConsiderNearItem` ranked by distance only). Now by class
   (`game::PickRank`, frame_logic_test): touched (5 cm), then the laser on
   it (6 degrees off its bound), then within the grab's reach, then in the
   laser's cone by how far off, then the palm. The game's info text (name,
   action, value, weight - HUDInfoMenu) is a hand-HUD element of its own,
   `[HandHud] InfoPlace=target` by default: hung just under what the
   crosshair is on, facing the eyes, its size kept as at the view distance
   (`vr::TargetRowPose`, `vr::TargetHangPoint`, hand_hud_test). The item
   itself is marked by the engine's effect shaders on the reference
   (`game/TargetMarker.h`, target_marker_test): an outline (`TargetOutline=1`,
   effectFortify 000562C8) and a glow (`TargetGlow=0`, effectTelekinesis
   00181C2E). Harness `stow`: "Target marker: ... marked (outline 1)",
   "Info target, lifted". How either looks: not seen.
2. **The inventory's figure hollow.** The interface pass drew it into
   OBVR's single-sample layer with the game's 8-sample depth-stencil, which
   DXVK leaves out of the framebuffer - no depth test, skin over armour. The
   pass now gets a single-sample depth-stencil of its own while redirected
   (`render/HudDepth.h`, hud_depth_test). Log: "Hud depth: own single-sample
   depth-stencil 4028x3380 made". The figure itself: not seen.
3. **Pulling from afar.** `PullReachMetres` and `ReachMarkerMetres` default
   2.0 (up to 3): pointing at an item shows the ring and the grip pulls it.
   A short pulse on that controller when the held thing reaches the hand
   (`game::TakeHeldObjectArrival`; the manifest's new vibration actions,
   IVRInput entry 23 TriggerHapticVibrationAction), none before. The harness
   has no real controller: "no pulse (SteamVR refused it ...)" there.
   **The tester's live INI still says 1.0 for both** - set them in the menu.
4. **"You cannot change weapons while attacking."** The inventory, the
   hotkeys (the quick ring), unequipping and dropping refuse while the
   player's action is anything but -1; in Full VR the animations are not
   seen, so those four `je` become `jmp` while Full VR is on
   (`game/EquipWhileActing.h`, equip_while_acting_test). Log: "Equip: 4 of 4
   action gates found". Why the action was not -1 when the tester equipped
   is not known; a log of the action at each refusal would say.
5. **A ray from the hand in the Arena raiment.** Best explanation found:
   the female Arena raiment's glove ("Hand:0") has elbow vertices weighted
   half to the upper arm, which in Full VR stays with the animation while the
   forearm follows the controller. Gloves' upper-arm bones now collapse into
   the pinned elbow (`game::StepGloveElbows`, arm_stump_test). Whether the
   tester's character was female is not known; not reproduced.
6. **Thrown through the air.** `[Look] NoPlayerKnockdown=1` (menu "No
   knockdown"): the knockdown (0x00654420, the process vtable +0x2F0, from a
   hit's knockdown roll, a magic explosion, PushActorAway) skips the player.
   Log: "the high process's knockdown rerouted". Not seen in a fight.
7. **Stuck at doors.** No log, no tester to ask. Open: walk through a door
   in the harness with `BodyCollision` on and off; if only on, the hand and
   weapon bodies catch the frame.
8. **The laser in the world.** `[Hands] LaserInWorld=0` by default (menu
   "Laser in the world"): the weapon hand's beam, ending at what the pick
   hit, else `WorldLaserMetres` (2). hand_mode_test.

Also found on the way: harness `stow` fails at "held item at the body" - it
failed the same way on 2026-09-28, before this round.

## Also open

- **The dual pass was mono without the game's antialiasing (found and
  fixed 2026-10-06, `game/SceneFrame.h`).** With `iMultiSample=0` both eyes
  carried the same picture: the game renders the world into a frame-sized
  texture and copies it into the back buffer once a frame (the image-space
  copy shader, gated on the renderer's frame state), so the second pass's
  picture never reached the back buffer OBVR captures from. OBVR now closes
  the frame between the passes (group pop, EndFrame, state 0), so the second
  pass copies as the first did; a second pass whose picture still is not in
  the back buffer is not captured and the log says so. Measured with
  `tools/disparity.ps1` on `stereo-view`'s eye dumps: 0 samples before -56 px
  in every band (the crop offset), after -104 (ceiling) to -59 (floor); 8
  samples -104 to -59 as well, unchanged. The player's INI losing its 8
  between two sessions on 2026-10-06 is still unexplained (the launcher's
  autodetect is the suspect).
- **Shield bash.** Not a vanilla action (UESP, Oblivion:Block: the Expert
  and Master perks give a chance of a stagger or disarm on a block; a manual
  bash exists only in Oblivion Remastered). A VR bash would be OBVR's own:
  the shield hand driven into an NPC, the engine's knockback applied.
- **New games.** The walkthrough and the hands' guide around the intro and
  the character creation: not yet tested.
- **Block with the fists and the bow (test more later; the tester,
  2026-10-01: "bisher keinen block gehabt ... Bogen: Hat manchmal geblockt.
  aber auch hier unklar wie oft das gehen soll").** The tester's log of that
  evening (until 21:09): both guards reach the engine - 36 times each the
  player's action went to 6 (blocking), and the engine's own block test
  counted at least 8 blows on the fists and 4 on the bow as blocked
  ("Block: a blow at the player's block ... (blocked)"; that line stops
  after 12, so later blocks went unlogged). What to look at:
  - **Fists against weapons do nothing in vanilla** (UESP Oblivion:Block:
    "Hand to hand blocks 0.25% damage per skill level, but only against
    unarmed opponents"; Novice: "hand-to-hand blocking against weapons has no
    effect"), and with the arms hidden there is no feedback either - likely
    why no block was felt. A decision for the tester: vanilla plus feedback
    (a sound or a pulse on a blocked blow), or OBVR's own reduction against
    weapons (not vanilla).
  - **Both guards flicker.** About half the blocks end within one to five log
    lines (11471-11550: on, off, on, off); a blow counts only when it lands
    in an "on". Suspected, not measured: the thresholds sit where the hands
    are held - the guard's hand no lower than 20 cm below the eyes (a bow
    held at the chest is 30-40 cm below), the bow within about 37 degrees of
    upright, and any small move read as a swing ends the fists' guard.
  - **First measure:** each guard's on and off with its values (the hand's
    height and reach, the bow's lean, a swing seen), each blow at the player
    with blocking or not, the attacker's weapon and the damage taken, and no
    cap on the block line. **Then** an on-threshold and a looser
    off-threshold with a short least hold (about 0.3 s), and for the bow
    perhaps the chest's height and 45 degrees.
- **Stagger in combat (reminder to test).** `[Look] NoPlayerStagger` (on by
  default) skips the player's stagger (0x005F4FD0, both call sites) and a
  hit's knockback (the character-proxy fetch at 0x0060008A). To test in the
  headset: fights with power attacks, with the option on (no lurch) and off
  (vanilla stagger back); whether any lurch remains from another path (the
  recoil 0x005F4F00 when the player's own attack is blocked is not touched,
  nor a knockdown).
- **Holding objects: closed for now (2026-09-26).** In the hand as it lay,
  pulled from up to a metre, floating in, the ring moving to the near side,
  thrown with SteamVR's velocity, NPCs looking at the eyes - all confirmed in
  the headset (docs/holding-objects-spec.md, tests one to ten). Still open
  there: 3 (rotation for large objects), 5 (grips by size), the eat and stow
  gestures, and items just across a cell border.
- **A player-only death force.** The killing blow can fling the player's
  ragdoll (vanilla fDeathForceForceMax); where the force is applied is not
  yet found (docs/combat-comfort-spec.md).
- **Hands a few centimetres low.** The tester suspects the tracking; a
  comparison with the SteamVR system menu's controllers would settle it.
- **Controls.** docs/controls-spec.md holds the layout, why, and the target
  picture (weapons drawn by reaching for them, fists, the quick menu).
- **Block and stagger.** docs/combat-comfort-spec.md holds the problems and
  every proposal.
- **Death view.** Held at the living eyes with the body drawn
  `DeathBodyAheadMetres` (0.5) ahead and `DeathBodyUpMetres` up, the HUD
  hidden; the tester: done for now. Not built: a fade-in of the body, a fade
  to the load.
- **Thumbs up (bug, for later).** The thumb now follows the full skeleton
  joint by joint (docs/holding-objects-spec.md, part 1b), and the tester
  finds it OK (2026-09-28). A clear thumbs up is still missing. The game's
  hand blends only between its open hand and its fist, and a turn of the
  thumb that is on neither is dropped. The tester's log of 2026-09-28 shows
  why a thumbs up does not come: with the controller's range of motion the
  thumb at its most open (not touching anything, the summary curl 0.01 to
  0.13) still reads 0.43 0.00 0.25 base to tip. Its base never comes out of
  the grip, and it rises to about 0.40 0.40 0.48 at a curl of 0.9. To look
  at:
  - a third pose to blend towards (a thumb straight up), from another
    animation or made by hand;
  - SteamVR's range without the controller
    (`VRSkeletalMotionRange_WithoutController`) in place of the one with it;
  - another reference skeleton to measure against.
- **Looking into the hand from below (sealed 2026-10-01, see
  `docs/holding-objects-spec.md`).** The tester, on
  2026-09-28: "man kann immer noch von unten in die hand schauen". The bare
  hand's lid is the hand's own far wall, drawn flat. What lies between the
  opening and that wall shows through it: the bow's grip, and the inside of
  the fist seen from below.
  - A lid only in the opening, pulled nearer by a stencil pass, was tried on
    2026-09-28 and reverted at the tester's request.
  - Its measurement stands: the hands are drawn with a nearer near plane
    than the world camera's 10 units (a wrist at 9 to 13 units was fully
    drawn), so the world frustum's planes do not convert a hand's distance
    into the depth buffer.
  - The first-person pass's own projection would be needed first.
