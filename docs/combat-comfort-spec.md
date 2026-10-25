# Combat in Full VR: block and stagger

Status: **partly built.** Written 2026-09-26 from the tester's reports and the
disassembly of Oblivion.exe 1.2.0.416. What is built says so; everything
else is a proposal. Addresses are this binary's.

## 1. Blocking

### How vanilla decides a block (researched 2026-09-26)

Read in the hit handler 0x005FEBF0 and its helpers, checked against the wikis.
What is read in code says so; names and meanings marked "inferred" are not
confirmed.

**When a hit counts as blocked.** All four must hold:

1. The target is not paralysed (vtable +0x1A0 = 0x005E17E0, actor value
   0x30).
2. The hit is not a Master-Sneak sneak attack (Sneak mastery 4, the flag that
   also ignores armour; UESP, Oblivion:The Complete Damage Formula).
3. The target's process action is Block, 6 (0x005E5670: action via process
   vtable +0x2D0 `== 6`). This is the same state xOBSE's `IsBlocking` reads
   (xOBSE Commands_AI.cpp). Nothing about timing or pose is checked. Whoever
   is in the Block action when the blow resolves blocks it.
4. **The attacker is in front** (0x006131D0, called at 0x005FF83E only when
   1 to 3 hold):
   - It takes the heading of the line from target to attacker, minus the
     target's own heading (vtable +0x1E0), in degrees, folded into 0..180.
   - The block counts only if that angle is at most `fCombatHitConeAngle`
     (the GMST's value at 0x00B36F28, named by its constructor call; default
     35 per the Dynamic Oblivion Combat description, nexusmods.com/oblivion/
     mods/49873).
   - This confirms the players' "only from the front" (reddit.com/r/oblivion/
     comments/1p0e2cv), which was only anecdotal until now.
   - For the player, the heading is the **body's** heading, not the
     headset's.

**How much it stops** (0x005474A0; GMST names from their constructor calls):

- blocked = min(fBlockMax, (base + mult × luck-modified Block skill / 100)
  × item × fatigue factor).
- item is 1 with a shield, `fBlockAmountWeaponMult` (0.5) with a weapon and
  no shield, and `fBlockAmountHandToHandMult` with neither.
- The two constants at 0x00B36EE8 and 0x00B36EF0 are inferred to be
  `fBlockSkillBase` (0) and `fBlockSkillMult` (1); the names were not found
  by the constructor search.
- `fBlockMax` defaults to 0.75 (cs.uesp.net/wiki/FBlockMax).
- Hand to hand stops nothing against an armed attacker (0x005FF885). This
  matches UESP, Oblivion:Block.
- The damage is scaled by (1 − blocked). What is left then goes through armour
  as usual.

**What a blocked hit sets off** (from 0x005FFEEA when the blocked share is
above 0):

- The blocker plays a block reaction (0x005F4E10). The anim group is 0x1C, or
  0x1D for a counterattack; these are inferred to be BlockHit and
  BlockAttack, from the order next to 0x1E Recoil and 0x1F Stagger.
- **Counterattack roll** (0x005F3C30), for melee only:
  - With a shield at Block Expert or better, or empty-handed at Hand to Hand
    Expert or better.
  - Chance `iPerkBlockStaggerChance` (0x00B37238; 25 per UESP).
- **Disarm roll** (0x005FC2B0):
  - With a shield at Block Master, or at Hand to Hand Master.
  - Chance `iPerkBlockDisarmChance` (0x00B37230; 5 per UESP).
- **The attacker recoils** (0x005F4F00 at 0x00600565, anim 0x1E, action 7):
  - after every blocked melee blow without a counterattack;
  - after a blocked ranged hit only when the disarm roll succeeded.
  - This is the recoil CS wiki's Combat Style page means ("An actor is
    recoiling when their strike is blocked").
- **Hand-to-hand block recoil**: an empty-handed block against an unarmed
  attacker makes the attacker recoil (0x005FFF4D) in either of two cases:
  - the blocker is at Hand to Hand Journeyman or better;
  - the blocker is at Block Journeyman or better and wins
    `iPerkHandToHandBlockRecoilChance` (0x00B37250).
- **For the player as the blocker**: 0x007EB010(1) is called (0x00600574).
  This is the start of the hit blur (GethitShader), with the blocked offset
  and a strength of 0.3. The earlier guess that it was the gamepad rumble was
  wrong. See `src/game/HitShader.h`. `[Look] NoHitBlur` (since 2026-09-29)
  skips it, and its sibling start 0x007EB080 as well.
- Blocking costs fatigue and wears the shield or weapon down below the
  Apprentice and Journeyman perks (UESP, Oblivion:Block; the fatigue formula
  on cs.uesp.net/wiki/FFatigueBlockBase). The code for these was not read.

**What an unsuccessful block is.** Vanilla has no failed block as an event.
Either the hit meets the four conditions and is reduced, or it lands in full:
not in the Block action, attacker outside the cone, paralysed, or a hand block
against a weapon. A blocked blow is never fully stopped: at most `fBlockMax`
(75 %).

**Consequences for OBVR:**

- The player's block already has a direction: 35° either side of the body's
  heading. With the walking direction decoupled from the view, the body may
  not face where the player looks. A guard raised towards an attacker at the
  side can still fail.
  - The 90-degree blade check (proposal A) is a second, stricter condition on
    top of this cone - built 2026-10-14 as the block's angle rule
    (`[Hands] BlockByAngle`, `game/BlockAngleLogic.h`, docs/next-up.md 1):
    the weapon blocks a blade crossing it within 30 degrees of a right angle,
    the shield a blow its face is turned to within 70; the fists and the bow
    as before. Since the same night the blades have to meet as well: their
    blade within `BlockReachUnits` (40) of the player's blade or shield at
    the blow, so a guard held high does not block a cut at the belly (the
    tester, in the headset: the height of the crossing blades did not
    matter).
  - The veto sits at that place: 0x006131D0 is one call with one boolean
    result (0x005FF83E), rerouted by `game/BlockCone` since the gaze cone,
    and the angle rule answers after the engine's own yes.
- NPCs blocking OBVR's motion strikes go through the same code. A strike into
  an NPC that is in the Block action and facing the player makes the
  **player** recoil (0x005F4F00 on the attacker). This is the open recoil
  point under Stagger.
- The counterattack and disarm need the perks, so a low-skill player sees
  neither.

**Mods that change it** (for comparison, none installed here):

- Timed block: Deadly Reflex; Dynamic Oblivion Combat (mods/49873): a block
  raised in the last second stops everything, a block held too long stops
  nothing.
- All-or-nothing blocks: Better Blocking (mods/27947).
- Recoil and stagger by a score: Doc Block Recoil Stagger (mods/35933).
- All of them work by script, polling `IsBlocking` and patching skill or
  health afterwards. None hooks the decision itself.

### What is built

- **Shield or left hand raised** (`vr::IsBlockGesture`): the left hand at
  least `BlockMinUp` above and `BlockMinForward` ahead of the eyes holds the
  game's block control. A raised shield always blocks. That is vanilla's
  block, so the engine decides what it stops and what it costs.
