# Free weapons - pick up, hold anywhere, drop, holster (feasibility, 2026-09-30)

The tester, 2026-09-30: "recherchier jz mal und erstelle eine spec ob es möglich wäre waffen komplett unlocked zu machen. soll heissen ich sehe in der spielwelt eine waffe nehme sie am griff auf -> sie ist ausgerüstet im hintergrund ... ich kann sie mit links und oder rechts beliebig anfassen wie ich will und zuschlagen wie ich will. wenn ich die waffe loslasse fällt sie einfach auf den boden. sogar mit half swording oder so. alles was nicht griff ist zählt dann als schadensfläche. wenn ich sie in den slot stecke (zB schulter rechts) ist sie dort geholstered. also Blade&Sorcery style. machbarkeitsstudie erstmal."

**Status: feasibility study only. Nothing here is built.** It builds on `docs/physical-combat-spec.md` (the B&S research of the same day) and does not repeat it. The new parts are:
- releasing a weapon drops it;
- holster slots for picked-up weapons;
- the damage surface as "everything but the handle".

Legend used throughout:
- **[R]** read in the repository or at the cited URL.
- **[D]** derived from what was read.
- **[G]** guessed, not checked.

No engine address was disassembled for this study. Every address below comes from a comment in the repository or from xOBSE.

## 1. The model: one weapon, two states

