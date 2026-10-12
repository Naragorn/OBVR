# Free weapons - pick up, hold anywhere, drop, holster (feasibility, 2026-09-30)

The tester, 2026-09-30: "recherchier jz mal und erstelle eine spec ob es möglich wäre waffen komplett unlocked zu machen. soll heissen ich sehe in der spielwelt eine waffe nehme sie am griff auf -> sie ist ausgerüstet im hintergrund ... ich kann sie mit links und oder rechts beliebig anfassen wie ich will und zuschlagen wie ich will. wenn ich die waffe loslasse fällt sie einfach auf den boden. sogar mit half swording oder so. alles was nicht griff ist zählt dann als schadensfläche. wenn ich sie in den slot stecke (zB schulter rechts) ist sie dort geholstered. also Blade&Sorcery style. machbarkeitsstudie erstmal."

**Status: feasibility study (2026-09-30); wanted by the tester since 2026-10-12, see section 4. Nothing here is built.** It builds on `docs/physical-combat-spec.md` (the B&S research of the same day) and does not repeat it. The new parts are:
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

## 4. Wanted by the tester (2026-10-12) - to build, nothing built yet

The tester, 2026-10-12: "wir müssen noch die linke hand am waffengreiffen verbessern. bisher braucht es ein festes grip bis die sich an der waffe einklinkt. das soll auch mit weniger druck möglich sein sodass man nicht "press" machen muss sondern einfach nur druck. außerdem waffe rechte hand ist wenn option an ist (default an) nicht mehr locked an der waffe. sodass man die jederzeit werfen oder droppen kann. andere wafffen am boden oder so rüstet man automatisch aus wenn man die am handle zu sich nimmt. also wie in blade and sorcery. und man kann waffen auch half sword mässig halten usw."

Four requirements, each against what exists today [R] and what this study found (section 2). The legend of the study applies.

### 4.1 The second hand takes the handle by the grip's force, not by a click

- **Today [R]:** the take is the grip's click: `vr::TwoHandTakes` with `leftGripClosedNow` (CameraHook.cpp:7042), and `vr::GripDown` reads the grip's pressed bit (HandInput.h:58). With the action manifest installed - the live path once the action set loads (OpenVRBackend.cpp:1061) - that bit is the boolean action `/actions/obvr/in/left_grip` (assets/input/actions.json:12), and the shipped Index binding makes it from the force sensor: `"force_input": "force"`, the click at a force of 0.3, released at 0.2 (assets/input/knuckles.json:21 and :27, `click_activate_threshold` / `click_deactivate_threshold`). That 0.3 is the "festes grip". Whether the tester's SteamVR runs the shipped binding or one edited in its binding UI cannot be read here.
- **The force itself is not read on that path [R]:** `HandPose::gripForce` is filled from the legacy controller state only (`state.axis[2].x`, OpenVRBackend.cpp:1156); the action path never sets it (`ReadControls` zeroes it, ControllerActions.h:222, and `ApplyActionControls` copies the buttons, the trigger and the sticks, ControllerActions.h:288-298). Its only use, the hand HUD's "open hand" when no curl reads (CameraHook.cpp:8856), and the "Hands: ... grip=" log line (CameraHook.cpp:3728) see 0 there.
- **Wanted:** the hand near the handle takes it at a light squeeze - pressure, not a press.
- **Two ways [D]:**
  1. Lower the binding's thresholds in knuckles.json. One line, but it lowers every grip: the grabs, the holsters, the bow's draw by grip (HandMode.cpp:471), the stow. Not recommended on its own.
  2. A vector1 action for the grip's force, read into `gripForce` on the action path, and the take decided by OBVR with its own thresholds: take at `TwoHandTakeForce` (a setting; a first guess 0.10-0.15, to be set from the log), hold while the force stays above a lower release value (hysteresis, as the binding's 0.3/0.2 does), the click as the fallback where no force reads (`gripForce` 0). The Index offers the force on `/input/grip` [R, the binding above]; what the Touch and other controllers offer there (a pull value, no force sensor) is to be checked in SteamVR's binding UI [G]. The fingers' curl from the skeleton (`SkeletalSummary`, read today) could stand in on a controller without a force sensor [G].
