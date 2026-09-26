# Combat in Full VR: block and stagger

Status: **partly built.** Written 2026-09-26 from the tester's reports and the
disassembly of Oblivion.exe 1.2.0.416. What is built says so; everything
else is a proposal. Addresses are this binary's.

## 1. Blocking

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
    iPerkAttackDisarmChance) and on the knockdown result of 0x006131D0.
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
- **Knockdown** (0x006131D0, fKnockdown*): the ragdoll fall.

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

## Test reminders

- With the switch on: fights with power attacks show no lurch back. The log
  says once "Comfort: a stagger of the player was skipped" or "... knockback
  ... skipped".
- With the switch off: the vanilla stagger and knockback are back.
- Anything that still jolts (recoil, knockdown, spells) goes into the list
  above.
