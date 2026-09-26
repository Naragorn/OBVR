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
  It sets two floats behind a flag and is probably the gamepad rumble; this
  is a guess.
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
  - The 90-degree blade check (proposal A) would be a second, stricter
    condition on top of this cone.
  - The veto belongs at the same place. 0x006131D0 is one call with one
    boolean result (0x005FF83E), a clean hook point for "blocked only if the
    blades cross".
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
- **Knockdown**: not touched. The player falls as a ragdoll. The death view
  (`DeathViewStill`) shows how a held view would handle a fall.
- **Other stagger paths** that avoid 0x005F4FD0 (a script's `PlayGroup`,
  for example): not traced.
- **Other callers of 0x008907A0** (0x005ED575, 0x00604D4B, 0x00654500,
  0x00892A42): not examined. A spell's force or an explosion may push the
  player through them.

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

## Test reminders

- With the switch on: fights with power attacks show no lurch back. The log
  says once "Comfort: a stagger of the player was skipped" or "... knockback
  ... skipped".
- With the switch off: the vanilla stagger and knockback are back.
- Anything that still jolts (recoil, knockdown, spells) goes into the list
  above.
