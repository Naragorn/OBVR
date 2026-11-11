# Breaking armor and bending weapons (idea 2026-11-11)

Status: **A and B built 2026-11-11** (section 5); C, D's look and E not.

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

## 3. Decided by the tester (2026-11-11)

The tester: "1. eigene auch 2. ne 3. vanilla formel verwenden mit
modifikator in obvr settings ich kann zb auch 1-10x schneller machen um
intensiveren combat zu haben".

- **The player's own armor and weapons wear too**, by the same rule as the
  enemies'.
- **Knocked-off pieces are not lootable.** A piece knocked off flies off
  and is gone (or lies as a broken, unlootable prop until the cell
  unloads - which of the two is decided when B is built, by what the
  engine allows without leaving items behind).
- **The rate is the engine's own formula** (the per-strike loss of
  section 1), times a multiplier in OBVR's settings, `1` = vanilla, up to
  `10` for harder, faster wear. One multiplier for armor and one for
  weapons, each a row in the settings menu (Combat: tuning).
- **Every piece at 0 comes off, not only helmets** (the tester, later the
  same day): every armor slot, shields and weapons. An enemy's flies off and
  is gone. The player's by `[Hands] PlayerGearBreaksOff`: `0` as the engine
  does it, `1` knocked off and gone like an enemy's, `2` (the default)
  unequipped into the inventory, broken and repairable, no flight.

## 4. Recommendation

**A first, then B for helmets.** A uses the engine's own numbers and makes
VR hits matter (a blow on the head hurts the helmet, not the boots); B
makes it visible where it counts most and reuses pieces OBVR already has.
C, D's look and E only after A and B have been played, and after a check
whether a generic overlay can carry the look (the cheapest step: one
texture blend tried on one armor).

## 5. Built (2026-11-11)

### What the engine does, read in Oblivion.exe 1.2.0.416

- Every hit in the attack handling (0x005FEBF0) wears **the attacker's
  weapon** by its base damage times `fDamageToWeaponPercentage` (0x00547240;
  the setting's value at 0x00B36E10, 0.06 as UESP gives it) and **one armor
  piece of the struck one** (0x00547260, `fDamageToArmorPercentage` at
  0x00B36E18). Both settings are read only there (their three references).
- The piece is **a random worn one**, weighted by slot (0x005E5A00: head,
  upper body, lower body, hands, feet, then the shield; called at
  0x005FFC1B). NPCs' armor wears the same way as the player's - the vanilla
  engine does wear an NPC's armor.
- The wear lands in the actor's item damage (vtable +0x2C4 -> 0x005F3870
  for Character and Creature; the player's 0x0065FF10 skips it in god mode
  and calls it): heavy/light armor perks scale it, the health falls by the
  amount, under 1 it is 0, the armor rating is recomputed (+0x2C0).