- **Weapon guard** (`vr::IsWeaponGuard`, since 64e0d5d): the weapon hand up
  (no more than 20 cm below the eyes) and ahead, the blade held across the
  body (at least 0.7 sideways, at most 0.5 up or down), not swinging, with
  the weapon drawn (fists included). It holds the same block control.
- The guard reads the blade as the controller's forward axis, the same axis
  the strike by motion runs along.
- **The block looks along the view** (`[Look] BlockFacesView`, on by default,
  since 2026-09-29; `game/BlockCone.h`).
  - The problem: the tester reported "block mit schild lässt ein paar
    attacken noch durch". The engine's cone takes the **body's** heading, and
    OBVR keeps the body apart from the view, so an attacker the headset faced
    could stand outside it.
  - The fix: the call to the cone check at 0x005FF83E is rerouted. For the
    player, the check runs with rotZ set to the gaze's heading, then puts it
    back.
  - It logs both angles ("Block: a blow at the player's block - N degrees off
    the view ..., M off the body ...") for the first twelve blows.
  - A blocked blow still takes up to 75 % off, never all (`fBlockMax`). Some
    damage coming through a good block is vanilla.
  - Not seen in the headset yet.

### Problems

1. **The angle to the attacker's blade is not checked.** Any guard pose
   blocks every blow, from any side. The tester wants a block to count only
   when the blade meets the attacker's roughly at 90 degrees (default on,
   can be switched off). Shields keep blocking whenever they are raised.
2. **The block is a held control, not an event.** OBVR holds the block key
   while the pose is right; the engine then treats the player as blocking
   for any hit in that time. There is no veto per blow.
3. **The guard is a fixed set of thresholds.** A high guard over the head,
   or a blade held diagonally, is not recognised. A lowered blade held across
   the hips is refused on purpose (the height floor), so resting the sword
   there does not block by accident.
4. **No shield bash.** Oblivion has none (UESP, Oblivion:Block: the Expert
   and Master perks only give a chance to stagger or disarm on a block; a
   manual bash exists only in Oblivion Remastered). A driven shield does
   nothing.

### Proposals

- **A. 90-degree block by the attacker's blade** (docs/next-up.md, item 1).
  - Find the attacker: an NPC whose process action is an attack and whose
    combat target is the player.
  - Read its blade: the world transform of the `Weapon` node in its
    third-person skeleton (not yet confirmed there).
  - Compare the angle between the two blades and allow the block within a
    window (for example 60 to 120 degrees).
  - Two ways to enforce it:
    - Hold the block key only while the angle is right.
    - Or hook the engine's "was this hit blocked" decision in the hit
      handler 0x005FEBF0 and veto it per blow. This is more exact, because
      it judges the blow that lands and not the pose just before it.
  - I recommend the veto: holding the key per frame reacts one frame late,
    and a fast blow falls between frames.
- **B. Recognise more guard poses.** Accept any blade orientation whose
  angle to the incoming blade is right (with A), instead of "across and
  level". That makes a high guard and a diagonal guard work.
- **C. Shield bash of OBVR's own.** The shield hand driven fast into an NPC
  within reach:
  - It applies the engine's knockback to that NPC through the same
    character-proxy push the hit handler uses (0x008907A0).
  - It optionally deals a small damage or fatigue hit through the hit
    handler with a weak attack.
  - It costs fatigue.
  - Off by default, as it is not vanilla.
- **D. Parry feedback.** A short controller vibration and the block sound on
  a successful block, so a VR block is felt and not only seen in the health
  bar.

### Open wish (Nadi, 2026-10-24): block with the torch

"Mit Torch soll man auch blocken können." Today the raised left hand blocks only with a
shield on it (`vr::LeftHandBlocks(gesture, shieldEquipped)`, vr/HandInput.h) [R]; a torch in
that hand blocks nothing, and the weapon's guard is the weapon hand's. What the engine does
with a torch in the block is unread [G: it blocks with the weapon, as with fists]. First
step: what the block (the BlockCone reroute, the angle rule) needs to take the torch's
length as the left hand's blocking body, as the shield's disc is taken.

## 2. Stagger

### What vanilla does (read in the disassembly)

A hit can do two separate things to its target:

- **The stagger.**
  - Its only start is 0x005F4FD0 (thiscall, the actor in ecx): anim group
    0x1F `Stagger`, then the process action kAction_Stagger (8).
  - It is called from two sites: 0x0060051E in the hit handler 0x005FEBF0,
    and 0x005FCCC8 after a reach search. Both have the form
    `mov ecx, <target>; call 005F4FD0`.
  - It fires on a power attack's disarm or stagger roll (0x005FC090,
    iPerkAttackDisarmChance). A second path at 0x006004D2 sets it when the
    attacker is inside the target's front cone (0x006131D0,
    `fCombatHitConeAngle`). This corrects the reading of 2026-09-26, which
    took it for a knockdown result.
- **The knockback.**
  - Every qualifying hit computes a force (0x00547690: fKnockback*, capped
    by fKnockbackForceMax, for fKnockbackTime).
  - The force is applied along the line from attacker to target, through
    the target's Havok character proxy: `mov ecx, esi; call 0065A2C0` at
    0x0060008A fetches the proxy, and `call 008907A0` at 0x006000AF pushes
    it.
  - This moves the actor for real.
  - It is skipped when the hit knocks down.

There are two more reactions:

- **Recoil** (0x005F4F00, anim group 0x1E, action 7): it hits the
  *attacker* when its blow is blocked (0x00600565, 0x005FFF4D).
- **Knockdown** (fKnockdown*): the ragdoll fall. Its function was not
  identified in this pass.

### The problem

In a headset the player's stagger jolts the view a step back. That is
motion the wearer did not make, and it causes nausea (2026-09-26).
Most likely the knockback is the lurch, because it moves the character
controller. The stagger animation may carry movement of its own. I could not
verify this from code; a headset test with each switched off would settle it.

### What is built (406d1c4)

`[Look] NoPlayerStagger` (settings: Comfort, "No stagger", default on):

- Both stagger call sites are rerouted to `OnStagger`, which does nothing for
  the player and calls the original for anyone else.
- The knockback's proxy fetch at 0x0060008A is rerouted to
  `OnKnockbackProxy`, which answers no proxy for the player. The hit handler
  already handles that case (`test ebx, ebx; je 006000B4`) and skips the
  push.
- NPCs are staggered and knocked back as in vanilla.
- The decision is `game::SkipForPlayer`, covered in FrameLogicTest.

### Open

- **Recoil when the player's own blow is blocked** (0x005F4F00): not
  touched. Whether it jolts the view is to be tested.