Every Bethesda-engine precedent keeps a weapon in one of two states, a **world item** or the **equipped weapon**, and converts between them. None makes one object both [D]:
- **Swap Drop and Hold** (Skyrim VR): "instantly equip a weapon you're holding… instantly unequip and drop a weapon that you have equipped" [R] (https://www.nexusmods.com/skyrimspecialedition/mods/49425).
- **Weapon Throw VR**: "Auto-Equip Picked Up option equips the thrown weapons immediately after you pick them up". In 1.1: "Thrown weapon is now recognized as came from the player… perks… experience" [R] (https://www.nexusmods.com/skyrimspecialedition/mods/31374).
- **Heisenberg** (Fallout 4 VR): "Allows pickup and equipping of weapons by dropping them on your weapon hand" and "Enables holstering of picked up weapons with Virtual Holsters" [R] (https://www.nexusmods.com/fallout4/mods/99105).
- **HIGGS** (Skyrim VR): "there is no restriction on where on the weapon you can grab" [R] (https://www.nexusmods.com/skyrimspecialedition/mods/43930). A player notes that a weapon has to be equipped to be held with both hands: "you can't just pick a weapon up off the floor and double grip it" [R] (reddit r/skyrimvr, thread w9vxfs).

**Recommendation: the weapon in the hand is always the equipped one.**
- Only the equipped weapon strikes. OBVR's strike, `0x005FEBF0`, resolves everything from it: damage with weapon, skill and power attack, the block, the sneak attack, the enchantment, the crime, OnHit and OnHitWith [R] (GameAddresses.h:1277-1311).
- Kept this way, skill XP, enchantments, poisons, perks, encumbrance, the inventory and saves stay the engine's own [D].
- A world object dealing damage itself would have to redo all of that. It is not recommended.

The flow the tester wants then becomes:

| Player does | Engine state change |
| --- | --- |
| grabs a weapon in the world by its handle | take it into the inventory (with the theft rules), equip it, drawn |
| holds, turns, two hands, half-sword | the drawn weapon posed freely against the hands |
| lets go of it | unequip, and drop it into the world where the hand was, with the hand's speed |
| puts it at a body slot | sheathe (stays equipped) |

## 2. The pieces

### 2.1 Grab from the world = equipped. Feasible, S-M.

- **Exists [R]:**
  - Grabbing world refs by hand (GrabPhysics).
  - `game::TakeIntoInventory` (src/game/TakeItem.cpp:42-83): `TESObjectREFR::Activate` at 0x004DD260 with (player, 0, 0, 1), as the activate control does. The player's pickup at 0x00660910 hands owned items to the crime code (GameAddresses.h:1543-1560). The stolen red-hand icon was seen (controls-spec 4.6). A theft with a witness is untested.
  - `game::EquipWeaponForm` (MeleeHits.cpp:306-317): `Actor::EquipItem` at 0x005FAEA0, silent (controls-spec 4.1).
  - The draw at 10x speed (`WeaponDrawSpeed`, src/game/WeaponDrawSpeed.h).
- **New:**
  - The handle test when the grip closes: the touched point projected onto the weapon's axis, between its origin and the pommel end (`AxialExtentOf`, as in controls-spec 4.10).
  - Take, equip and draw chained as one flow. The take has to wait until the engine's grab has let go of the ref, which 4.6 already does with a 1 s timeout.
- **Risks:**
  - `EquipItem` with no extra data "picks the stack itself" [R] (GameAddresses.h:1520-1522). With two stacks of the same form, it could equip a different one (other health, poison or enchantment) than the one just picked up.
  - The draw is 10x faster but still an animation [R]. A truly instant equip: **could not verify**.
- **Settles it:** a harness run logging the equipped stack's extra data against the picked ref, and how many frames the hand is empty.

### 2.2 Held anywhere, both hands, half-swording. Visually feasible, M.

- **Exists [R]:**
  - The hands pinned to the controllers. The weapon hangs on the "Weapon" node under the right hand bone and follows it as a child (HandBones.cpp:240-249; HandGrip.h:290-293). OBVR never writes the Weapon node itself.
  - The two-hand grip (controls-spec 4.10): the left hand moves onto the weapon, keeps its own turn round the handle, and is shaped on the way in.
  - Held world objects are posed by writing their world transform directly (HeldObject.cpp:121-125).
- **New:**
  - The weapon placed against the grabbing hand where it was grabbed: an offset along its axis, and a turn round the grip. Write the Weapon node's local transform each frame after the hand pin, or move the hand along the weapon instead.
  - 4.10 extended to one-handed weapons and to a second hand anywhere, including above the right hand (half-swording: the left hand on the blade).
  - Which hand leads when both hold: the first to grab, or the dominant one, as 4.10 now does.
- **Unknown:** whether the game's animation rewrites the Weapon node's local transform each frame [G]. The finger links are rewritten every frame (holding-objects-spec, part 1b), so it is likely. Then OBVR writes it after the animation, as it does for the hands.
  - Settles it: a harness run that writes it once and logs whether it held the next frame.

### 2.3 In the left hand only. Uncertain, L.

- The engine has one weapon slot, and the left hand's slots are shield and torch [R] (controls-spec 4.1; physical-combat-spec §3E). Dual wielding is not possible.
- The way to try [D]:
  - Write the Weapon node's world transform onto the left palm each frame, the held-object method.
  - Show the right hand empty.
  - Keep the node's bound in view (`KeepFirstPersonNodesInView` exists for the Hand nodes).
  - Switch the strike's hand gate and `BladeInWorld` to the left pose. The strike takes its blade from the hand's pose plus the weapon's reach, not from the node (MeleeHits.cpp:427-428) [R].
- Other routes, re-parenting the node or a mirrored skeleton, are in left-handed-spec §2.1. None has been tried.
- Unknowns: third person (the body's weapon is a separate node), the sheathe animation, and the strike code's assumptions about the right hand.

### 2.4 Everything but the handle does damage. Partly feasible, M-L.

- **Where it touched:** way 1 of physical-combat-spec F2 (skeleton capsules with a swept blade) gives the contact point along the weapon and the body part. Feasible [R, proposal].
- **Not the handle:** trivial once the contact point is known. The handle span is the one 4.10 measures [D].
- **What kind of hit:** B&S splits it into damagers, "Blunt, Pierce and Slashing… a dagger can use a blunt damager for the handle", and advises "add blunt damagers to your handles" [R] (https://kospy.github.io/BasSDK/Components/ThunderRoad/Items/Damager.html, …/Guides/Items/ItemsBestPractises.html).
  - Oblivion's model has no such labels [D]. The edge could be found from the blade's cross-section: its longest spread across the axis is the flat's plane, and the edge lies across it. That is an `AxialExtentOf` along the node's x and z [D; not measured].
- **Blocked: different damage for edge, flat, pommel or a half-sword thrust.**
  - `0x005FEBF0` takes no location and no multiplier. Its only lever is the first-argument byte for a power attack [R].
  - Where to scale damage is physical-combat-spec §3B's open question.
  - A pommel strike as a blunt hit, or as hand-to-hand while a blade is equipped: **could not verify**.
- **Buildable now:** a hit from anywhere but the handle, at the full or the power attack's damage.

### 2.5 Let go = it falls. Feasible if the drop call works, M.

- **Exists [R]:**
  - Throwing a held ref with the hand's speed (GrabPhysics). Thrown things stagger or knock people down (ThrowLogic).
  - Swords dropped with `player.drop` do get a Havok body some frames later: they were grabbed and pushed by a hand in the harness (hand-script-harness.md:150, PASS).
- **New:** unequip (0x005F2E70, as `UnequipWeapon`), then drop into the world at the hand's pose with its velocity.
  - xOBSE declares `TESObjectREFR::RemoveItem(form, extraList, qty, useContainerOwnership, drop, destRef, float* dropPos, float* dropRot, …)` as virtual 0x40 [R] (xOBSE GameObjects.h).
  - Its address in 1.2.0.416, how the new ref comes back, and whether it has a body at once: **could not verify**.
  - The disarm (0x005FC090, combat-comfort-spec) drops a weapon into the world; its drop call was not traced.
- **Settles it:** disassemble the vtable slot +0x100 and the console's Drop command, then a harness run: drop at a pose, log the new ref, its body, and the velocity set.
- **Ownership [G]:** the dropped item becomes an unowned ref in the world, the engine's normal behaviour.

### 2.6 Put it at a slot = holstered. Exists in part, S-M.

- **Exists [R]:**
  - The places: one-hander at the hip, two-hander at the shoulder, bow at the other shoulder, each on its chosen side (left-handed-spec §4).
  - Draw and sheathe by reaching.
  - A remembered weapon per kind, equipped again from the inventory.
- **Holstered means sheathed, still equipped.** The game shows only the equipped weapon on the body. A second weapon at another slot is in the inventory, remembered, not visible [R] (controls-spec 4.1).
- A slot that stays visibly filled for every weapon would need scene nodes of OBVR's own [G, L]. Not proposed now.

### 2.7 The rest of the game. Mostly free, because the weapon is equipped.

- Inventory, encumbrance and saves are the engine's own [D].
- Theft goes through Activate, as today. A witnessed theft is untested.
- NPCs reacting to a drawn weapon: **not researched**.

## 3. Verdict and order

| Piece | Verdict | Size |
| --- | --- | --- |
| Grab from the world = equipped | feasible | S-M |
| Hold anywhere, two hands, half-sword | feasible to see | M |
| Left hand only | uncertain | L |
| Everything but the handle damages | yes; per-surface damage blocked | M-L |
| Let go = falls | feasible if the drop call works | M |
| Slot = holster | exists in part | S-M |

Proposed order, each piece testable on its own:
1. Grab from the world by the handle: equipped and drawn, and the frames without a weapon measured.
2. Let go: unequip and drop with the hand's speed, after the `RemoveItem` / Drop research.
3. The weapon held where it was grabbed (the Weapon node's transform), and 4.10 for any second grip, half-swording included.
4. Damage from the contact point (physical-combat-spec F2 way 1), handle excluded.
5. The left hand alone, as an experiment.
6. Damage by kind (edge, flat, pommel), once a place to scale the damage is found.

Research not done for this study: disassembly of the drop and equip paths, NPC reactions, and a live check of the Weapon node's per-frame ownership. deepwiki could not be reached (connection timeout).