- **Open:** what force a light squeeze reports on the Index (measure: log `gripForce` each frame while the hand is near the handle - the mask line logs it only when a button changes); whether the preshape's range (controls-spec 4.10, from 18 units) is the right "near enough" for the lower threshold, so that the shaping and the take are one motion.
- **Tests:** two_hand_test for the thresholds, the hysteresis and the fallback to the click; ControllerActionsTest for the new action; ControllerActionAssetsTest.py for the binding files.

### 4.2 The weapon hand's weapon is not locked - an option, default on

- **Today [R]:** the drawn weapon hangs on the Weapon node under the right hand bone (section 2.2); nothing lets it go but the sheathe at a holster (controls-spec 4.1) and the inventory. The weapon hand's grip away from a holster is the grab (HandInput.h:775), at the hip the holster's.
- **Wanted:** with the option on, the weapon is held only while the weapon hand's grip holds it. Opening the grip lets it go: it falls where the hand was, or flies with the hand's speed (section 2.5: unequip, drop with the velocity; the drop call's address is still to be found). Blade & Sorcery's grip. Option off: today's locked weapon.
- **Implied, to confirm with the tester [D]:** the grip is then held through the whole fight. A grip read open for less than `TwoHandReleaseSeconds` (0.12 s, the flicker guard of controls-spec 4.10) must not drop it; with 4.1's force action the drop can ask for the force to stay under a low value that long. The sheathe by reaching stays the other way to put the weapon away.
- **What goes with it [R, D]:** the blade's stoppers and contacts (controls-spec 4.32 and the sections after it) and the weight spring (physical-combat-spec section 6) act on the weapon hand's pin; a thrown weapon leaves them. The strike (0x005FEBF0) needs the equipped weapon: a thrown weapon hits as a thrown object does (ThrowLogic: a stagger from 4 m/s, down from 7 m/s), not with its own damage - section 2.4's limit.
- **Open:** the drop call (section 2.5); how long the hand is without a weapon after the unequip; the dropped weapon's ownership and the NPC reaction; what the empty weapon hand does next (the fists ready, or nothing).

### 4.3 A weapon in the world, taken at its handle, is equipped

- **Today [R]:** a weapon ref grabbed by hand is a held object (GrabPhysics / HeldObject, docs/holding-objects-spec.md); it is equipped from the inventory, the quick menu or a holster's remembered weapon; the stow at the body puts a held thing into the inventory (controls-spec, the stow).
- **Wanted:** closing the grip on the handle of a weapon lying or standing in the world takes it into the inventory, equips it, and draws it into the hand at once, held where it was grabbed (sections 2.1 and 2.2). Blade & Sorcery's way. Grabbed off the handle, on the blade: a held object as today, or refused - the tester's choice; B&S holds it anywhere.
- **Pieces [D]:** the handle test on a world ref (its model's axial extent, `game::AxialExtentOf`, measured today for the drawn weapon only - the ref's own node instead); take, equip and draw as one flow with the draw at `WeaponDrawSpeed` (section 2.1); the hand's hold at the grabbed point (section 2.2, the Weapon node's transform). With 4.2 on, the hand that lets go drops it again - a weapon can be picked up, looked at, and dropped.
- **Risks (section 2.1):** the stack `EquipItem` picks; the theft rules through Activate; the frames without a weapon.

### 4.4 Half-swording, and holds anywhere on the weapon

- **Today [R]:** the second hand takes a handle below the weapon hand only (`vr::TwoHandTakes`: above the right hand is the blade; a staff's shaft is the exception); the weapon's pose against the weapon hand is the animation's.
- **Wanted:** both hands anywhere on the weapon, the blade included - the left hand on the blade for a half-sword thrust, the right hand moved along the handle, a mace held by its head. Section 2.2's "4.10 extended to a second hand anywhere", and the weapon posed against the hands where they hold it.
- **Open:** the damage of a half-sword thrust (per-surface damage is blocked, section 2.4: the strike takes no location); the own blade against the own hand - the stoppers stop the free hand at the own blade today (controls-spec, "the blade stops at your shield and the free hand at your blade", ae95ed7), and a hand that holds the blade has to pass that stopper.

### 4.5 Order

1. 4.1, the force take: small, on its own, the first thing to feel.
2. Section 3's step 1: grab from the world by the handle = equipped (4.3).
3. Section 3's step 2: let go = it falls (4.2), after the drop call research.
4. Section 3's step 3: held where grabbed, the second hand anywhere, half-swording (4.4).