- **At 0:** a drawn weapon or a shield (biped slot 13) is **dropped into the
  world by an NPC** (vtable +0x2C8, a lootable item; 0x004DC000 records it
  as the actor's dropped item) unless it is a quest item, and **unequipped by
  the player** (0x005F2E70). **Every other armor piece stays worn** with no
  rating, NPC or player.
- A VR strike is the engine's own hit (game/MeleeHits.cpp ApplyStrike): a
  drawn blade's strike wears the player's weapon there; a blunt blow struck
  as the engine's fist (KickStrike: kick, shield bash, pommel, hilt) wears
  no weapon, as the engine's fist does not.

### What OBVR does (game/GearWear.cpp, game/GearWearLogic.h)

- **The multipliers** (`ArmorWearMult`, `WeaponWearMult`, 1..10) scale the
  two settings' values each frame (`StepGmstScale`): the base is whatever the
  setting held when it was last not OBVR's own write, so the game's data, a
  mod or a console `setgs` stays the base and the scale never compounds. It
  applies to every hit in the game, the player's and everyone's.
- **The struck piece:** the picker's call at 0x005FFC1B is rerouted; inside
  a VR strike whose blade touched a capsule (the part from
  `NearestTouchedPart`) the piece is the one over that part
  (`ArmorSlotsForPart`: head the helmet; neck the helmet, else the cuirass;
  an arm the gauntlets, else the cuirass; a leg the greaves, else the boots;
  the torso the cuirass). No piece there: no armor wear from that strike. A
  blunt fist-blow (no capsule part) and every other hit keep the engine's
  random piece.
- **At 0** (the item damage slot rerouted for Character, Creature and the
  player; `ZeroHealthAction`): an NPC's piece the engine dropped is not
  recorded as its dropped item and is sent flying; an NPC's piece the engine
  leaves worn is dropped by OBVR (the engine's own drop, +0x2C8) and sent
  flying. The flight: the piece's Havok body sent along the blow and up
  (`KnockOffVelocity`) and spun; after `GearKnockFlightSeconds` (2.5) it is
  disabled and deleted in one queued console batch (xOBSE
  `DeleteReference`, a dropped reference is a dynamic one) - **never left as a lootable item**; it can
  only be picked up during its flight. The player: mode 0 leaves it to the
  engine, mode 1 drops and knocks it off the same way (a weapon leaves the
  hand), mode 2 unequips it (worn armor; a weapon or shield the engine
  already unequipped is left in the inventory). Quest items are left alone.
- **AI re-equip:** a knocked-off piece is out of the NPC's inventory, so it
  cannot be re-equipped; the deletion takes it before an NPC could pick it
  up off the ground (unless within the flight).

### Tests

- `gear_wear_test` (tests/GearWearTest.cpp): the slots per part, the
  multiplier's clamp, the setting's scale (no compounding, a foreign change
  taken as the new base), the three player modes against worn / carried /
  engine-dropped, NPC every case, the hard blunt blow's threshold, the
  flight's send / disable / delete and a body that never comes.
- Harness, Linux headless 2026-11-11, all PASS:
  - `wear-knock-npc.txt`: the beggar's Dwarven Cuirass at 1 health, x10 -
    "the strike on ...'s right arm wears its cuirass" (an arm with no
    gauntlets: the cuirass over it), "takes 3.50 (armor wear x10.0), now
    0.0", "at 0 - knocked off", "the knocked-off ... taken away after 2.5 s"
    (its body came and flew).
  - `wear-vanilla-rate.txt` (x1) against the same run at x10 with the
    cuirass at full health: the first slash's wear 1.75 against 17.52 - ten
    times.
  - `wear-weapon-rate.txt`: the player's drawn Elven Longsword takes 7.20 a
    strike at x10, 0.72 at x1.
  - The settings' base followed the game's data and the Linux mod list: the
    armor setting read 0.5, 9, then 6 (a mod's), the weapon's 0.01, 0.06,
    then 0.04 - each scaled from its own base, never compounded.
  - Checked against origin/main (2d878b7): `strike-at-touch.txt` (the
    blade "sliced out ... past the turn cap") and `strike-skills-hands.txt`
    ("taken as a hit" missing) fail the same there - not this change.

### Open

- **Open bug - the hard blunt blow on a damaged helmet** (B's first idea):
  `HelmetKnockedByBlow` is decided and tested, but not wired: it needs the
  piece's full health (TESHealthForm in the armor form), not yet read. With
  every piece knocked off at 0, a helmet comes off by wear alone.
- **Open bug - the blunt fist-blows have no part:** a kick, a shield bash,
  a pommel or the hilt wear the engine's random piece; their contact point
  could give the part the way the blade's does.
- **Unchecked: the player's piece at 0** in each mode. Measured: xOBSE's
  `player.SetEquippedCurrentHealth 1 9` does not reach the drawn sword (its
  health stayed 395), and the player's armor wears only when an NPC hits the
  player. An NPC's weapon at 0 (it needs an NPC attacking), creatures,
  Windows and the headset are unchecked too.
