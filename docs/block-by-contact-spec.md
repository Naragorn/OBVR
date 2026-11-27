# Block by contact: decisions, what is built, what the engine does with a block

One place for the block's new rule and the decisions behind it (2026-11-27). The
details of each build stand where they were written:

- docs/free-weapons-spec.md, "The tester's Windows run of 2026-11-27" - each build with
  what was read, derived and guessed.
- docs/combat-comfort-spec.md - "How vanilla decides a block" and what is built on it.
- docs/next-up.md, "For later: the shield against its real mesh, and blows by real contact".
- `../OBNG/knowledge/blocking.md` and `../OBNG/knowledge/combat-ai-and-blocking.md` - the
  engine's side, with addresses.

Labels: [R] read (code, EXE, a log), [D] derived, [G] guessed.

**Nothing in this file has run in the game, the harness or a headset.** Every build has
unit tests; the harness scenarios named below are written and were never run (Windows has
no harness runs; Linux was not booted). Nadi's last log is from 2026-11-27 13:55, before
any of it.

## 1. The rule in one paragraph

Nothing holds the game's Block control from a guard pose any more. A blow at the player is
blocked when the thing the player holds is in the blow's way at the moment the engine
resolves it: OBVR answers the engine's block check for that one blow (the forced block,
game/Parry.cpp, the hit handler's call at 0x005FF7DF) and decides from where the shield,
the blade or the torch stands (game/BlockAngleLogic.h and beside it). So a raised guard
never walks the player, and a guard that is not in the way does not block. This is Blade &
Sorcery's rule: it has no block state, a strike is stopped by what it meets
(vrarwiki.com/wiki/Blade_%26_Sorcery: "There is no block or parry button; players deflect
enemy strikes").

## 2. Nadi's decisions, in order (2026-11-27)

| # | What was asked or reported | Decision | Built |
|---|---|---|---|
| 1 | "With the shield equipped I could not run" - OBVR held Block for the raised shield hand, vanilla walks a blocking player | First C (a stricter shield gesture), then, after hearing how Blade & Sorcery does it, **E: the shield blocks only what it is in the way of** ("go e") | f067265 |
| 2 | An arrow at the shield showed the body's impact | Arrows by the same rule ("go") | e598814 |
| 3 | The margins round the disc were guesses - "we can read meshes, adapt it per shield" | The shield's own outline, read from its mesh, for arrows | e3bdd8b |
| 4 | Triangles instead of the outline; the outline for blows | Both put off at first ("put the two points into the specs for later") | docs/next-up.md, 54d3a03 |
| 5 | "No, we build the outline for blows now too. Blocking in the game felt oddly too generous and partly a few frames delayed" (from the run with the OLD rule) | The outline for blows, a shield's own smaller reach, and the shield has to stand between | dba3279 |
| 6 | "How is blocking with a one- or two-hander?" - still the pose holding Block | The weapons the same way ("ja go") | e49cdf5 |
| 7 | "go same for fackel stab fäuste bogen" | The torch, the staff, the fists and the bow the same way | Torch 8e136fa (its fallback 184a4de), staff abfe4a0, bow e2785bf, fists bee61b4 |
| 8 | "How can we add the foes' AI and the wear afterwards?" - "go 1-3": play it first, the wear at once, the AI read only | The wear: read, **no patch needed** (section 4). The AI: read (section 5), **decision open** | eb5701f (spec), OBNG 5ec89a4 |

Open, Nadi's:

- **The AI's answer** (section 5): leave foes at their full attack rate, or halve it again
  for a foe the guard stands towards. Recommendation: leave, play first.
- **Aimed wear** (section 4): wear the shield or blade that blocked instead of vanilla's
  random armour piece. New behaviour, not a repair.

## 3. What decides a blow now

Settings, all in `[Hands]`, hot reloaded; the rows are under Combat in the settings:

| Setting | Default | Does |
|---|---|---|
| `ShieldBlockByContact` | 1 | The shield by contact. 0: the raised shield hand holds Block, as before |
| `BlockShieldReachUnits` | 25 | How near a foe's blade has to be to the shield's outline [G: from the weapon guards' tally, 10 to 37 units at the hit] |
| `BlockShieldInterposeUnits` | 6 | How far inside the outline's edge the line to the chest has to pass [G] |
| `WeaponBlockByContact` | 1 | Every guard but the shield by contact. 0: each pose holds Block, as before |
| `BlockShieldDegrees`, `BlockBladeDegrees`, `BlockReachUnits`, `BlockBodyDegrees`, `BlockInterposeUnits` | 70, 30, 40, 70, 25 | The angle rule's numbers since 2026-10-14, unchanged |

| Guard | Blocked when | Not while | Cannot judge |
|---|---|---|---|
| Shield, a foe with a blade | The face within 70 degrees of the blow, and the blade within 25 units of the shield's outline or the shield between the blade's middle and the chest | - | The game's cone while the hand is raised as before |
| Shield, a foe without a blade | The face within 70 degrees, and the shield between the foe's centre and the chest | - | The same |
| Shield, an arrow | The arrow's point on the outline (its half width 1.5 units [G]; 10 units along the normal [G]), or its flight crossing the outline | - | The same |
| Sword, axe, mace, one- or two-handed | The blades cross within 30 degrees of a right angle and within 40 units; without a blade on the foe, the blade across their line within 25 units of the line to the chest | The weapon hand swings (real contact then is the parry's) | The game's cone while the old guard pose is up |
| Torch | Its shaft, by the same rule | The torch hand swings | The game's cone while it is raised |
| Staff | Its line (the weapon node along its y axis to its bound's end [D: not seen in a log yet]), by the weapon's rule | The hand moves at swing speed | The game's cone while the old guard pose is up |
| Bow | Its limbs as a line, tip to tip (45 units each way [D]), by the weapon's rule | An arrow is nocked or drawn | The game's cone while it is held upright as before |
| Fists, a foe without a weapon | One forearm (elbow to knuckles) across their line, by the weapon's no-blade rule | That arm punches | The game's cone while both fists are up |
| Fists, a foe with a weapon, or an arrow | Never: the engine's hand-to-hand block stops nothing there, so the blow lands and is not called blocked | | |
| Parry (the player's blade met theirs in their attack) | The whole blow, 0.8 s, as before - asked first | | |

Order at the blow: the parry, then the shield, then the weapon hand's (blade, staff or bow),
then the torch, then the forearms; any one in
the way blocks. No outline read from a shield's mesh (fewer than 6 of its 24 rows filled):
the circle of its longer spoke, as before.

The shield's outline: 24 row spans in the shield's own plane, collected in the disc read's
second walk over the model's vertices (game/ShieldShape), read with the disc - when the
shield's node or bound changes, else every 90 frames; never per frame [R].

## 4. What a forced block gives, and what it does not

Read in the hit handler 0x005FEBF0 and the functions it calls, and in the projectile
impact 0x0060A640 [R unless marked]:

- **It gives:** the share blocked from the Block skill and Luck (0x005474A0), the block's
  impact effect and sound for the item (0x006AFFF0), the attacker's recoil (their call with
  the share). The path the parry and the torch used before; in the harness since
  2026-10-25 (torch-parry.txt, "blocked 1.00 of it").
- **The wear is not lost** (corrected 2026-11-27; the first reading said it was). The wear
  goes by the damage that lands, so a blocked blow wears less either way. The item is a
  random armour piece of the target's (0x005E5A00), never "the item that blocked". The
  second Block check in that branch (0x005FFC3F) asks the **attacker**, not the target -
  it never saw the struck player's block, held or forced [R: `mov ecx,edi`; that it is a
  slip of the original is derived].
- **It does not give:**
  - the walk while blocking (0x005FDBC6 in 0x005FCAB0 reads the action directly: Block and
    running become walking) - the point of the change;
  - the jump refused while in the Block action (0x006729F3) - gone with it;
  - the foes' halved attack chance against a blocking target (section 5);
  - the first-person block animation [G: not checked; the arms follow the controllers].
- **Inexact, known:**
  - The engine takes the blocking item from the equipped slots, the shield first. A blow
    the blade stopped while a shield is worn counts as a shield block (the larger share).
  - A fist's block is hand to hand to the engine: it stops nothing against a blade or an
    arrow (0x005FF890 `fldz`), as in vanilla. New against vanilla: such a blow at a fist's
    guard is now a plain hit (the same damage, no block reaction), and is not called
    blocked. A bare torch's block goes through the hand-to-hand stub the torch had before,
    which gives it the Block skill's share.
  - A staff and a bow count as the weapon item to the engine [D: from the slot calls].
- **Not read:** where the block perks and the block's fatigue cost are applied. If that is
  outside the hit handler, they are lost under the rule.

## 5. The foes' AI and a blocking target (read 2026-11-27, nothing built)

Full list: `../OBNG/knowledge/combat-ai-and-blocking.md` (27 call sites of 0x005E5670,
"the actor's current action is Block", found by scanning the code section).

- **Three sites ask about the target** [R], all in the decision to attack: 0x00620414
  (the melee decision), 0x00618B06 (the decision without a block option), 0x0061E6F0 (the
  attack while closing in). The answer becomes an argument of the attack-chance function
  0x00546800: the chance times the combat style's own multiplier (advanced field +0x48)
  or `fAIDefaultAttackDuringBlockMult`, 0.5 by default [R: the constructor].
- **So in vanilla a foe attacks a blocking target about half as often**; the rest of the
  roll goes to its own block or to holds of 0.5 to 1.5 s [R]. Nothing makes a foe
  power-attack, circle or feint because its target blocks [R: no other target-asking site].
- **Every other site asks the actor about itself** and only ends its own block (the combat
  controller's states, the package procedures), or is already answered by OBVR (the hit
  handler, the projectile impact).
- **The player has no combat controller** (vtable +0x330 returns 0), so the controller's
  "is blocking" mirror at +0x49 is never set for the player [R].
- **Before the change** [D]: with the hand up, every foe attacked half as often, whether
  the shield faced them or not. **Now:** always the full rate.

If it is to be answered: one shared call-site stub at the three sites - the asker is
`[esi+3Ch]`, the target in ecx; for the player it returns "the guard stands between this
foe and the player's chest", the test section 3 makes per blow, evaluated for that foe.
Cost: small (three call patches, one pure decision with tests, a harness scenario counting
a bandit's attack starts with the guard up and down). Model and effort: Sonnet 5.5 medium.
Risks: the halved rate stacks with OBVR's attack tempo and frequency settings, so guarded
fights may turn passive [G]; how often each decision runs is not read.

**Recommendation:** leave it, play the rule first. Blade & Sorcery has no such brake, and
the fight's pace is OBVR's own setting already. Build it if the foes feel too wild against
a standing guard.

## 6. Open work

- **Every guard is built** (the staff's line in game/StaffLine.cpp, the bow's in
  game/BowVisual.cpp with the bow's block moved out of HandMode::Update, the forearms' in
  game/ForearmLines.cpp). Three readings are derived and not yet seen in a log: that a
  staff's model lies along its node's y axis ("Hands: the staff's line - N units long,
  along x y z" settles it), the bow's 45 units to each tip, and the forearm bone's name.
  The frame a staff fires is not known to the block code, so it is not held off [G].
- **Owed:** the harness on Linux - shield-block-contact.txt, shield-block-arrow.txt,
  weapon-block-contact.txt, torch-block-contact.txt, staff-block-contact.txt (each: the
  guard up while the stick sprints, a blow with the guard in the way, one with it aside);
  bow-block-contact.txt and fist-block-contact.txt check the pose only (no unarmed foe is
  placed, so a blow at a forearm is untested) - and the headset. The log says at each blow
  which guard was asked, the angle, the distance and the verdict ("Block angle: ... at the
  shield / weapon / bow / torch by contact ...").
- **The controls card** says "Hold your shield or blade in a blow's way." and does not
  list the staff, the torch, the bow and the forearm.
- **Asked 2026-11-27, not built:** the shield's weight, as the weapons have theirs
  (docs/physical-combat-spec.md, "The shield's weight").
- **For later** (docs/next-up.md): a flight against the shield's triangles; the foe's blade
  followed over the whole blow, so the block comes when the blades meet and not 10 to 37
  units early; the parry against the shield's plate instead of a ball.
- **Every number in section 3 marked [G]** waits for the first headset log.