- **Knockdown**: built 2026-10-05 as `[Look] NoPlayerKnockdown` (dd51528;
  settings: Comfort, "No knockdown", default on): the one knockdown function
  (0x00654420, the process vtable's slot +0x2F0 in the high and the
  middle-high process - a hit's knockdown roll, a magic explosion, the
  script's PushActorAway) is rerouted in both slots and does nothing for the
  player; the damage still lands (game/PlayerStagger.h). The fatigue
  knock-out (0x006545E0, a collapse, not a throw) is left alone.
  - **Open bug (the tester, 2026-10-12):** "paar mal recht selten haben npc
    meinen player in die luft oder auf den boden gehauen, sodass auf 3rd
    person forced wurde und whl durch obvr auf 1st und dass dann in loop bis
    player aufstand. sollten wir whl wegpatchen wenn die option an ist in
    obvr." Two things in it:
    1. *A throw got through.* The tester's live INI has no
       `NoPlayerKnockdown` line, so the default (on) applies - derived from
       Config.cpp, not seen in a log. What the log of such a fight settles:
       "Comfort: the high process's knockdown rerouted at 00A71B04" at the
       start says the patch is in (a slot held by another plugin logs
       "knockdowns are left to the game" instead); "Comfort: a knockdown of
       the player was skipped ... from <address>" at the blow says the throw
       went through the patched function and was refused - then the fall
       came from elsewhere. Candidates the option does not cover: the
       fatigue knock-out (the player's fatigue at zero from the blows, a
       vanilla collapse that also puts the player on the ground), the other
       callers of the Havok push 0x008907A0 listed below (not examined),
       paralysis (not researched). Which it was: **could not verify**
       without the log.
    2. *The view flickered between third and first person until the player
       stood up.* The game shows the fall in third person; Full VR puts the
       player back into first person on every frame it finds them in third
       (`HandModeForcesFirstPerson`, camera/FrameLogic.h, `[Hands]
       ForceFirstPerson=1`, through the game's own ToggleCamera - the force
       and the setting were removed on 2026-10-20, the engine undoes every
       switch of its own) unless a
       menu is up or the player is dead. A knock state the game holds in
       third person is fought each frame; the log shows "Hands: the player
       was in third person - put back into first" repeated. **Fixed
       2026-10-20:** the knock state (+0x11C) and the sit/sleep state
       (+0x11D) hold the force off with the animation bytes
       (`EngineOwnsThirdPerson`, camera/FrameLogic.h; fall-third-person.txt:
       two switches in a run with the paralysis and the collapse played by
       the engine, against 257 before; controls-spec "the engine's other
       third persons"). The death got
       its exception for exactly this (the 2026-09-25 log, CameraHook.cpp
       at the call); the knock state has none.
    - **Proposed, nothing built (the tester asked for the note only):**
      (a) the fall treated as the death is: no forcing of first person
      while the player is in the knock state (process+0x11C, which the
      knockdown function sets to 2, "Explode Lead In") or in the fatigue
      collapse, the view held still there as `DeathViewStill` holds it;
      (b) BUILT 2026-10-13 as the exhaustion instead of the fall (the
      section below): the fatigue collapse and the paralysis fall are
      spared for the player under [Look] Exhaustion; (c) with the
      log's "from <address>", the uncovered path rerouted as the three
      known ones are.
- **Other stagger paths** that avoid 0x005F4FD0 (a script's `PlayGroup`,
  for example): not traced.
- **Other callers of 0x008907A0** (0x005ED575, 0x00604D4B, 0x00654500,
  0x00892A42): not examined. A spell's force or an explosion may push the
  player through them.

### Exhaustion instead of the collapse, and instead of the paralysis fall (the tester, 2026-10-12; built 2026-10-13, headset open)

The tester: "default an, neuer toggle: wenn fatigue low geht gibts statt
knockdown werden die waffen super schwer wie in walking dead saints n
sinners. man kann damit kaum schwingen. und der player keucht und es gibt
einen sound. selbes für paralyse."

**What vanilla does [R, read 2026-10-12 and 2026-10-13]:**

- Fatigue: "if your character suffers a spell or a hand-to-hand attack that
  lowers fatigue below 0 ... they will collapse on the ground until the
  Fatigue exceeds zero", and "melee damage is severely reduced when fatigue
  is at zero": `Damage x ((Fatigue / MaxFatigue) + 1) / 2` (UESP,
  Oblivion:Fatigue). No sound goes with it.
- Paralyze: "immobilizes the target for D seconds", the target "usually
  falls down"; for the player "the view will shift to third-person and you
  get to watch yourself fall down", the inventory still opens, "you can't
  cast spells" (UESP, Oblivion:Paralyze). The effect is actor value 0x30.
- **Both falls come from one function, the knock state's step**
  `0x006545E0` - thiscall(process, actor), the process vtable's slot +0x2F4
  (0x00A71B08 high, 0x00A72978 middle-high; right after the knockdown's
  +0x2F0), no direct calls - which runs every frame. With the knock state
  (process+0x11C) 0 and the actor not dead: the fatigue through
  GetActorValue_F 0xA (`0x005E0A60`) below 0, or the paralysis through
  `0x005E17E0` (GetActorValue 0x30 != 0), or the actor's DeadState (+0xB0)
  6, enters the fall: paralysed, state 3 with the ragdoll on (`0x0088D070`);
  else state 4, a push through the controller (`0x008AB440`) and an
  animation. State 4 passes into 3 once the blend is through (the shared
  lead-in at `0x00654803`); state 3 lies until the fatigue is 0 or more and
  the paralysis gone, then the get-up (5, 6; the same as the knockdown's).
  `PlayerStagger.h` earlier called this function "the fatigue knock-out";
  it is the whole state machine.
- The reference, The Walking Dead: Saints & Sinners: stamina drains with
  sprinting, climbing and melee, and players describe the character "so
  winded that swinging a bat more than a couple of times becomes a problem"
  (Road to VR's review). The exact rule of its heavy arms rests on player
  comments.

**Built [R]:** `[Look] Exhaustion=1` (Comfort, "Exhaustion, not collapse"),
`ExhaustionBelowPercent` (12, "Exhausted below (%)"),
`ExhaustionAbovePercent` (25, "Recovered above (%)"), `ExhaustionWeight` (2.5
since 2026-10-14, 4 before, "Exhausted weapon weight (x)"), `ExhaustionSounds` (1, "Exhaustion
sounds"); game/ExhaustionLogic.h (the decisions, exhaustion_test),
game/PlayerStagger (the reroute), vr/WeaponWeight.h (the heavy arms,
weapon_weight_test), game/GameSound.h (the waves):

1. **The fall spared.** Both step slots are rerouted to OBVR's own, the
   knockdown's pattern: for the player, with the setting on, the step is
   SKIPPED on a frame it would begin a fall - the knock state 0 and the
   fatigue below 0 or the paralysis set (`CollapseSkipped`) - and run on
   every other (a knockdown under way, the lying and the get-up keep their
   course; NoPlayerKnockdown off still knocks the player down); everyone
   else goes to the original. The player stays on their feet; the fatigue
   and the paralysis themselves stay the game's - no spells while paralysed,
   the weak blows at no fatigue. The DeadState 6 trigger is left to the
   engine (not read: what it is). Logged once per episode "Comfort: the
   player's fatigue collapse was skipped (Look.Exhaustion) - fatigue n,
   paralysed 0, knock state 0; they stay on their feet, exhausted" (or
   "paralysis fall").
2. **The state.** Exhausted under 12 % of the base fatigue (the
   GetActorValue_F readers the teleport's cost uses) or while paralysed;
   over above 25 % and not paralysed (hysteresis: vanilla regenerates
   fatigue, so it ends by itself); eased in over 0.5 s, out over 1.5 s.
   Off mid-state: over at once.
3. **The heavy arms** (`vr::WeaponLagExhausted`): section 6's lag with its
   time constant from a floor of 80 ms - a dagger that never lagged lags
   now - times the factor, and the caps times it, bounded at half a metre
   and ninety degrees, blended in by the ease. A longsword at the 40 %
   slider: 25 ms and 0.1 m become 200 ms and 0.25 m at x2.5 (320 ms and 0.4
   m at the first x4) - a hand at 1 m/s trails the weapon 0.2 m behind it,
   the swings by motion seldom reach the strike's speed, and the game's own
   formula halves the damage at no fatigue. The fists have no spring and
   are not changed (open: a threshold of their own).
4. **The gasps (the tester, 2026-10-13 noon: "die schnaufer und keuchen
   gefallen mir nicht, wie machen andere mods das? ... so nah wie möglich an
   vanilla").** The morning's waves (a breathing loop and a cry, CC0, through
   winmm) are out. Vanilla has no breathing for low fatigue, only the
   collapse; its human gasps - the sound forms NPCHumanGaspMale (000C80ED)
   and NPCHumanGaspFemale (000C80EC), the folders sound/fx/npc/human/gasp/
   male and /female of four files each (1.0 to 2.4 s), one picked by the
   engine each play - are the nearest it has, so they are what plays, by the
   player's sex (`game::PlayerSex`: the reference's base form through vtable
   +0x170, an NPC's flags byte at +0x28 and 1, as the script's GetIsSex
   reads it at 0x004F6FC0 / 0x00519D20), through the game's own sound
   system with a volume of its own (`game::PlaySoundFormVolume`: 0x006B7280
   thiscall(sound, float) after the start, the system's 0x006ABA20 keeping
   it at the sound's +0x3C and applying it under the master and the effects
   sliders, 0x006B6F20). Paced as Realistic Fatigue (abo, Nexus 10925; its
   readme) paces its panting: a heartbeat every 1.0 to 0.333 s by the
   fatigue missing (its blurInterval = 1 - 0.6666 * level^2), a pant every
   four beats (pantInterval = 4 * blurInterval: 4 s with nothing missing,
   2.5 s at a quarter left, 1.33 s at none), louder the more is missing
   (pantLevel = level^2, never under 0.25); paralysed counts as all of it
   missing. The moment a fall was spared: one gasp at full volume, the
   pants a whole interval after it; a spell begun without a fall: the first
   pant half a second in (`ExhaustionMissing`, `HeartbeatSeconds`,
   `PantIntervalSeconds`, `PantVolume`, `StepBreath`, exhaustion_test).
   Recovered, or the sounds off: nothing more (the gasps are one-shots).
5. Logged "Exhaustion: begun - fatigue n of n (n%), paralysed 0, a fall
   spared; the weapon's weight x2.5 over 0.5 s", "Exhaustion: over after n s
   ...", "the gasp - the game's NPCHumanGaspFemale (000C80EC) at 100%
   volume; a pant every 1.3 s with 100% of the fatigue missing (0 pants, 1
   gasps this spell)", the first two pants of a spell and then every tenth
   ("a pant - ..."), and at a hand script's marks and every two seconds
   exhausted the state with the weapon's gap behind the hand and the
   spell's counts.

**Harness `exhaustion.txt`, PASS 2026-10-13 (the fourth run; a longsword
drawn, the console's `setav paralysis 1` / `0`, then `forceav fatigue -100` /
`145`):** the paralysis - "Comfort: the player's paralysis fall was skipped
(Look.Exhaustion) - fatigue 145.0, paralysed 1, knock state 0; they stay on
their feet, exhausted", "Exhaustion: begun - fatigue 145 of 145 (100%),
paralysed 1; the weapon's weight x4.0 over 0.5 s", "the gasp", "the breath,
looped", the slow swings "up to 0.22, 0.33, 0.29 m from the hand" (0.12 m
fresh, the cap x4 would allow 0.5), at the mark "ease 1.00, factor x4.00 ...
the weapon lagging, 0.09 m behind the hand", cleared "the breath stopped",
"over after 5.5 s"; the fatigue - "fatigue collapse was skipped ... fatigue
-99.9", "begun - fatigue -100 of 145 (-69%)", the gasp and the breath, "0.25
m behind the hand" at a two-second line, forced back "over after 10.1 s". The
three runs before taught: the game has no `damageav` and no `restoreav`
(ModAV, SetAV, ForceAV are its commands); and at low fatigue the tutorial
quest's "Your Fatigue is
low" message box (Oblivion.esm; the harness save still has the quest) pauses
the world, so the knock state step never runs under it - neither Enter nor
Space closes it from the script - which is why the fatigue comes last.
**With the game's gasps, PASS 2026-10-13 15:48 (the same script):** the
paralysis - "the gasp - the game's NPCHumanGaspFemale (000C80EC) at 100%
volume; a pant every 1.3 s with 100% of the fatigue missing (0 pants, 1
gasps this spell)" (the harness character is a woman), "a pant ... (1
pants, 1 gasps)", "(2 pants ...)", "over after 5.6 s"; the fatigue -
"begun - fatigue -100 of 145 (-69%)", the gasp, the pants, "over after 9.8
s"; no "could not be played".

**Headset (2026-10-13 evening): "erschöpfung: passt aber: attacken sind
verboten und statt 4x nur noch 2.5x gewicht: wirkt ok".** Read as: at x4
the weapon trails so far that attacking is as good as forbidden, so x2.5
is the default since 2026-10-14 (the tester's INI has no key for it, so
the default is what plays). The tester's log of that evening: two spells
of 19 and 11 s at x4, begun at 10 and 9 % by teleport jumps, the weapon
"up to 0.37 m and 70 degrees from the hand" in a swing and 0.01 to 0.15 m
behind it at the two-second lines, the pants every 1.5 to 2.2 s at 69 to
95 % volume (the gasps: "passt"); no fighter in reach during either spell,
so no strike was tried under it - the swing is still detected from the
controller, the hit test runs on the trailing blade. The engine's hit
handler 0x005FEBF0 reads the fatigue (actor value 10) only for its debug
line "%.20s hits %.20s for %.1f/%.1f %s damage!" (Ghidra, 2026-10-14);
whether the engine refuses an attack at low fatigue somewhere else was not
looked for.

**Open:** the headset - whether the game's gasps every 1.3 to 2.5 s read as
panting or as being startled (a pant could take the quieter of the four
files, or the drowning gasps' kin; nothing else human is in the game's
sounds), whether a strike lands under x2.5 (a fight while exhausted has
not been had), and whether a paralysed player who stays on their feet can
still walk (the engine's own paralysis blocks the spells; the legs were the
fall's) - the tester's call whether to hold the legs too. The knockdown's
open bug above keeps its (a) and (c).

### All the proposals, including those not built

1. **No stagger and no knockback for the player** (built, default on). It is
   the simplest and has no motion at all. The cost is that combat gets a
   little easier, because the player is never interrupted.
2. **Stagger without movement.** Keep the stagger (the player cannot act for
   its length, as in vanilla) and drop only the knockback push. Both are
   now separate reroutes, so this is a second switch. This is the choice if
   the game feels too easy without stagger.
3. **Knockback damped instead of removed.** Scale the force for the player
   (for example to a quarter) by rerouting 0x008907A0 at 0x006000AF and
   scaling its force argument. A small push without the jolt; to be judged
   in the headset.
4. **Hold the view during a stagger.** Keep the camera where it was while
   the body moves, as the still death view does. The world would slide under
   the player instead of the player moving. This is likely worse, so it is
   listed only for completeness.
5. **Comfort vignette on a hit.** Darken the edges while a stagger or push
   plays, as the snap-turn vignette does. It helps with options 2 and 3 and
   when the switch is off. It prevents nothing.
6. **Recoil for the player removed as well.** Reroute 0x005F4F00's callers
   the same way, if the test shows that the recoil jolts too.

## 3. Death

- The death view (`[Look] DeathViewStill`) holds the camera from the last
  living frame. The first dead frame is already the game's third-person chase
  camera, so holding from there showed a jump.
- Since 2026-09-26 it also holds the base rotation and the vertical offset
  (`StepDeathTurn`). A "final stagger" was still felt at death; the
  assumption, not yet verified, is the chase camera turning and sinking
  towards the body. The tester confirmed it on 2026-09-26: "nun perfekt".
- `[Look] DeathViewBackMetres` (0.8 at first, 0 since the next point) holds
  the view that far behind the living eyes, level along the heading, so the
  body is seen falling in front of it. Otherwise nobody could tell what had
  happened.
- Since the tester's question (2026-09-26, "why a jolt back instead of the
  body shown half a metre ahead"), the step back is off by default
  (`DeathViewBackMetres=0`) and `[Look] DeathBodyAheadMetres` (default 0.5)
  draws the dead body that far ahead instead, level along the living
  heading, while the view stays still (`game::DeathBody`). The skeleton
  (Bip01 down) gets the offset on its world translations and bounds for the
  world render only and everything is put back after it, because the
  ragdoll's nodes are Havok's and a node update would put them back. Not
  built: a fade-in of the body.
- `[Look] DeathBodyUpMetres` (default 0) raises (negative: lowers) the body
  drawn ahead, at the tester's request.
- **Flung body and hovering fall (researched 2026-09-26, nothing built).**
  - The killing blow's push on a ragdoll is vanilla: game settings
    `fDeathForceForceMin` (35) and `fDeathForceForceMax` (85) cap it
    (cs.uesp.net/wiki/FDeathForceForceMax: "determines how much force is
    allowed when someone dies"). Players report corpses "sent to the
    stratosphere" in vanilla (reddit.com/r/oblivion/comments/1f3j305).
    Realistic Ragdolls and Force (nexusmods.com/oblivion/mods/5011) tames it.
    Nothing specific to the player's own corpse was found.
  - Slow or hovering falls are credited to vanilla ragdoll collision shapes
    and damping by Ragdolls for Oblivion (nexusmods.com/oblivion/mods/51844:
    "All ragdolls will no longer fall down slowly"). Not verified as the
    cause here.
  - High frame rate: Havok steps at `[HAVOK] fMaxTime` (0.0167, 60 Hz).
    The installed Oblivion Display Tweaks has `bfMaxTime=1`, which adjusts it
    to the frame rate, so 90 Hz physics should already be covered. No source
    ties Oblivion ragdoll launches to high fps.
  - OBVR's own share, not excluded: `DeathBodyAheadMetres` draws the body
    level along the heading, so on a slope it can look above (or in) the
    ground. Setting it to 0 for one death tells the two apart.
  - Possible later: a player-only death force, scaling the push where the
    death force is applied (not yet found in the exe).
- `[Look] HideHudWhenDead` (default on) hides the HUD and the crosshair
  while dead, until the load menu opens; the game kept drawing its HUD for
  seconds (`camera::HudHiddenForDeath`).

## 4. Open (the tester, 2026-10-10, for later): combat made VR-ready - slower attacks, fewer attackers at once

The tester: "combat vr ready machen: gegner angriff tempo bei % slider in
settings verringern können. dann wird die animation verlangsamt, sodass
blocken und kampf in vr einfacher ist. und auch die schnelligkeit/
häufigkeit mit der die gegner angreifen. und neue option: immer nur N
gegner darf beim nahkampf/oder immer gleichzeitig angreifen, sodass vr
player nicht reizüberflutet werden, n einstellbar 1-5 default 1."

Three settings, nothing built:

1. **Their attack's tempo, a % slider** (proposed `[Hands] EnemyAttackSpeed`,
   100 = the game's own): the attacker's attack animation plays slower, so
   the windup is long enough to see and to block or parry - the blade a VR
   player has to meet with their own (weapon-collision-spec section 11).
2. **How often they attack**: the time between one attack and the next, and
   the chance an attack is chosen at all.
3. **N at a time** (proposed `[Hands] MeleeAttackersAtOnce`, 1-5, default
   1; or "all", as vanilla): only N of those fighting the player in melee
   may be attacking at any moment; the rest hold off until one is done -
   no flood of blows from every side.

What OBVR has to build on (read in the repo): a game setting held at
runtime and given back (`CombatReach`, fCombatDistance); the movement
setters of each fighter's high process (`SlowApproach`, `HoldStill`);
the hit handler's block hooks (`Parry`); and each fighter's process
action, read every frame (`ActionOf`, `IsAttackAction`, game/BladeBodies,
game/ParryLogic.h) - so who is attacking right now is known.

Candidates, none researched:

- **The tempo.** Oblivion plays an attack animation at a rate set from the
  weapon's speed (the WEAP record's Speed; the setting names
  `fMediumWeaponSpeedMax` and `fLargeWeaponSpeedMax` are in Oblivion.exe's
  strings). Where the engine sets an attacking actor's animation rate is not
  read; slowing only their attack sequences there (never the player's) is
  the way. A whole-actor time scale would slow their walk and turn too.
- **How often.** Setting names in Oblivion.exe's strings: the combat
  style defaults `iAIDefaultAttackChance`, `fAIDefaultAttackNoAttackMult`,
  `fAIDefaultAttackDuringAttackMult`, `fAIDefaultAttackDuringBlockMult`,
  `iAIDefaultPowerAttackChance`, and `fPowerAttackDelay`,
  `fCombatAdvanceNormalAttackChance`, `fCombatForwardAttackChance`. Their
  exact meaning is not read; most NPCs carry their own combat style record
  (CSTY), which these defaults may not reach. A runtime setting like
  CombatReach's where one applies; the combat style's own values otherwise.
- **N at a time.** The binary has `iNumberActorsInCombatPlayer` - by its
  name how many actors may fight the player at once (meaning not verified;
  that would be all-or-nothing per actor, not per blow). The finer gate is
  OBVR's own: with every fighter's action known, an attack that would make
  N+1 is held back - the attack's start refused, or the fighter kept at the
  edge of their reach with the movement setters (as `SlowApproach` does)
  until a slot is free. Where the AI decides to attack is not located.
- Order proposed: N at a time first (the largest comfort, and OBVR already
  sees every attacker), then the tempo, then how often.

**The ring (the tester, the same day, for later):** "neues feature hinter
toggle, default an: gegner umkreisen den spieler auf einer distanz und geben
ihm x meter abstand (default 5m). einstellbar 0=off bis 10m. aus diesem
kreis an gegner greift dann immer nur die anzahl an die der spieler in der
anderen setting angegeben hat. ähnlich zu blade&sorcery." Nothing built.

4. **Fighters circle at a distance** (proposed `[Hands] CombatRing=1` and
   `CombatRingMetres` 5, 0 = off, up to 10): those fighting the player who
   are not attacking right now keep about that distance and walk round the
   player rather than closing in; out of that ring, only as many as setting
   3 allows (N, default 1) step in to attack, and go back out to the ring
   when their attack is done. The B&S picture: a loose circle, one or two
   engaging, the rest waiting at the edge.
- How it could go, on what exists (not researched in the engine): the
  ring is setting 3's gate plus a place to wait. A waiting fighter's
  movement goes through the same two setters `SlowApproach` and `HoldStill`
  already reroute (HighProcess vtable +0x2C4/+0x2C8; xOBSE's flags:
  forward 0x1, backward 0x2, left 0x4, right 0x8): inside the ring the
  forward step becomes a step back, at the ring a step to the left or the
  right - a strafe, which the AI's own turn to face its target (combat
  keeps it facing the player) makes a walk round the player. Who waits and
  who attacks is OBVR's own choice each frame (nearest first, a turn each
  after an attack ends), with every fighter's action read as the parry
  reads it. Their attack itself still has to be held back while they wait
  (setting 3's open question: where the AI starts an attack). With the
  slow approach at 5 m too, they walk the last metres to the ring.
- To watch for: a ring larger than the room (indoors, a corridor) - the
  ring gives way to walls by the character controller's own collision, but
  a fighter stuck in a doorway may never get a turn; ranged fighters and
  casters keep their own distance and are not ring members; creatures
  circle the same way (their flags are the same process's).

**Built 2026-10-10 (items 3 and 4; the tester: "bau als nächstes das combat
Feature mit der umkreisung von gegnern und dass nur n gegner gleichzeitig
angreifen"):**

- `[Hands] CombatRing=1`, `CombatRingMetres=5` (0 off, up to 10),
  `MeleeAttackersAtOnce=1` (1-5); settings rows "Fighters' ring", "Ring
  distance (m)", "Attackers at once".
- **Who is in it** (game/CombatRing.cpp): each frame the process manager's
  high list - a Character or Creature in combat (Actor vtable +0x334) whose
  combat target is the player (Actor vtable +0x338, 0x005E0AF0, the combat
  controller's target; xOBSE GameObjects.h `GetCombatTarget`, the next
  virtual after `IsInCombat`; the slot read in both vtables, the function
  disassembled), with its distance across the ground and whether its process
  action is an attack (the parry's `IsAttackAction`).
- **The turns** (game/CombatRingLogic.h, combat_ring_test): N have a turn
  and fight as the game has them. A turn ends when its blow is through, or
  after 8 s without one - only while somebody waits, never mid-blow; the
  longest waiting (the nearest among equals) steps in. One alone keeps its
  turn. A waiting fighter mid-blow is left to finish it. Members gone from
  the list (dead, fled, out of combat) are forgotten; past 16, the rest are
  left to the game.
- **The waiting's legs** (`WaitingOrder`, `RingFlags`): inside the ring
  less its band (0.7 m, at most a third of the ring) a walk backwards; in
  the band a walk to the side (half left, half right by the actor's
  address), which the AI's own turn to its target makes a walk round the
  player; beyond it as the game has them (the slow approach walks them in).
  The flags go through the slow approach's two rerouted setters
  (`SetRingLegs`) and give the legs their walk; they do NOT move an NPC the
  AI wants elsewhere (harness, the first run: a fighter given a walk
  backwards stood at 1.9 m for five seconds). The step itself is the ring's:
  each frame a waiting fighter is placed away from the player - 65 units a
  second at the band, up to twice that deep inside, where the AI's own
  step towards the player otherwise cancels it - or round the player at 65
  with a pull to the ring's line (`RingStepOffset`,
  `game::PlaceActorAt`, the off hand's drag). A waiting fighter is kept off
  even mid-blow; the one held by the off hand stands whatever the ring
  wants. Their attack itself is not refused: kept outside the reach no blow
  of theirs lands.
- Logged: "Combat ring: n fighting the player - t with a turn, w waiting at
  r m (...)" on every change, the first forty; at a hand script's mark a
  state line per member.
- **The tester, 2026-10-10:** "sieht wie gehen aus vll etwas zu schnell.
  jeder npc kriegt evtl zu wenig zeit zum angreifen. eher hektisch." Now
  the walk 65 units a second (was 100), and a turn lasts at least
  `[Hands] CombatRingTurnSeconds` (6 s, 2-30; settings "Turn length (s)")
  before a blow ends it, 10 s at most without one. Harness (2026-10-11):
  turns of 6-9 s, the one stepping out walked 1.4 -> 3.0 m in 3 s.
- **Harness PASS** (`tools/hand-scripts/combat-ring.txt`, three bandits, a
  3 m ring, 2026-10-10): one with the turn at 0.5-2.4 m, the two waiting
  held at 3.0 m going round (order 2), one sent back walking out 1.8 -> 2.2
  -> 3.0 m; the turn passed six times in 25 s. To watch in the headset:
  whether the placed steps look like walking, and walls (a placement is not
  stopped by them). Ranged fighters and casters are members too.

**Built 2026-10-11 (items 1 and 2; the tester: "mach nun das mit dem attack
speed und häufigkeit in der spec"):**

- `[Hands] EnemyAttackSpeed=75` (20-200) and `EnemyAttackFrequency=100`
  (10-200), per cent of the game's own; settings rows "Attack tempo (%)",
  "Attack frequency (%)". Only those fighting the player (combat target the
  player, the ring's reading); game/AttackTempoLogic.h, game/AttackTempo.cpp,
  attack_tempo_test.
- **The tempo.** Each attack sequence (groups 19 AttackBow to 26
  AttackRightPower, in any of the actor's five slots) is followed while it
  plays, and its offset (NiControllerSequence +0x48, the draw speed's
  reading) moved each frame so its time runs `speed` times what the engine
  would run it: pose, keys - the blow's hit among them - and the group's end
  all come that much later, in their order. Read in the harness: at the
  game's own speed an attack sequence's time runs at the weapon's speed
  (0.7, 0.9, 1.1 of the clock for the bandit's weapons; its frequency at
  +0x28 reads 1 throughout, so the engine adds the weapon's speed to the
  offset the same way - a plain half step taken back gave 0.4, not 0.45, of
  the clock at a weapon's 0.9), so the step taken back is scaled by that
  rate, measured over the first frame followed before the offset is touched.
  The engine fires a sequence's keys over the window from its time at the
  clock before the step to its time after, both read with the offset as it
  is then (0x00476F4D, 0x00477A4B), so an offset moved back would make a
  key fire twice: the one call that fires them (0x00477AB1 ->
  TESAnimGroup's handler 0x0051AF70) goes through a wrapper that starts a
  followed sequence's window where its last one ended.
- **The frequency.** The melee decisions (0x006189E0, 0x00620290; 0x0061E5A0
  too) roll against the attack chance the combat style computes
  (0x00546800: chance plus bonuses and skill modifier, times the style's
  "attack while under attack" or "attack not under attack" multiplier -
  the CS wiki's Combat Style page; TESCombatStyle's vtable 0x00A407F4),
  and a decision to hold draws a hold timer (0x00546B20, between the
  style's min and max, 0.5-1.5 s by default) before the next. The two
  multipliers' getters (0x004AA0B0, 0x004AA130) are detoured to return
  theirs times the setting, and the two hold draws' call sites (0x00619176,
  0x00620BFB) to draw theirs divided by it - in scope only: each decision
  fetches the style through Actor::GetCombatStyle (0x005E0F50) right
  before, which is detoured to say whether that actor fights the player.
- **Harness PASS** (`tools/hand-scripts/attack-tempo.txt`,
  `attack-frequency.txt`, the control `attack-tempo-vanilla.txt`; one
  bandit at an untouchable player, the ring off, 2026-10-11):
  - the control: the bandit's swings' time ran x0.90 of the clock (its
    weapon), 66 and 32 swings in two minutes of marks, holds of 0.5-1.5 s
    drawn as they are;
  - tempo 50: every swing's time ran x0.35 of the clock at a weapon's
    0.70 - "x0.50 read back"; a power attack of 1.47 s of animation took
    4.18 s;
  - frequency 50: 33 swings in the minute the control had 66, every hold
    doubled (1.1-3.0 s) - "(x2.00)" - 12 holds drawn, 12 attack actions.
  Logged: "Attack tempo: ..." on install, per attack followed, per swing of
  anyone fighting the player (its animation's run against the clock), per
  hold drawn in scope; at a hand script's mark a state line.
- To watch in the headset: whether a slowed attack looks like slow motion
  of that fighter alone (its walk is not slowed) or stutters; whether the
  hit lands when the blade arrives; bows (AttackBow is slowed too); and
  whether 75 is the right default - the tester asked for a slider.
- **The tester's round (2026-10-11):** "angriffs speed geht! und häufigkeit
  wohl auch aber will mehr testen."

**Built 2026-10-11 (the ring, after the tester's round): the steps probed,
turns of a random length, the weakest first.**

The tester: "gegner umkreisen einen: ja das geht aber teilweise pusht du die
gegner in oder auf wände oder zäune. das muss besser gehen." and "gegner
bekommen turns: vll können wir hier auch random time geben von random(5s,
15s) oder? was würdest du hier vorschlagen? und auch ob es eine hierarchie
gibt? also erstmal kommen nur alle low levels an und dann die high levels.
schlag auch hier was vor."

- **The steps probed** (`RingStepProbe`, `RingStepClear`,
  game/CombatRingLogic.h; `ProbeRingStep` in game/CombatRing.cpp): a step
  is placed, not walked, so before a waiting fighter is placed the world is
  asked - the blade's own ray pick (layer 23), as the blade's step asks it:
  a ray at knee height (30 units above the feet) and one at chest height
  (95) along the step and the body's half width (22 units) past it, each
  stopped by anything fixed (`ContactKindOf`: fixed and keyframed bodies -
  walls, fences, trees, doors; clutter and the living are passed), and the
  ground under the new spot against the ground under the old (a ray from
  60 units above the feet to 90 below): a rise or drop over 24 units (a
  fence's top, a ledge) refuses the step, a stair's step is climbed (the
  placement carries the rise). A refused step round the player turns that
  fighter round the other way (`RingStepRefused`, once a second at most:
  one blocked both ways stands); a refused step back is simply not taken.
  Four picks a waiting fighter a frame.
- **The legs' side, and only the shortfall placed** (found by the probe's
  first harness runs, 2026-10-11): the ring's "left" is counter-clockwise
  seen from above, and the legs face the player - so it is a step to the
  actor's RIGHT. The flags had it the other way round, and the legs' own
  walk (the strafe flags DO move an NPC, unlike the walk backwards - the
  AI's step towards the player cancels that one) carried the waiting
  against the placed step: they went round the wrong way at 20 units a
  second (the legs' 85 less the step's 65), and at a wall they stood,
  pushed into it by the legs, while the probe looked the other way along
  the placed step and refused nothing. Now `RingFlags` gives the legs the
  ring's sense, and the step is a shortfall: what the legs did since the
  step before is measured (`RingLegsWalked`: the position against where the
  last step left them) and only the rest of the wanted step along its
  direction is placed (`RingStepShortfall`; the whole step when the walk is
  unknown - a first step, a frame without one; up to twice the step against
  legs walking the wrong way). Measured (ring-wall, three runs): going round
  the legs walk 67-88 units a second along the step and the ring adds 0-12
  (the pull to the line); sent back the legs walk 54 against and the ring
  places up to 130. The state line gives both rates.
- **A blow in the air is the attack sequence, not the action**
  (`AttackSwinging`, game/AttackTempo.cpp): the "never mid-blow" rule and
  "an attack in this turn" read the process's action at first - which
  stays "attack" across a whole chain of blows (the harness: one action of
  12.7 s over 11 swings, waiting fighters "attacking" at 3 m for half a
  minute), so turns of 6 s ran 15 and one of 13.6 s ran 22.8. Now a blow is
  an attack sequence animating in one of the actor's five slots, as the
  tempo's watch reads them each frame (one frame behind the ring's step):
  turns end within a swing of their length (11.5 of 11.6, 9.3 of 8.0, 4.9
  of 5.5).
- **Seen, not changed**: the waiting swing at the air from 2.4-3.2 m (the
  swings logged by the tempo are theirs while they wait) - the AI's
  decision, not range-gated by the ring; harmless, since they cannot reach.
  A proposal for later: a waiting member's attack chance to 0 through the
  tempo's multiplier hook, so only those with a turn swing. And "no ground
  there" refused a step once or twice a run in the hall's middle (x 160-350,
  y -720..-780 of the harness hall); the spot is not identified, the
  fighter turned round and walked on.
- **Turns of a random length** (`CombatRingTurnMaxSeconds`, 12; settings
  "Turn length at most (s)", 2-60): each turn given is drawn a length
  between `CombatRingTurnSeconds` and it (`TurnLengthFor`, a linear
  congruential draw a frame, two turns given in one frame spread apart);
  the idle end (no attack) stays at 10 s or the longest turn, whichever is
  more. The tester's "random(5s, 15s)" is `CombatRingTurnSeconds=5`,
  `CombatRingTurnMaxSeconds=15`; the defaults 6-12 keep the old 6 as the
  shortest.
- **The weakest first** (`CombatRingByLevel=1`; "Weakest fight first"): the
  next turn goes to the lowest-level fighter waiting (`RingGoesBefore`),
  ties to the longest waiting, then the nearest; off, the longest waiting
  first as before. One whose turn just ended rests a turn's shortest
  length before it is chosen again (found in the unit test: the lowest
  level is the lowest again the moment its turn ends, so a lone weak
  fighter took every turn while the boss watched) - two or more weak ones
  take turns among themselves and the boss waits; one weak one and a boss
  alternate. The level is the game's (`LeveledLevel`: the base form's
  level, or with the PC-level-offset flag the player's level plus it, held
  between the base's min and max - xOBSE's TESActorBaseData at +0x24 of
  the base form: flags +0x04, level +0x0E, min +0x10, max +0x12). A
  starvation cap: one waiting a whole minute (`kRingStarvedSeconds`, 60)
  steps in whatever its level, so a boss is not held back for a fight in
  which the weak ones never die. **The proposal, and why**: strict by level
  as asked, with the cap as the safety; an alternative not built would
  weigh level and waiting time together (a boss waiting twice as long as a
  mook goes first) - softer, but the tester asked for the hierarchy.
- Logged: "Combat ring: ... given this frame <actor> (level n, s s); at
  once n, the weakest first", "Combat ring: <actor>'s step round refused -
  something fixed at knee height (rise 0 units); it goes round the other
  way"; at a mark "Combat ring: the walls - +x n, -x n, +y n, -y n units
  from the player" (the nearest fixed thing in the four directions, 0 none
  within 3000) and the state line with each one's level, position, side,
  turn length, and since the last mark the steps placed and refused and
  the legs' and the ring's units a second.
- Tests: `combat_ring_test` (the settings' bounds, the draw, the level with
  and without the offset and its bounds, the order by level, the starvation
  cap, by level off, two lengths from one draw; the probe's every refusal,
  the side kept and turned, the cooldown; the legs' side; the shortfall -
  the walk unknown, walked, half walked, across, against and capped, a
  diagonal, no step, no number; the walk measured from the step before,
  the books, unknown after a frame without a step).
- **Harness PASS** (`tools/hand-scripts/ring-wall.txt`, three bandits, a 3 m
  ring, turns 5-15 s, the player walking ahead to the hall's far wall and
  standing 14 units from it, 2026-10-11, three runs of the final build):
  "step round refused - something fixed at knee height ... it goes round
  the other way" for all three at the wall, 0-2 refusals a fighter in 5 s,
  every fighter's position on the player's side of the wall (y -546 and
  below against the wall's -518); turns drawn 5.5-11.6 s and ended within
  a swing of their length; see docs/hand-script-harness.md.

**Built 2026-10-12: the ring holds still in a pause, and its fighters walk**
(the tester, after his first fights with the ring: "gegner ring: in pausen
menüs scheints du die gegner weiter zu pushen obwohl eig pause sein soll und
nix passieren soll. du pusht die gegner auch weg wenn sie eig stehen oder?
das wirkt unrealistisch, die sollten eig gehen"):

- **The pause.** The ring stepped every frame the camera ran, menus
  included, and behind the Esc menu no leg walked - so the whole step was
  "the legs' shortfall" and was placed: the waiting slid across the ground
  with the game stopped (his log: the Esc menu and a message box during the
  fight). Now nothing of the ring runs while the world is paused behind a
  menu (`game::WorldPausedForMenus`, MenuPause.h: the same answer the update
  step's redirected sites get - vanilla's pause, unless the menu on top
  keeps the world running, as the container's panel does): no step, no
  placement, the turns' clocks stand. Logged "Combat ring: the world paused
  behind a menu - n fighter(s) held where they stand".
- **The slide.** Inside the ring the waiting were sent back with the walk
  backwards, which does not move an NPC the AI wants near (2026-10-10, -11:
  one stood at 1.9 m; sent back, the actor moved 54 units a second TOWARDS
  the player while the ring placed up to 130 a second away) - a fighter slid
  backwards across the ground while it stood or walked at the player. Now
  (CombatRingLogic.h, "The legs walk; the ring only adds to a walk"):
  - every waiting fighter's legs walk to the side - round the player - at
    the ring and inside it alike (`RingFlags`; the walk to the side does move
    them, 67-88 units a second in ring-wall's runs);
  - the ring places only the radial part: inside, a drift outwards of 40
    units a second, so the walk round spirals out; at the ring, the pull to
    its line (`RingRadialStep`);
  - and only onto legs that walk, and not against them (`RingMayPlace`, the
    walk group animating in the actor's five slots, TESAnimGroup Forward 3
    to FastRight 10 - read as the attack watch reads its attack groups):
    standing, striking or staggering legs get nothing placed, and nothing
    is slid away from the player while its legs walk at him;
  - the walk round is asked of the world at knee height before the legs take
    it: a wall or a fence turns it round (`RingStepRefused`), as before.
- **Harness PASS** (`tools/hand-scripts/ring-walk.txt`, three bandits, a 3
  m ring, 2026-10-12): the waiting walking round with their legs (legs
  group 5 or 6, flags 0104 / 0108), the legs 46-86 units a second and the
  ring placing 1-34 (the drift, the pull to the line), steps held 0-21 a
  mark (legs not walking); in the Esc menu "paused - <actor> at x y" at
  both marks three seconds apart, every position the same to the tenth of
  a unit; out of it the ring went on and the turns passed. Seen, not
  looked into: in the first mark after the pause one waiting fighter had
  gone 455 units in 1.2 s (its legs "697 units a second", the ring placing
  27) - a run of its own, not the ring's step.
- **Seen in the headset** (the tester, 2026-10-12 evening, the build of
  d465faf and later): "auch bewegst du npcs nicht mehr im esc oder pause
  menü. und sie gehen nun nach hinten statt zu levitaten" - the pause
  holds, and the waiting walk instead of sliding. Not said: whether the
  drift out on a sideways walk reads as walking.

**Built 2026-10-15: the knockback of the player's own hits.** The game
shoves whoever the player hits through their Havok proxy on every landed
blow (fKnockback*; a light sword hit moved a frozen beggar 19 units in the
harness, out of the next blow's reach - weapon-collision-spec 17.3). The
tester: "gegner wegschieben mit schlägen ... wäre ein fehler." `[Hands]
EnemyKnockback` (per cent of the game's push, default 0) and
`EnemyKnockbackPowerOnly` (light hits push nobody); settings rows "Hits push
enemies (%)", "Only power attacks push" under Hands: enemies. The player's
own knockback stays `[Look] NoPlayerStagger`'s business, other fighters'
hits the game's own. Harness blade-held PASS 2026-10-15: at 0 % the beggar
stood at the same spot through four strikes; blade-held-cap at 100 %: 19
units back a hit, the game's own.

## Test reminders

- With the switch on: fights with power attacks show no lurch back. The log
  says once "Comfort: a stagger of the player was skipped" or "... knockback
  ... skipped".
- With the switch off: the vanilla stagger and knockback are back.
- Anything that still jolts (recoil, knockdown, spells) goes into the list
  above.
- A blow that still throws you into the air or onto the ground (seen
  rarely, 2026-10-12): send the log. Wanted in it: "the high process's
  knockdown rerouted" at the start, and at the blow "a knockdown of the
  player was skipped ... from <address>" or nothing - nothing means a path
  the option does not cover (section 2, Open). The flicker between third
  and first person until you stand was "Hands: the player was in third
  person - put back into first", repeated - gone since 2026-10-20 (the knock
  state holds Full VR off, "Hands: the engine's third person left alone ...
  knock state N" in the log instead).
