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

## Also open

- **Grab reliability.** Cause found in the disassembly: the grab's start
  casts its own ray from the first-person camera along the player's rotation
  and takes what that ray hits (GameAddresses.h, kCallGrabHandler); the head-
  to-hand line passed beside the object into the table or floor. The start now
  looks at the point the laser's pick hit. Headset confirmation open; the log
  line "the engine TOOK / did NOT take it" is the measure.
- **Shield bash.** Not a vanilla action (UESP, Oblivion:Block: the Expert
  and Master perks give a chance of a stagger or disarm on a block; a manual
  bash exists only in Oblivion Remastered). A VR bash would be OBVR's own:
  the shield hand driven into an NPC, the engine's knockback applied.
- **New games.** The walkthrough and the hands' guide around the intro and
  the character creation: not yet tested.
- **Stagger in combat (reminder to test).** `[Look] NoPlayerStagger` (on by
  default) skips the player's stagger (0x005F4FD0, both call sites) and a
  hit's knockback (the character-proxy fetch at 0x0060008A). To test in the
  headset: fights with power attacks, with the option on (no lurch) and off
  (vanilla stagger back); whether any lurch remains from another path (the
  recoil 0x005F4F00 when the player's own attack is blocked is not touched,
  nor a knockdown).
- **Holding objects for real.** docs/holding-objects-spec.md: parts 1 (grip
  pose), 2 (touched point in the palm) and 4 (small objects fixed in the hand)
  built 2026-09-26; 3 (rotation for large objects) and 5 (grips by size) open.
- **Block and stagger.** docs/combat-comfort-spec.md holds the problems and
  every proposal.
