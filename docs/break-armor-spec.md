# Breaking armor and bending weapons (idea 2026-11-11)

Status: **idea, nothing built.**

The tester (2026-11-11): "neue idee dynamisches breakarmor system. ich kann
gegner die rüstung weghauen schrittweise oder kauputt machen. das game hat
ja bereits ein zustands system. selbes für waffen also dynamische
verformungen."

## 1. What the engine already has (read on UESP)

- Every armor piece and weapon has a **health** (condition). "As the armor
  is used in combat its health will slowly degrade to 0. As its health
  decreases so will its armor rating. Once it reaches 0 the armor is
  considered broken and is unusable."
  (https://en.uesp.net/wiki/Oblivion_Mod:ObEdit/User_Interface/Armor)
- A piece's rating: `BaseArmorRating * (0.35 + 0.0065 * ArmorSkill) *
  ArmorHealth / MaxArmorHealth`; a weapon's damage carries
  `(WeaponHealth / BaseWeaponHealth + 1) / 2`; a weapon loses
  `0.06 * BaseWeaponDamage` health per strike; `fDamageToArmorPercentage`
  is 9 (https://en.uesp.net/wiki/Oblivion:The_Complete_Damage_Formula,
  https://en.uesp.net/wiki/Oblivion:Weapons).
- So **the numbers are there**: wear, the rating's fall and "broken" at 0.
  What the engine does not have is anything visible: a worn piece looks
  the same at 100 % and at 1 %.
- *Unverified:* whether the engine damages an NPC's armor health at all
  in vanilla (the player's does), and what "unusable" means for a broken
  piece an NPC still wears (unequipped, or worn with 0 rating). Both are a
  short read in the decompile or one harness run.

## 2. Options

| Option | What | Cost | Why |
| --- | --- | --- | --- |
| **A. Hit the piece, wear the piece (recommended first step)** | Where a VR strike lands (OBVR knows the body part from the contact: head, torso, arm, leg) it takes health off the armor piece on that slot, scaled by the strike's speed and the weapon's kind (blunt more, blade less on plate). Rating falls by the engine's own formula. | small to medium | The contact's body part and speed exist (strike rules, dismemberment's part). New: slot from part, the health write, the scaling. Fully testable by harness and log. Invisible by itself. |
| **B. Knock the piece off** | At 0 health, or on a hard blunt hit to a damaged piece, the piece comes off: unequipped and dropped as a world item that flies off along the blow. Helmets first. | medium | Reuses the engine's unequip and drop, OBVR's thrown-object physics and the hit's direction. Risk: NPC AI re-equipping it at once; worn sets with one model for several slots (cuirass with greaves). |
| C. Visible wear | The piece's look follows its health: dents, scratches, dirt through a texture swap or a blend between two textures per health step. | large | Needs art per armor (or a generic overlay shader), and the engine's per-reference texture control is an unknown. Mods add thousands of armors. |
| D. Weapons bend | A weapon's health falls with hard hits (blade on plate, edge on stone); low health shows as a bent blade (bones or a morph in the weapon model) and plays worse (less damage by the engine's formula, maybe a wobble on the swing). | large (the look), small (the numbers) | The number part is the engine's own (health per strike, damage by health). The look needs deformable weapon models, which the vanilla NIFs are not: a bend would be a bone or a swapped mesh per weapon. |
| E. Breaking a weapon | At 0 health a blade snaps: the weapon is swapped for a broken copy (the hilt) and the blade flies off. | medium to large | Needs a broken model per weapon type (or one generic hilt), and the item swap with enchantments, poisons and the player's hotkeys kept. |

## 3. Questions for the tester

- Should the player's own armor and weapons wear the same way (and so need
  more repairs), or only the enemies'?
- Knocked-off pieces: lootable afterwards (yes by default, they are the
  engine's items)?
- How fast: a few hard hits to break a helmet, or many?

## 4. Recommendation

**A first, then B for helmets.** A uses the engine's own numbers and makes
VR hits matter (a blow on the head hurts the helmet, not the boots); B
makes it visible where it counts most and reuses pieces OBVR already has.
C, D's look and E only after A and B have been played, and after a check
whether a generic overlay can carry the look (the cheapest step: one
texture blend tried on one armor).
