# Free weapons - pick up, hold anywhere, drop, holster (feasibility, 2026-09-30)

The tester, 2026-09-30: "recherchier jz mal und erstelle eine spec ob es möglich wäre waffen komplett unlocked zu machen. soll heissen ich sehe in der spielwelt eine waffe nehme sie am griff auf -> sie ist ausgerüstet im hintergrund ... ich kann sie mit links und oder rechts beliebig anfassen wie ich will und zuschlagen wie ich will. wenn ich die waffe loslasse fällt sie einfach auf den boden. sogar mit half swording oder so. alles was nicht griff ist zählt dann als schadensfläche. wenn ich sie in den slot stecke (zB schulter rechts) ist sie dort geholstered. also Blade&Sorcery style. machbarkeitsstudie erstmal."

**Status: feasibility study (2026-09-30); wanted by the tester since 2026-10-12 (section 4); the requirements settled with the tester 2026-10-23 (section 5). Built so far: the grip trace instrument of section 5.4 alone, nothing of the feature.** It builds on `docs/physical-combat-spec.md` (the B&S research of the same day) and does not repeat it. The new parts are:
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
  - Its address in 1.2.0.416, how the new ref comes back, and whether it has a body at once: **could not verify**. VERIFIED 2026-10-24 (REA on Oblivion.exe, xOBSE Hooks_Gameplay.cpp as the map): RemoveItem's body is 0x004D8760 (`ret 0x28`, ten arguments as xOBSE declares them); it takes the item out of the container (0x004D6D40) and hands everything to 0x0048FBC0, which with the drop flag creates the world reference through 0x0048B080 and 0x0044A7D0 (a plain TESObjectREFR for an item, 0x004D9A70) and returns it in eax. dropPos is copied into the new reference's position as given; without it the engine puts the item 50 units ahead and 30 up of the container, by its rotation; dropRot given or the container's own. The model and the Havok body are not made there: the cell queues a create task, and the body is there 2 to 3 frames later in the harness (weapon-drop.txt). One reference per stack with the count as an extra. The console's Drop (0x00500650) calls the slot as (item, 0, count, 0, 1, 0, 0, 0, 1, 0).
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

## 5. Requirements settled with the tester (2026-10-23)

The tester, 2026-10-23: "weapons no longer have to select in inventory to equip them. instead just grab any weapon by the handle and its equipped. let go and it falls in the world. stow it on the side/shoulder (depending if 1 hand, 2 hand, bow, etc) and its in inventory. if a weapon is alrdy there then thats not possible. players then have to go to inventory and unequip that slot first." Settled in the session that followed, decision by decision; the legend of the study applies. Sections 2 and 4 keep the pieces and the earlier wishes; where they differ from this section, this section rules.

### 5.1 Decisions

| | Question | Decision | Cost | Why |
| --- | --- | --- | --- | --- |
| 1 | Where the grip is | The vanilla hold point: one point per weapon, where the game's own animation holds it when equipped ("der griff ist immer da wo der player in vanilla die waffe standardmässig hält wenn sie ausgerüstet ist"). Measured so far (vr/TwoHandLogic.h): the sword's palm 0.8 units below the Weapon node, the two-hander's right hand at the node, the staff 37.6 units up the shaft; the bow not measured. | cheap to small | one point per kind from the model, as `HandleSpanFor` reads the models today; the bow's point is a harness measurement |
| 2 | The pick-up ring | On the grip of every weapon within reach, always; the ring elsewhere on the weapon (the blade) only with the hand close and precisely there, and that grab is a carry (the held object of today), not an equip. B&S gives its handles the priority by an "Artificial Distance" (the "Half Swording and Support Grip" mod, Nexus 10151, names it); vanilla B&S has not grabbed blades since U8 (reddit, "Half-Swording", 2020). | small | `game::NearbyItems` aims the pick between the middle and the nearest vertex today (`NearSideWeight`); a weapon ref gets its grip point as the preferred pick, the precise pick stays |
| 3 | Hold or toggle | Hold: the weapon is in the hand while the grip holds it. Toggle an option. | small | section 4.2 said the same; the hold's flicker guard is a release time |
| 4 | What closes a grip | A realistic hold, not the shipped binding's force click: the holsters first, then every weapon grip (the world weapon, the second hand, the drop); the signal and its thresholds from the measurement of 5.4, the click as the fallback where nothing analog reads. | small after the measurement | the thresholds are settings with hysteresis, 4.1's way 2 |
| 5 | Model A or B | A: only the equipped weapon hangs on the body, the game's own; the holsters remember the last weapon of each kind as today (controls-spec 4.1). B - every holster visibly filled, OBVR's own scene nodes on the body, the stow state in the save - noted for later ("die waffe an meinen schultern sehe ich ja eh nicht bzw würde sogar nerven"). | A: the feature's medium; B: large on top | section 2.6 |
| 6 | A holster already holding a weapon | With A the stow replaces: the holster's weapon is the last of its kind stowed or equipped, the one before stays in the inventory as a plain item - today's rule. The refusal in the tester's first wording belongs to B: there the holster's weapon stays in sight, and freeing it is grabbing it out, as in B&S. With A the hip looks empty once the engine's one slot has moved to the new weapon, a refusal there would be a wall with nothing to see, and "unequip that slot in the inventory" has nothing to unequip - the holster's weapon is not the equipped one. | cheap | the engine's single weapon slot (section 1) |
| 7 | What counts as a weapon | The six kinds first (`EquippedKind`: blades and blunts one- and two-handed, staff, bow). Shield and torch next: the off-hand holsters them at ANY holster zone and takes them back from there ("schild oder fackel soll ich aber auch holstern können an beliebiger holster"); built 2026-10-24 at the one-hander's and the two-hander's place, the bow's place kept for the bow (5.4). Arrows never. | the six: in the feature's medium; shield and torch: small to medium | shield and torch need the off-hand's holster memory, the unequip call for their slots (not found yet; `UnequipWeapon` 0x005F2E70 is the weapon's) and a reading of what vanilla shows for a sheathed shield; under A they are as invisible in a holster as a second weapon is |
| 8 | The other hand | Carries weapons as loose objects, as today; never equips. Equipping into the off-hand noted for later ("wäre schon cool wenn das auch ginge"). | later: large | section 2.3: the engine's one weapon slot is the right hand's; the Weapon node written onto the left palm each frame, the strike's hand gate switched, third person, the sheathe animation and the strike code's assumptions all unknown; LeftHandMirror (2026-10-20) mirrors the whole skeleton for left-handers and is no answer for one grab |
| 9 | Someone else's weapon | The game's rules through Activate, as the take does today: theft when seen. | cheap | section 2.7 |
| 10 | Letting go | It falls, and it flies with the hand's speed: fall and throw both ("fallen und werfen!"). No throw damage (section 2.4's limit: the strike takes no location; a thrown weapon hits as a thrown object does, ThrowLogic). | fall small, throw medium | the drop call of section 2.5 is unverified (`UnequipWeapon` 0x005F2E70; `RemoveItem`'s drop, virtual 0x40, slot +0x100; how the new ref comes back and when it has a body); the throw needs that body to take the velocity; a harness measurement of both first. The hold's flicker guard: a grip read open for less than a release time must not drop (`TwoHandReleaseSeconds` 0.12 s of controls-spec 4.10 as the first value; the measurement tells) |
| 11 | The chest spot | Takes the held weapon into the inventory unequipped, as it takes every held thing ("stow gibts ja schon für alle items also auch waffen"). | small | the stow exists (`vr::StepStow`, `game::TakeIntoInventory`); a weapon in the weapon hand is equipped, so this is the unequip plus the stow's place |
| 12 | Gates | No grab-equip in menus, mounted, dead or knocked down. A two-hander grabbed with a shield on the arm: the engine takes the shield off, as vanilla does. A quest weapon, which the game does not let fall: it stays in the hand with the game's own message; sliding it into its holster instead is a later nicety (small). | cheap | the gates exist for the holsters and the grab |
| 13 | The measurement | Accepted (5.4): the instrument built, the headset round the tester's. | cheap, plus a headset round | |

### 5.2 What the decisions rest on [R, D]

- The take into the inventory happens at the grab, not at the stow [D]: the engine shows a weapon in the hand only as the equipped weapon, and `EquipItem` equips from the inventory (section 2.1). It shows in the encumbrance, in the inventory menu while the weapon is held, and in the theft's moment.
- The engine's one weapon slot, and the off-hand's own slots for the shield and the torch [R, section 2.3]: no dual wielding; a two-hander unequips the shield.
- The holster of a kind holds the last weapon of that kind seen equipped [R, controls-spec 4.1]: decision 6 keeps it.
- Quest items cannot be dropped [R, the game's rule]: decision 12.

### 5.3 Noted for later, not built

- Model B: every holster visibly filled (OBVR's own nodes on the body, the stow state in the save), with the refusal of an occupied holster and the grab out of it. Large.
- The off-hand equips a weapon. Large (decision 8).
- Shield and torch at any holster. Small to medium (decision 7): the second step, after the six kinds. BUILT 2026-10-24 at the one-hander's and the two-hander's place (5.4); the bow's place stays the bow's.
- A quest weapon that cannot fall slides into its holster. Small.

### 5.4 The grip's signal, and the instrument built 2026-10-23

**What reads today [R].** The game runs on the action path (the tester's OBVR.log: "OpenVR input: normalized Touch/Index controller actions ready"). The grip is the boolean action `left_grip` / `right_grip`, and the shipped Index binding makes its click from the force sensor: `"force_input": "force"`, the click at 0.3, let go at 0.2 (assets/input/knuckles.json). The force sensor reads 0 until the hand presses: that is the "100 % grip" of the tester's wording. The Touch binding reads its grip in the trigger mode and takes the click alone (assets/input/oculus_touch.json); the Touch grip is an analog trigger, not a switch.

**The Index's two grip signals [R].** `/input/grip` carries a value and a force (the input profile's components, deepwiki on ValveSoftware/openvr: `value` a 0 to 1 scalar, `force` "the input can sense force"). The value is the capacitive sensing: "this is only the capacitive proximity reading and will max out at 1.0 if touching lightly" (godotengine/godot issue 74023, https://github.com/godotengine/godot/issues/74023, which also names the force at `/input/squeeze/force` and, after Unity's Valve Index profile page, the force sensor as the Index's alone). The force sensor saturates quickly too, by a user's report: "It does not require much force, before "maxing out", as the force value output goes from 0.00 to 1" (r/ValveIndex, https://www.reddit.com/r/ValveIndex/comments/q1sswb/). So a realistic hold - the fingers closed round the grip without pressing - is a high value with a force near 0, and where the hand at rest in the strap sits on the value is unmeasured: it may read high. The thresholds are to be measured, not guessed.

**The binding modes [R].** SteamVR's binding modes for an analog source: trigger (pull as a vector1, click, touch), button (click, touch), force_sensor (force), grab - the mode table in Rune Berg's SteamVR Input guide for Unreal 4.21 (https://runeberg.medium.com/getting-started-on-the-new-steamvr-input-system-with-unreal-engine-4-21-7856a02f4b38). A binding file lists one path under several modes at once: TechtonicaVR's bindings_knuckles.json binds `/user/hand/left/input/grip` in the `force_sensor` mode with `force` and again in the `grab` mode (https://github.com/Xenira/TechtonicaVR, libs/StreamingAssets/SteamVR/bindings_knuckles.json).

**Built [R].**
- Two vector1 actions per hand in the manifest, optional: `grip_pull` and `grip_force` (assets/input/actions.json). The Index binding adds the grip in the trigger mode with its pull (the value) and in the force_sensor mode with its force, beside the button mode's click as before; the Touch binding adds the pull to the grip's trigger mode, no force (no sensor). tests/ControllerActionAssetsTest.py checks both files.
- The action path reads them into `HandPose::gripPull` and `HandPose::gripForce` (vr/ControllerActions.h: the analog loop takes every vector1 as it took the trigger, 0 to 1; an inactive or malformed action stays 0 and unflagged in the active mask). controller_actions_test, nine actions a hand. **A change in play:** `gripForce` was 0 on the action path before (ControllerActions.h zeroed it; the legacy path alone filled it from axis 2), so the fist by the grip's force ([Hands] FistGripForce 0.6, vr/Fist.h, built 2026-10-15 for the tester's left Index) never fired on the live path; now it sees the force sensor, and a firm squeeze of the weapon hand with nothing drawn raises the fists as the setting says.
- `[Debug] GripTrace=1` (vr/GripTrace.h, pure, grip_trace_test): a "Grip trace:" line per hand with the pull, the force and the click - at the hand's first tracked frame, every half second after, and at once when the click changes; "(not bound)" after a signal whose action is inactive, "(legacy)" on the legacy path. The hand script's grip is pull and force alike (test/HandScriptPose.h); scenario tools/hand-scripts/grip-trace.txt, not run yet.

**Open [G].**
- Whether SteamVR hands the Index grip's capacitive value through the trigger mode's pull as the binding assumes: the trace's "(not bound)" after the pull says if not; the `grab` mode, or the raw `value` input, would be the next try.
- A binding the tester once customised in SteamVR's binding UI shadows the shipped default and lacks the new actions; the trace would then say "(not bound)" for both. Unverified; the binding UI's default reset would settle it.
- What the hand at rest in the strap, a light hold and a firm squeeze read on the pull and the force, on each hand, at the hip and free: the measurement.

**Then [D].** One grip definition for OBVR with hysteresis, as settings - a close and an open threshold on the signal the measurement picks (the pull, the force, or the pull with the force as the confirmation) - applied to the holsters first (decision 4), then to every weapon grip; the click stays the fallback where no analog signal reads. The fist keeps its own, stronger signal.

**Measured 2026-10-23, 19:18 to 19:21, the tester's Index controllers on the Linux game [R].** Both grip actions bound and active (bound mask 1FF), no "(not bound)", 364 trace lines. A first session at 18:41 recorded nothing: a harness test INI of another session's run lay in the plugin folder and the game read it as the test overlay (GripTrace=0, its hand script started) - how it got there four minutes after that runner's cleanup stayed unexplained; the token guard of docs/hand-script-harness.md is the answer. The right hand, 90 s:

| The hand | Pull | Force | Click |
| --- | --- | --- | --- |
| at rest in the strap, fingers relaxed | 0.00 to 0.10 (111 of 179 lines at 0.0 or 0.1) | 0.00 | 0 |
| fingers closed round the grip, no pressure | 1.00, steady over seconds | 0.00 to 0.20 | 0 |
| squeezing | 1.00 | 0.30 to 0.80 | 1 from 0.30 to 0.38, 0 again at 0.15 to 0.20 - the binding's 0.3 and 0.2 as shipped |

The pull passes its middle values in under a second on the way up and down (a handful of lines at 0.2 to 0.9 in the whole session), and once closed it stays at 1.00 without a flicker. So the pull separates a relaxed hand from a closed one by itself; the force is not needed for the hold, it stays the click's and the fist's signal. **Derived thresholds [D]:** a grip closes at a pull of 0.8 and opens at 0.4 - both far from the rest's 0.1 and the hold's 1.0, with the hysteresis against the transition - as the settings of the grip definition; the click as the fallback where no pull reads. The accidental-grab risk of the capacitive value (section 5.4's open point) did not show: this tester's hand at rest reads 0.0 to 0.1.

**The left hand, measured 19:26 to 19:28 in a second session [R]** (the first session had left the left controller alone: "links habe ich kaum oder gar nix gemacht damit"). The same picture: at rest a pull of 0.0 to 0.1 with one 1.5 s stretch at 0.3 while the hand settled, the fingers closed round the grip 1.00 steady for eight seconds at a force of 0.0 to 0.2 and no click, squeezing clicks at a force of 0.30 to 0.32 and lets go at 0.16 to 0.18, the force up to 0.48. So the thresholds hold for both controllers, and the left's force sensor answers. Its finger sensors stay flaky (this session the ring finger 0.00 and the little finger 1.00 whatever the hand did; two weeks ago the little finger always 0.00) - a reason not to build a hold on the curls.

**Built 2026-10-23, evening [R]: the hold, on the holsters.** vr/GripHold.h: a hand's hold from the grip's pull with the hysteresis above - closed from `GripCloseAt` 0.8 up, open again from `GripOpenAt` 0.4 down - as the settings `[Hands] GripByPull`, `GripCloseAt`, `GripOpenAt`; the click in the pull's place where no pull reads (the legacy input, a binding without the action, the setting off), nonsense limits sanitised (grip_hold_test, 40 flows). The holsters' grip is the hold now (HandMode.cpp; decision 4's first step): a reach is open the hand, put it at the place, close the fingers; a hand arriving closed stays a grab, as the holster always had it. The grab, the bow's draw, the two-hand take and the stow keep the click until the next steps. Scenario holster-pull.txt proves the weak grip, the draw, the hold at half and the sheathe by the counts.

**Built 2026-10-23, late evening [R]: the world weapon taken at its grip (decisions 1, 2, 8, 9, 11's neighbour; section 5.5's step 2).** game/WeaponGrab.h, pure, weapon_grab_test: the grip point on a weapon reference's model - its node's world transform, the vanilla hold at -0.9 along the model's y for blades, blunts and (unmeasured) the bow, -37.6 for a staff (`GripPointOf`, `GripAxialFor`; the type from the base form's +0x90, game::RefWeaponType); what a closing grip does (`ChooseWeaponGrab`): the weapon hand's controller within `[Hands] WeaponGripReachMetres` (0.12) of the grip takes it, a touch elsewhere on the model carries it as any object, the other hand only ever carries, and a weapon is never pulled from afar; the take's steps (`StepWeaponTake`): the game's activation (game::TakeIntoInventory - its owner's rules, a theft when seen), the equip sent the next frame (game::EquipWeaponForm, silent), the ready-weapon click once the game shows the weapon equipped and sheathed, routed through the hand mode's ready tracker (HandModeFrame::readyWeaponClick) so "Hands: ready weapon done" reports the draw; given up after two seconds, cancelled by a menu. The trigger is the weapon hand's hold closing (vr/GripHold.h, the pull), the same hold as the holsters'; the pick's ring sits on the grip point unless the hand touches the model elsewhere (CameraHook, the pick's aim). Gates: in the world, no menu, no fit running, nothing held, no weapon drawn, the holster not claiming the grip, not dead; `[Hands] GrabEquipsWeapons=0` leaves weapons plain objects. Measured in the harness (weapon-grab.txt, PASS 21:17 after five measuring runs): the dropped sword's grip 0.20 m below and 0.10 m left of the first guess, the sword still settling ten seconds after the drop, and the reach measured at the drawn hand's palm sits 0.15 m off a grip the controller is on - so the reach is the controller's, as every reach of the pick is; the equip showed and drew 0.13 s after the take. Open: the bow's hold point; the equip's stack pick with two of the same form; the holster's "grip closed at no place" line on every take (cosmetic); the feel of 0.12 m in the headset; the frames between the take and the equip (one) and the draw (0.13 s) as the tester sees them.

**The tester's first round, 2026-10-23 about 21:20 [R]:** "hat 2 aus 5 versuchen geklappt mit dem greifen vom boden". His OBVR.log, the "Weapon grab:" lines: the two takes came with the controller 0.12 m and 0.12 m from the grip ("a hold takes it", taken, equipped, drawn in 0.15 s both times). Of the other three: one had the controller at 0.14 m while the hand touched the model at 3 to 4 units - a carry, which he then stowed at the chest; one read "a hold takes it" at 0.08 m and still no take, the sword carried and stowed instead - the hold had closed on the way, before the hand was at the grip, so no closing edge came at it; the fifth left no line within reach. Three things follow, to build next (the tester stopped the evening there):
1. **The pull stays, as an option, default on** (the tester: "es soll weiterhin möglich sein das zu mir zu teleporten wie es vorher war da comfortabler ... neue option: dinge an die hand zu teleporten wie bisher. default on"): a weapon out of the hand's reach floats to the hand on a grip as every item did before this step, `[Hands] PullToHand=1`; what a pulled weapon does on arrival at the hand with the hold closed - equipped at once, or carried until the hand finds its grip - is to decide with the tester. Cheap: the pull's refusal for weapons is one condition.
2. **The take while the hold is closed, not only at its closing edge:** a hand that closes on the way and arrives at the grip closed takes the weapon (Blade & Sorcery's grab on contact with the grip held); after a take the hold has to open once before the next. Cheap, grip-hold flows plus a harness run.
3. **The reach is tight at 0.12 m:** a hand touching a sword on the floor keeps its controller 0.12 to 0.14 m above the grip point. `WeaponGripReachMetres` 0.18 as the next try, measured again in the headset. Cheap.

**Built 2026-10-24 morning [R], all three** (the tester: "sofort ausrüsten. go"): `[Hands] PullToHand=1` keeps the pull (ChooseWeaponGrab's Pull: a weapon out of the hand's touch floats to the hand on a grip as every item does), and any weapon the weapon hand's grab takes - pulled from afar, or touched on its blade - is equipped the moment it is in the hand (StepWeaponPull: the grab key up so the engine lets go, then the take, equip and draw as at the grip; the grab physics stands back as it does for a stow, since the throw of a reference the activation had deleted went down with the game twice). So decision 2's carry is now the other hand's alone, and a weapon-hand touch on the blade equips too - to confirm with the tester. The take fires while the hold is closed at the grip, once per closing, and a hand closed by a take grabs nothing else until it opens (StepWeaponTakeTrigger, WeaponHoldBlocksGrab; the first run had the still-closed hand grab a potion beside the sword and the grab restart the draw every frame). `WeaponGripReachMetres` 0.18. Scenarios weapon-grab.txt (closing on the way) PASS 08:05, weapon-pull.txt PASS 08:04, holster-pull.txt PASS 07:59 and 08:11 (its 08:07 run failed with the game at nine frames a second beside another session's game started at the same minute: the draw's keys fired whole in one frame and the draw never followed); weapon_grab_test 60 flows. On the way the harness itself was fixed: the synthetic head drives the pose reads from the script's load on, not from its start, because an awake headset on the desk had anchored the world two metres off before the script began (test/HandScriptRuntime.cpp). The grip's take also keeps a quiet second after any grab key.

**Built 2026-10-24, late morning [R]: let go = fall, throw, holster, chest (decisions 3, 10 and 11; 5.5's steps 3 and 4).** game/WeaponRelease.h, pure, weapon_release_test (30 flows): with `[Hands] WeaponHold=1` (default) the holding hand's hold closing on the drawn weapon arms it (the weapon hand's hold; the other hand's for a bow), and the hold open for `WeaponReleaseSeconds` (0.12, the flicker guard) lets go by where the hand is: in one of its holster zones the ready click sheathes it; at the stow spot it is unequipped and stays in the inventory; anywhere else it is unequipped and dropped where the hand is (game::DropEquippedWeapon - UnequipItem, then the player's RemoveItem with the drop flag and the hand's position, section 2.5 as verified), and when its body arrives it is sent off with the hand's speed (the fastest of the last eight frames, `WeaponThrowStrength` times, below 35 units/s nothing: a fall). A weapon drawn with the hand open (the inventory, the ring) stays until the hand closes on it. A quest item (TESForm flag 0x400) is refused once per opening and stays in the hand; the holster and the chest still take it. Gates: the world, no menu, no fit, not mounted, not dead, no take or pull of the grip running. Harness: weapon-drop.txt PASS 09:43 (the body after 2 frames, the model 0.1 units from the hand), weapon-throw.txt PASS 09:44 (41.89 m/s from a scripted jump, the sword 377 units ahead), weapon-release-holster.txt PASS 09:45; weapon-grab, weapon-pull and holster-pull PASS again after (holster-pull with WeaponHold=0, since its 0.3 out of the zone is a release now). Seen: after a drop the engine stays in the ready state, so the empty hand showed the fists up (weapon state 2); the tester (2026-10-24): a bug, the hand opened to let go, no fist belongs there - so once the slot shows empty with the fists up, the ready click lowers them (WeaponReleasePhase::Lowering, "the slot is empty and the game shows the fists up on the open hand - lowered"; weapon-drop.txt PASS 10:08 ends in weapon state 1, slot 0). That answers 4.2's open question: the empty hand is an open hand. The drop plays the game's own drop sound (not checked by ear). Open: the bow's release (the left hold, untested in the harness); the stack the engine drops with two of the same form; the feel of 0.12 s and of the throw's strength in the headset; a weapon taken at the grip is drawn with the hand already closed, so the first opening after a take drops it - as in B&S.

**Built 2026-10-24, early afternoon [R]: the shield and the torch at the holsters (decision 7; 5.5's step 5).** `[Hands] HolsterShieldTorch=1` (default): the other hand carrying a shield or a torch, its hold closing at the one-hander's place (the left hip, for a right-hander) or the two-hander's (over the right shoulder) stows it there - remembered at that place, unequipped into the inventory with the game's item sound - and the other hand empty, its hold closing at a place where one was stowed takes it back (EquipItem, the game's sound). One item per place, the last stowed; an item is at one place at a time. In vr::StepHolster (holster_test, 96 checks with the new flows), the forms in the glue for the session, as the weapons' memory. The engine side, read 2026-10-24: the light's getter beside the shield's in the process table (GetEquippedLightData at +0xF0, 0x0064B270, the same body as the shield's 0x0064B2D0 with the entry at process+0xE8); EquipItem (0x005FAEA0) counts the item in the inventory first and does nothing below one, so a take of a shield sold since makes nothing, and armour and lights go through the weapons' equip; the unequip is the weapons' UnequipItem, generalised (game::UnequipItemForm). Decided on the way, against the tester's "an beliebiger holster": **the bow's place stays the bow's.** A shield stowed behind the left shoulder would come back on the next reach there when the bow was wanted, and the one-reach swap from sword and shield to the bow (the engine takes the shield off for the bow, as vanilla does) would need three. Cheap to change if the tester wants the shoulder for the shield (a fourth place, or the bow behind the right shoulder). A take waits while a two-hander or a bow is drawn ("it goes back first"); with a bow drawn the other hand holds the bow and stows nothing. **The game's rule the harness found:** a shield equipped with the fists, a two-hander or a bow in the slot stays off the hand (UESP Oblivion:Armor, "the shield is assumed to be unavailable"), and a torch is in the hand while the weapon is sheathed (the game's manual); so what the hand carries is read from the process (the in-hand shield and light), and the watch of a stow or a take reads the item's worn stack instead (game::PlayerWearsItem) - a shield taken back with the fists is equipped and shows once a one-hander is out. Harness: holster-shield-torch.txt PASS 13:56 (shield at the hip twice, torch over the shoulder, an empty reach, the take with the fists), shield-equip-probe.txt measured the rule; holster.txt, weapon-drop.txt and weapon-release-holster.txt PASS after. Open: the headset; whether the take with the fists should say so in the game (today only the log does); Model A leaves a stowed shield unseen on the body.

### 5.5 Order

1. The measurement (5.4), then the grip definition on the holsters. BUILT 2026-10-23, headset open.
2. The world weapon taken at its grip: the ring on the grip, take + equip + draw as one flow (sections 2.1 and 4.3), the carry off the grip. BUILT 2026-10-23 (5.4), headset open.
3. Let go: the drop research of section 2.5, then the fall, then the throw. BUILT 2026-10-24 (5.4), headset open.
4. The chest spot for the held weapon. BUILT 2026-10-24 with 3 (the stow spot unequips), headset open.
5. Shield and torch at any holster. BUILT 2026-10-24 (5.4, "Built 2026-10-24, early afternoon"), the bow's place excepted, headset open.
6. The later list of 5.3 as the tester asks.

### 5.6 Nadi's Windows round, 2026-10-24 evening (open, noted 2026-10-25)

Nadi played the build of 2026-10-24 on Windows; the log is saved as `artifacts/scratch/OBVR-win-20261024-2229.log`. The
legend of the study applies.

- **Fixed 2026-10-25: letting go threw the weapon too hard** ("Waffe loslassen ist zu viel
  force"). The log had 10 drops, every one "the hand at 0.00 m/s (a fall)" and "sent off at
  0.00 m/s" [R], so the push was not OBVR's speed. Reproduced in the harness
  (weapon-drop-sweep.txt): the hand opens still, and as the sword comes into the world it
  sweeps on through it at about 2.5 m/s, as a real hand moves on - the sword, given 0
  units/s, flew off at 551, 370, 238 units/s and 23 units on in twelve frames. Two pushers,
  found one after the other: OBVR's own ray push of the hands and the blade
  (game/WorldPush.h) gave it the hand's speed - with that off and the group below, 148 to
  290 units/s were left - and the hands' keyframed Havok bodies, in the step or two before
  OBVR sees the new body. Fix: the dropped weapon's body takes the player's collision group,
  the hands' bodies' group, at the send-off (game::PassDroppedBody), is left alone by the ray
  push while it passes (game::DroppedBodyPassing), and gets its own group back once its bound
  is a hand's margin (12 units) from both tracked hands and out of the player's capsule
  (`DroppedWeaponClear`, weapon_release_test; "clear of the hands and the player after N
  frames - group put back"); a grab taking it again keeps its own group; and while it waits
  for its body the hands' bodies stand out of the world (two or three frames). The sweep then
  matched a still drop: 0.0 0.0 -8.5 and -11.9 units in twelve frames, a plain fall
  (2026-10-25 11:14 and 11:15). A new "flight of the dropped ..." line logs every drop's
  first twelve frames.
- **Fixed 2026-10-25: throwing did not work** ("Waffe werfen geht nicht"). Cause [R in the
  code]: the throw's speed was the fastest of the last eight frames before the drop, and the
  drop comes WeaponReleaseSeconds (0.12 s) after the hold opens - at 90 Hz eight frames are
  89 ms, so the window held only the follow-through, never the throw (all 10 drops of the
  evening 0.00 m/s). weapon-throw.txt had passed because its hand jumped 0.06 s after the
  opening. Fix (game/WeaponRelease.h): the release step keeps the hand's speed while the hold
  is closed and takes the fastest of the eight frames before the opening and the opening's
  own; the follow-through after it is not the throw (weapon_release_test, red first).
  weapon-throw-swing.txt (a swing at about 3 m/s, the hand opened at its fastest frame and
  stopped dead): FAIL 10:22 with the old step ("a fall"), PASS with the fix (4.6 to 4.8 m/s,
  the sword 48 to 62 units on in twelve frames). weapon-throw.txt now jumps in the frame of
  the opening.
- **A split second's pause** between the weapon floating in and being equipped, and the same
  when letting go ("eine Splitsekunde Pause zwischen Waffe schwebt an und wird ausgerüstet,
  same beim Loslassen"). Fixed 2026-10-25; the harness counts below are at 74 to 75 frames a
  second (13.5 ms a frame), the headset is open.
  - **Letting go [R in the code, M in the harness]:** the pause was OBVR's own wait. The drawn
    hand keeps the weapon, and its fingers stay the animation's grip while the weapon hangs on
    the hand (game/HandGrip.cpp, HandHoldsItem), for `WeaponReleaseSeconds` after the hold
    reads open - 0.12 s, a flicker guard against a flicker no log ever showed (the pull reads
    1.00 steady while closed, 5.4, and opens only below 0.4). Now 0.03 s: weapon-drop 12:12
    "let go at 3 (40 ms), off the hand at 3, the dropped one's model loaded at 4 (53 ms), its
    body at 4" - one frame with no weapon between the hand and the world. A hold read open for
    less is kept and logged ("the hold read open for N s and closed again - kept";
    weapon-hold-flicker.txt PASS 12:11). The throw's speed does not depend on it since the fix
    above. Left: the pull's own way down from 1.0 to 0.4 as the fingers leave the grip,
    unmeasured.
  - **Taking, the cause [R in the game's files, M in the harness]:** the floated model goes at
    the activation, the equip goes the same frame and puts the weapon on the sheath, the frame
    the game shows it equipped the ready click is set for the next frame's hand mode, and the
    engine plays the Equip group; the weapon hangs on the hand only at the group's Attach key -
    onehandequip.kf 0.300 s of 0.633, twohandequip.kf and staffequip.kf 0.367 of 0.933,
    bowequip.kf 0.500 of 0.800, handtohandequip.kf 0.100 of 0.200 (pyffi on the first-person
    .kf files). Counted by game/WeaponTimeline.h (weapon_timeline_test): weapon-pull and
    weapon-grab 12:13 and 12:14, **7 frames, 93 ms** from the take to the weapon on the hand's
    bone (equipped shown at +1, out at +3, on the hand at +7).
  - **Taking, the fix [R, M]:** two steps, each measured.
    1. `[Hands] TakeDrawsAtOnce=1` (game/WeaponGrab.h StepWeaponTake, weapon_grab_test): in the
       take's frame the weapon on the hip comes off, the weapon-out flag is set (HighProcess
       +0x114; the process's own setter 0x0064AD20 writes nothing else, read in the binary) and
       the equip follows - no ready click, no frame waiting for the sheath. The probe
       weapon-equip-drawn-probe.txt (12:20, [Debug] WeaponNodeTrace) had shown an equip with the
       fists out on the hand in the same frame with no Equip group; with the flag set by hand the
       engine still plays the Equip group itself, but at once: 5 frames, 66 to 70 ms (weapon-pull
       12:26, weapon-grab 12:27).
       Shown sheathed after all, the click draws it as before; out but not on the hand within
       0.25 s, a click sheathes it and the next draws it ("straight into the hand did not hold";
       never seen).
    2. A take's draw goes on to its Attach key (game/WeaponDrawSpeed.h JumpNextDrawToAttach,
       weapon_draw_speed_test): armed by the take's equip for the next Equip sequence, its blend
       in shrunk to nothing, and in its first playing frame its time moved to just past the key
       read by name from the sequence's text keys (+0x20, the count at +0x0C, the keys at
       +0x10, eight bytes each with the text at +4, as the game's key handler 0x0051AF70 reads
       them); the rest plays at the draw speed, so the group still ends later.
    Together **3 frames, 39 to 44 ms**: weapon-pull 12:31, weapon-grab 12:32, weapon-pull-claymore
    12:35 and weapon-pull-bow 12:37 (the bow on the left hand), all "moved on to its Attach".
    weapon-pull-draw.txt keeps the old way covered (TakeDrawsAtOnce=0: 5 frames with the jump
    alone, 12:33). Left [D]: the three frames are the flag seen, the sequence's first frame and the
    engine's attach a frame after the key; closing them means hanging the weapon on the hand
    ourselves - the floated model kept at the hand until then (medium to large), not built.
  - **Seen on the way:** a weapon let go at the holster leaves the hand 9 frames, 161 ms, after
    the hold opens (weapon-release-holster 12:15) - the Unequip group's Detach key
    (onehandunequip.kf 0.467 s of 0.867). The same pause in the other direction; not reported,
    not changed.
- **Wish: a short label at each holster** showing what is holstered there ("Kurze Anzeigetafel
  für jeden Holster, was zeigt, was da nun geholstert ist"). Under Model A nothing shows on
  the body (decision 5); the holster's remembered weapon, shield or torch is known [R]. A small
  panel at the place while the hand is near it, as the hand HUD's overlays are drawn
  (hud-on-hands-spec) [D].
- **Wish: strikes with the pommel, and the foe recoils** ("man kann Gegner mit dem
  Waffenknauf hitten, Gegner kriegen Recoil"). Section 2.4's open point: the strike
  (0x005FEBF0) takes no location and no kind of damage [R]. Buildable [D]: a touch by the
  handle's end counted as a hit (the power flag off), with the foe's recoil from the shove's
  stagger (ShoveActor); a blunt damage for the pommel stays blocked as in 2.4.
- **Wish: half-swording, and the foe recoils** ("Half swording, Gegner recoilen"). Section 4.4.
  The design talked through on 2026-10-25 (in the session, not decided): stage 1 the left
  hand on the blade (TwoHandTakes' zone above the hand, as the staff's shaft), stage 2 the
  blade steered by the line between both controllers, while the hand is on the blade, as a
  share setting (the drawn left hand drifts about 7 cm from its controller at a 10 degree
  wrist error 30 units up the blade, derived), stage 3 a half-sword thrust as a power
  attack from a shorter thrust; more damage stays blocked (2.4). New here: the foe recoils
  from a half-sword thrust.

### 5.7 The torch let go of, still burning, and kept with a two-hander (built 2026-10-26)

Nadi, 2026-10-26: "Fackeln: wenn in der hand (egal ob inventar oder in welt aufgenommen) kann
ich sie jederzeit in die Spielwelt auf den boden oder sonstwo droppen durch loslassen an der
hand und zwar lit also noch brennend (light source in world) (gibt viele mods die das bereits
machen, hier schauen). Auch wenn ich 2 hander habe rechts gezogen bleibt die fackel in der
linken Hand aktiv." Then: "wenn ich die fackel fallen lasse dann ohne sound".

**What the mods do [read on Nexus, 2026-10-26].** The original game's drop-lit-torch mods swap
in a second light record for the dropped torch (DLTIC, oblivion/mods/3429: "The burning torch
is a new item") or move a light of their own to it (ROM DLT, oblivion/mods/34644: "I do
everything with the standard Torch02, and a torch light source"); TorchLight for the remaster
calls the vanilla drop "a lightless log". OBVR needs no plugin: it lights the dropped
reference with the engine's own calls.

**Why a dropped torch is dark [read, measured].** Torch02's light flags are 0x2B - dynamic,
carried, flicker and "off by default" (0x20) [M, torch-drop-lit.txt 08:41]. A new reference
of a light takes the form flag 0x80 ("fire out") from that bit (0x0044A968, through
0x0046AB60) [R]; the 3D's set-up reads it: set, the flames are taken off, clear, they are put
on (0x004E41DA: 0x004DE1C0 off, 0x004E26F0 on) [R]. The light itself, 0x004B1600 (the same call
the 3D's set-up makes for a light reference, 0x004DF53E, and the body makes for a torch in the
hand, 0x00478270), makes no light for an "off by default" light unless the reference's vtable
+0x190 says yes - an actor's does, a plain reference's answers false (0x0069D990) - and marks the
fire out again [R]. Measured on the dropped torch as the engine made it: fire out 1, no flame,
the cap hidden, nothing under "AttachLight".

**Built.**
- *The release* (game/TorchRelease.h, torch_release_test, 16 flows): as the weapon's hold
  (5.4) for the other hand's torch - armed when the hold closes on the torch in the hand,
  let go when it opens past WeaponReleaseSeconds: at a holster nothing (HolsterShieldTorch's
  close stows it there, as before), at the chest into the inventory, anywhere else dropped
  at the hand with the hand's speed (WeaponThrowStrength). A hold that is a grab's, a
  person's, the weapon handle's or the holster's never arms it. `[Hands] TorchDrop` (default
  1). The drop and the chest's stow take the torch off without the item sound, as a dropped
  weapon (Nadi: "ohne sound"); the holster keeps the game's sound.
- *The fire* (game/TorchFire.h): in the dropped torch's first frame with a 3D, the fire-out
  flag cleared, the flames put on (0x004E26F0), and the light made by 0x004B1600 with the
  base light's "off by default" bit cleared for that one call and set back after. Measured:
  fire out 0, one flame, the cap shown, a NiPointLight under "AttachLight", the same a second
  later [M, 08:43].
- *Kept burning* (game/TorchGroundLogic.h, torch_ground_test, 8 flows): the mark is the
  torch's own fire flag - cleared by OBVR, kept in the save by the engine - so OBVR keeps no
  list and writes nothing into the save (Nadi, 2026-10-26, asked whether a saved list risks
  bloat: "go" for the way without save data). Every 30 frames the player's cell is walked:
  a carried, off-by-default light with the fire on, its flames there and no light under
  "AttachLight", gets the light again; one with the fire out is vanilla's dark log and left
  alone. When the engine builds a torch's 3D anew - the cell left, the cell buffer purged,
  come back - it puts the flames on itself (the cleared flag) and no light; OBVR adds the
  light. Measured [M]: torch-drop-relight.txt 09:04 and 09:51 ("1 given their light again -
  the 3D came back without it"), and **after the game was quit and started again**, the save
  loaded in a fresh game: the torch burning with its light again, its name shown for the
  take (torch-relight-restart.txt 09:49). A list by form id came first and went: its id
  finds nothing both when the torch was picked up and when its cell is unloaded.
- *Learned the hard way [M]:* flames put on a 3D already in the scene draw without their
  render state and the renderer faults (NiDX9RenderState on the flame's NiTriShape under
  "BASE Lights\Torch02.NIF", CrashLogger 09:02). So the flames are OBVR's only in the drop's
  first frame - the engine sets the new 3D's render state up when it hangs it into the cell -
  and a 3D built anew gets only the light.
- *Picked up again*: taken into the inventory as activating takes it (Debug.
  TorchTakeBackSeconds for the harness), the reference and the light under it go with it;
  the cell's walk counts it no more and the game ran on [M, torch-drop-pickup.txt 09:10 and
  09:53].
- *The two-hander* (game/TorchKeepLogic.h, game/TorchKeep.cpp, torch_keep_test): 0x005E7230
  decides what the left hand holds; with weapon types 1, 3, 4 and 5 (the two-handed blade
  and blunt, the staff, the bow) drawn it hides the worn torch - its 3D off (0x004DCAB0), no
  unequip - at 0x005E73E6 (a weapon or a torch equipped) and 0x005E76C6 (the refresh the draw
  calls) [R, the subagent's reading checked in the disassembly]. Both jump to stubs: the
  engine's own test first, then, with OBVR's flag set, the actor the player and the weapon a
  two-handed blade or blunt, past the hide as if sheathed. The staff and the bow keep the
  engine's way (the bow is held in that hand). The left hand with a torch does not take the
  two-hander's handle. `[Hands] TorchWithTwoHander` (default 1). Measured: the Iron Claymore
  drawn, "the torch in the other hand", the picture shows it burning in the left hand
  [M, torch-two-hander.txt 09:11]; with the setting 0, "no torch worn - put away" after one
  frame with it still in the hand [M, torch-two-hander-vanilla.txt 09:14].

**Open.**
- Only the player's cell is walked: a torch in a neighbouring exterior cell whose 3D is
  built anew while the player stands next door gets its light when the player steps into
  its cell (derived, not seen). Cheap to widen to the loaded cells if it shows.
- A save loaded in the same cell kept the very same reference and 3D (no rebuild) [M, 08:55];
  a load from another cell is untested.
- The light's look on the floor is the engine's own torch light; whether it reads as bright
  enough in the headset is Nadi's to judge.
- Twice on 2026-10-26 the harness's load of OBVRHarness sat at the loading screen for 240 s
  (torch-drop-lit.txt 08:36 and 09:15); the same happened on 2026-10-24 and 10-25 with older
  builds (weapon-grab.txt, enemy-health.txt), so it is not this work's; a rerun loaded.

**The drop lit by the engine itself (fixed 2026-10-26, after Nadi's crash).** OBVR put the
dropped torch's flames on (0x004E26F0) in the frame it first saw the 3D, assumed to be before
the engine hangs that 3D into the cell, where the new shapes get their render state. At
times it already hung there: the flames' four shapes stayed without a property state and the
renderer faulted drawing the Sparks (0x007659A9; Nadi's Windows crash, reproduced by
torch-drop-stow-cycle.txt at its third drop - controls-spec has the readings). Now the
light's "off by default" bit is off for the drop's RemoveItem call as well (it was already for
AttachLight), so the new reference is not "fire out" and the engine's own set-up puts the
flames on; OBVR adds only the light, and never puts flames on a 3D already in the scene
(game/TorchGroundLogic.h LightFlagsForLitDrop, EngineLightsDrop, FlamesMayBePutOn).
torch-drop-stow-cycle.txt PASS: eight drops, one flame each, every shape with its state.

### 5.8 A dropped weapon or torch keeps the turn it had in the hand (fixed 2026-10-26)

Nadi, 2026-10-26, of the Windows session the day before: "Die waffe/torch hat wenn sie fällt
eine ganz andere Ausrichtung als wie sie in der hand gehalten wurde als sie fallengelassen
wurde ... für torches whl gleich."

**Cause, measured.** The drop is the engine's RemoveItem with the drop flag and the hand's
point; its turn argument was null, so the engine turned the new reference its own way.
weapon-drop-engine-turn.txt (the control, `[Debug] DropTurnAsHeld=0`): when the dropped Iron
Longsword had its body, two frames on, its 3D lay **84.5 degrees** off the turn it had in the
hand [M, 2026-10-26 10:43]. Its point was the hand's, 9 units from where the sword's model
hangs in it.

**Fix** (game/DropRotationLogic.h, drop_rotation_test 10 flows, red first):
- The item's pose in the hand: the model under the first-person "Weapon" or "Torch" node, read
  off the drawn bones (the pose of the frame before) and carried onto the hand's pose now, as
  the hand contacts read their shapes.
- The reference keeps its turn as three angles (+0x20); the engine builds its 3D's matrix from
  them in 0x004D7AF0 - about x, then y, then z (0x0070FD30, 0x0070FD80, 0x0070FDD0, multiplied
  by 0x007100A0) [R]. RefAnglesFromMatrix finds the angles back for any turn, the straight up
  and down included.
- The drop is given those angles as its turn argument and the model's point; the new
  reference's angles and place are then written as well (game::PlaceDroppedRef) - its 3D is
  built a frame or two later and its body from that.
- Measured with it [M, 10:39 to 10:46]: weapon-drop.txt 0.0 degrees off when the body came,
  torch-drop-lit.txt 1.4, weapon-throw.txt 0.0 (thrown at the hand's speed as before),
  weapon-drop-near-body.txt 20.3 (the sword let go against the player's body - it met it in
  the two frames before its body came, derived, not seen).

**The throw's spin (built 2026-10-26, Nadi: "go").** A weapon or torch thrown takes the hand's
turning speed with it: SteamVR's angular velocity of the controller, kept frame by frame
beside the speed, taken from the frame the throw's speed is taken from (FastestSample), times
`[Hands] ThrowSpinStrength` (default 1, 0 no spin), at most 25 radians a second (four turns)
against a tracking glitch; none for a fall (game/GrabPhysics.h ThrowSpin, weapon_release_test
and torch_release_test, 8 new flows, red first). It goes onto the body with its speed, through
the motion's angular velocity (vtable +0x58, the slot the grab's drive already uses). The
harness's scripted hands now report a turning speed too, from their orientation a frame ago
(vr::AngularVelocityBetween, quaternion_test), as SteamVR does. Measured [M,
weapon-throw-spin.txt 2026-10-26 11:07]: the hand turned 10 degrees in the throw's frame,
"spun at 13.2 radians a second (2.10 turns), its body keeps 13.2"; weapon-throw.txt without a
turn: no spin, the throw as before. Not judged: the feel - whether a real wrist's flick gives
too much tumble; the strength is the knob. Thrown world objects held by the grab (not a
weapon or torch let go of) take the hand's turning only as before, in their speed (v + w x r),
not as a spin.

### 5.9 The torch's blow: a stagger, a knockdown only with a power swing (fixed 2026-10-26)

Nadi, 2026-10-26: "Mit torch ist es aktuell zu einfach gegner zu boden zu hauen... evtl ini
anpassen sodass fackel hit zwar npcs recoiled aber nicht gleich umhaut. umhauen erst mit
ordentlich schwung whl soviel wie der schwerer angriff hat beim fighten".

**Cause, read in his log** (artifacts/nadi-win-live/20261025-211700): the torch hand is the
off hand, and with a one-hander drawn its blow is the off hand's strike (GrappleLogic.h
OffHandStrikeFor, controls-spec 4.34): from ShoveSpeed a stagger, from ShoveHardSpeed a
knockdown - by the hand's speed alone. His live INI has ShoveHardSpeed=2.4 (the template 3.2):
"the left hand at 2.4 m/s towards 184D904C - hard (the off hand, a weapon drawn)", "knocked
down", with the parry lines of those frames measuring the blades "from the torch" [M]. Three
knockdowns of that fight came that way, at 2.4, 3.2 and 2.6 m/s (a fourth later, at 3.1 m/s,
has no torch line near it - not known whether the torch was in the hand).

**Fix** (game/TorchBlowLogic.h, torch_blow_test 14 flows; hand_mode_test 5 more for the other
hand's swing length; red first). `[Hands] TorchKnockdownPowerOnly=1` (default; 0 is the speed
alone, as before):
- The speed still decides whether the torch's blow lands (ShoveSpeed); landed, it staggers.
- A knockdown wants a power swing of that hand - as long as the weapon's power attack
  (PowerSwingMetres, a thrust PowerThrustMetres; vr::SwingIsPower on the other hand's own swing
  detector, which runs with a weapon drawn too).
- The torch meets a body before its swing is that long, so a blow met while the swing is not
  yet a power one is held to the swing's end and lands as what the whole swing was - the
  weapon's strike by motion holds its bodies the same way (MeleeHit.h SettleHeldStrike, reused).
  The sound and the controller's pulse come as it meets them; the stagger or the fall when the
  swing is over or has become a power one. A body gone or dead by then takes nothing
  (RefFingerprint, game::ActorStillLiving).
- The torch's blow is a blow, never a slap, whatever the fingers round it do: an open hand on
  the torch slapped the face in the first harness run and set off Put it in its Place's slap,
  which moved the beggar away (the slap's -40 disposition with it).

**Measured** [M, 2026-10-26 11:44 and 11:46, Linux]: torch-blow.txt PASS - a 0.70 m slash at
49.2 m/s (past the 2.4 hard speed by far) "held to the swing's end ... by the speed alone
hard", "held 0.07 s - its swing ended short of a power one: staggered"; a 1.6 m slash met him
0.80 m in, "held 0.04 s - its swing grew into a power one: knocked down"; no blow in the face.
torch-blow-speed.txt (the control, TorchKnockdownPowerOnly=0) PASS: the same 0.70 m slash
"hard ... - the torch, by the speed alone", knocked down at once.

**Not changed:** the bare off hand (a fist or an open hand) with a weapon drawn still knocks
down from ShoveHardSpeed by its speed alone; the shove with the weapons away likewise. **Not
judged:** the feel in the headset - whether the hold to the swing's end (0.04-0.07 s in the
harness, longer for a real arm's follow-through) reads as a late reaction.

### 5.10 A torch in the world taken at its grip by the other hand (built 2026-10-27)

Nadi, 2026-10-27: "Fackeln sollen wie waffen aus der welt aufgreifbar am griff wo sie dann
auch gehalten werden, kreis und hand tooltip bekommen und dann dort aufgenommen werden können
per bücken und greifen oder teleport. Und dann immer aktiv mit flamme."

**Built** (game/TorchGrab.h, torch_grab_test; the glue beside the weapon's in CameraHook).
`[Hands] GrabEquipsTorches` (default 1).
- *What counts as a torch:* a light whose base has the carried bit (+0x7C, 0x02), so Torch02
  and its kin, never a lamp of the cell (game::RefIsTorch).
- *The hand:* the other hand takes it. The engine has one torch slot and shows it in the left
  hand only (section 2.3); a torch in the weapon hand would be decision 8's large job. So the
  weapon hand carries a torch as any object, as the other hand carries a weapon
  (game::IsTakingHand). Left-handed play follows the game's roles, as the weapons do.
- *The grip and the ring:* the weapon pick (5.4) takes the torch too: the ring and the name sit
  on the grip, the reach is WeaponGripReachMetres from the other hand's controller, a touch
  elsewhere on the torch carries it. The grip is 2.7 units below the model's origin along +y,
  the palm's place on the torch in the hand, measured [M, torch-grab.txt 2026-10-27: "in the hand
  the palm sits at -0.8 -2.7 -1.4 on the torch's model"]; Torch02.nif's shaft runs -14.4 to 33.5
  along y, the flame at 27 [R, pyffi].
- *The take:* the other hand's hold closing at the grip, or closed and arriving there, once
  per closing (the weapon's latch, its own), takes it with the game's activation (its owner's
  rules, the pickup sound at the grip) and equips it with EquipItem, silent; done once the
  game shows it worn (game::StepTorchTake, given up after two seconds, cancelled by a menu).
- *The pull* (PullToHand): a torch out of touch floats to the other hand on a grip and is taken
  and equipped on arrival, without the pickup sound - the weapon's pull steps with the other
  hand's hold.
- *The flame:* a torch in the hand burns as the engine always lights it there (0x00478270,
  5.7) - also one that lay dark on the floor. Not checked in a picture this round [D].
- *Let go:* as 5.7 - the hold that took it is closed on it, so the first opening drops it,
  burning; as the weapon taken at its grip does (B&S).
- *Gates* (game::TorchTakeGateFor, said on the pick line): the setting, the world without a
  menu, fit, grab in flight or death; not with a bow drawn (the bow is in that hand); not with
  a shield or a torch already on that hand; not while the holster has the grip. A gated take
  is a carry.

**Measured [M]:** torch-grab.txt PASS 2026-10-27 (the left controller 0.05 m from the grip,
taken, "equipped - in the other hand"), torch-pull.txt PASS (pulled from 0.21 m, taken on
arrival, equipped); weapon-grab, weapon-pull and torch-drop-pickup PASS again after.

**Open.** The headset; a shield worn with the fists out is not on the hand, so the torch take
is allowed and EquipItem swaps it for the shield, as the game does when a torch is equipped
[unverified]; whether the dark-floor torch should already burn before it is taken (cheap: the
5.7 lighting on every carried light in the cell, but that changes vanilla's world).

### 5.11 Both hands on the handle by an ordinary grip (built 2026-10-27)

Nadi, 2026-10-27: "2 hand griff der linken hand sollte schon ab normaler griff Stärke ähnlich
dem ziehen von waffen als voller griff zählen, nicht nur auf 100 Prozent mit press so wie B&S."
The two-hand take was one of the grips 5.4 had left on the force click (the binding's 0.3 of
the force sensor, a squeeze). It now reads the left hand's hold (vr/GripHold.h) as the holsters
and the world weapon's grip do: the fingers closed round the handle (the pull from 0.8, the
force near 0 by 5.4's measurement) take it, opening them below 0.4 lets go; TwoHandHolds'
flicker guard stays. `[Hands] GripByPull=0`, or a controller without the pull, keeps the click.
The hand script gives a grip's force apart now (`left force 0`, the click from 0.3 as the
binding), so a scenario can close the fingers without the squeeze: two-hand-hold.txt.
Still on the click: the grab of objects, the bow's draw, the stow.

### 5.12 Four places on the body, a sign at each (built 2026-10-27)

Nadi, 2026-10-27: "Waffen, schilde und torch kann ich nun überall holstern wie in B&S.
2händer und staff und Bogen aber nur rücken rechts oder links. Torch, one hander und pfeile
linke oder rechte Hüfte ... Mit kleiner Anzeigetafel wie in B&S." Asked: shields at all four
places, the quiver at the hips and the shoulder, the sign shows the name while a hand is at
the place, visible on the body later (stage 2, the hips only - the back is not seen in first
person). The hand split, told to the players (Nadi: "nur waffenhand für waffen und pfeile,
offhand für schilde, bogen und torches"): it is the engine's own (one weapon slot, the right
hand's; the bow held in the left; the shield and the torch the left hand's slots, 2.3).

**Built** (vr/Holster.h, holster_test; game/HolsterSlots, game/HolsterSlotRecord.h,
holster_slot_record_test; ui/HolsterSignPainter, holster_sign_painter_test):

| Place | Takes | Zone |
| --- | --- | --- |
| left hip, right hip | one-handed weapons, the torch, a shield | HolsterOneHand* on both sides |
| left shoulder, right shoulder (behind) | two-handed weapons, staffs, the bow, a shield | HolsterBow* left, HolsterTwoHand* right |

- *One thing per place*, kept as its base form id; one form at one place at a time
  (`KeepAtSlot`). In the co-save beside the quick menu's pages (record 'HSLT', version 1,
  ids resolved through ResolveRefID; a plugin gone leaves its place empty); cleared for a new
  game. Before this the holsters remembered the last weapon of each kind seen this session,
  and the shield and torch per place for the session only.
- *The reach* (`StepHolster`): a hand's hold closing at a place. The weapon hand: a drawn
  weapon put there (sheathed, kept there) when the place takes it and keeps nothing else; an
  empty hand draws the weapon kept there (equipped first when another is in the slot); a
  place keeping another weapon of the weapon hand's with one drawn: it goes back first.
  The other hand: the drawn bow put at a shoulder; a shield or torch put at a free place
  that takes it (taken off into the pack, the game's item sound); an empty hand takes back
  the bow, shield or torch kept there (a shield or torch waits while a two-hander or the bow
  is drawn). With the bow drawn the weapon hand's reach is the quiver's.
- *Letting go* (WeaponHold, TorchDrop): the drawn weapon, or the torch, opened at a free
  place that takes it is put there; at one that does not take it it falls as anywhere.
- *A weapon equipped in the inventory* and kept at no place goes to its kind's place on its
  side when that is free (`DefaultSlotFor`: Holster*Side, auto = a right-hander's left hip,
  right shoulder, left shoulder, mirrored left-handed) - the old reaches work without a stow.
  A weapon dropped is forgotten at its place; one that will not equip (sold) is forgotten
  when the reach gives up.
- *The wrong hand, the wrong place*: nothing done, and said - the log's line, and the sign's
  hint for 2.5 s: "Weapons: weapon hand", "Bow, shield, torch: other hand", "Goes at the
  hip", "Goes on the back", "Place taken", "Put the weapon away first". The sign also hints
  before a reach, while a hand that would get it wrong is at the place (`SignOutcomeFor`).
- *The sign* (`[Hands] HolsterLabels=1`): what the place keeps or "empty", and the hint.
  Logged on each change ("Holster sign: at ... - ..."). Warmed with the other overlays at
  the first world frame. Since 2026-10-28 (Nadi: "teilweise zu weit weg vom blick und zu
  dominant, mach die dezenter wie B&S und sichtbarer. ausserdem kommen die zu schnell oder
  competen mitteinander"; vr/HolsterSign.h, holster_sign_test):
  - *Look* - B&S's prompt is a small white "Holster weapon", red when the slot is full or
    does not take the weapon (r/BladeAndSorcery,
    https://www.reddit.com/r/BladeAndSorcery/comments/sbo54y/). So white text on a half
    see-through plate without an edge, the hint in a soft red, 0.11 m wide (was a gold-edged
    dark plate, 0.16 m).
  - *Place* - it was at the place: the hip's about 80 degrees below a level gaze, the back's
    0.29 m before the face (0.16 m there is about 30 degrees of the view). Now 0.55 m from
    the eyes, 24 degrees below the gaze and 14 degrees to the place's side, following the
    head with a 0.12 s ease: about 11 degrees wide, seen without looking for it, out of the
    middle. Where B&S hangs its prompt is not what this follows: Nadi asked for it nearer
    the view.
  - *Timing* - a hand has to stay 0.3 s at a place before it comes (a swing past the hip
    shows nothing); the hand it came for owns it while it stays and 0.2 s after, so the
    other hand cannot take it over; both arriving together, the weapon hand's. A hint comes
    at once and takes it. Fades in over 0.12 s, out over 0.2 s; a menu or a load hides it
    at once.
  - **Open (Nadi, 2026-10-28 night, for the next day): "die waffen tafeln sollen nicht
    kommen wenn sie eh leer sind".** A place that keeps nothing should show no sign at
    all - today it says "empty". Still wanted: a hint (wrong hand, place taken, put the
    weapon away first) when a reach there would do something or is refused. Decided
    (Nadi, 2026-10-28 night: "ja"): an empty place that would TAKE the drawn weapon
    still shows "Holster weapon", as B&S shows its prompt only then; an empty place with
    nothing to put there shows nothing. **Built 2026-10-29**: vr/Holster.h
    `EmptyPlaceSign` ("Holster weapon", or bow, shield, torch, for what the hand carries
    when the place would take it) and `HolsterSignWanted` (the kept thing, a hint, or that
    text - else no sign), holster_test "The sign at an empty place". Harness
    holster-places.txt (rejects `- "empty"`): at the empty right hip with the sword drawn
    the sign said "Holster weapon", no "empty" anywhere. Headset open.
- *Arrows*: either hip gives an arrow too (`[Hands] QuiverAtHips=1`, vr/Archery.h `AtQuiver`
  with the hips), the shoulder's quiver as before.
- *The hip's sheath*: a one-hander kept at the right hip hangs there (the game's SideWeapon
  node mirrored, vr/SheathMirror.h), for either hand; the back's and the quiver's nodes as
  before.
- In the game's texts: the controls card (weapon places, other hand), the start guide's line,
  the archery tutorial card, the settings row, README.

**Measured [M], headless, 2026-10-27:** holster-places, holster-shield-torch, holster,
holster-two-handed, holster-staff, holster-left-handed, torch-stow-cycle,
weapon-release-holster and holster-pull PASS. Seen on the way: the harness save keeps an
Elven Long Sword equipped, so a sword the console equips goes to the other hip
(`PlaceForUnkept`); near its carry limit the save shows the tutorial's "more than you can
carry" box over the world when a claymore is added (the scenarios set Strength 100); the old
reach scenarios opened the weapon hand after a draw, which WeaponHold (5.4, 2026-10-24) turns
into a drop - they run with WeaponHold=0 now.

**The take's hold (fixed 2026-10-27).** Nadi, Windows: "wenn einmal ausgerüstet dann am
controller Hold halten (oder sie geht in die spielwelt) statt toggle grip press". His log: after
"reach for the torch - taken back" no "the hold closed on the torch" followed - the holster kept
the grip that took it (its claim), and the torch's release (TorchRelease) leaves every hold the
holster claims alone; so opening kept the torch, and only a second closing armed it. Now the
holster says when its claim took the item into the hand (`HolsterVerdict::leftTook`: a take of
the shield or torch, the bow's draw), and that claim is the item's: the hold holds it, opening
lets it fall - as a weapon taken at its grip. torch-take-hold.txt PASS; holster-shield-torch.txt
runs with TorchDrop=0 now (its steps open the hand after each reach).

**The torch's grace (built 2026-10-27).** Nadi: "wenn ich aus dem inventar eine fackel aktiviere
dann kommt die in die linke hand und gibt zeit um die hand zu schliessen damit sie nicht sofot zu
boden fällt. aber ich muss sie schon halten, sonst fällt sie". `[Hands] TorchGraceSeconds=2`
(game/TorchRelease.h, torch_release_test): a torch newly equipped - the inventory, a hotkey - into
an open hand has that long in the world (menus and holds that are not its own stop the clock) for
the hand to close on it; never held, it falls where the hand is, not thrown. Equipped into a closed
hold (from a place, from the world) it is held at once. A load brings no torch to hold: the glue
counts a torch as new only when it shows without a load between (CameraHook, the other hand's
items). 0 keeps a torch in an open hand as before. The 24 scenarios that equip a torch by the
console into an open hand run with 0; torch-grace.txt PASS (the first torch fell after two seconds
never held, the second, held within one, stayed five and fell when let go).

**Not built (stage 2, Nadi's go needed):** an item kept but not equipped seen on the body.
Only the equipped weapon shows, at its vanilla node (the hip one moved to its side). The
cost is the model of an unequipped item hung on the body - large: the render state of a 3D
put into the scene (the torch flame's crash, 5.7) and its place in the save; at the hips only,
as Nadi asked, it saves two of the four mounts, not the core.

### 5.13 A throw looks back in time, not in frames (built 2026-10-28)

**Reported (Nadi, Windows run of 2026-10-27):** "Fackeln werfen ging oft aber
nicht immer. Waffen werden dann whl ähnlich."

**Read from that run's OBVR.log [R]:** five torches let go. One left as a
throw (the hand at 4.18 m/s, spinning 10.9 rad/s), two as a fall (0.10 and
0.21 m/s after the ease, about 0.55 and 0.8 m/s before it), two without a
line - the torch's line budget was spent. A pause menu cancelled a sixth
(as it should). Whether the two falls were throws cannot be read: the log
had only the speed the throw took.

**Cause [D, from the code]:** the throw's speed was the fastest of the last
8 frames before the hold read open (`kThrowHistory`), 89 ms at 90 Hz. A hand
opened a little after the swing's peak has its peak outside that, and the
torch falls with the hand already slowing. The weapon's release
(`WeaponRelease.h`) and a grabbed thing's (`GrabPhysics.cpp`) took the same
history, so weapons behave the same.

**Built:** the history keeps each sample's frame time; the throw takes the
fastest of the last `kThrowWindowSeconds` (0.15 s, a first guess) before the
opening, the same time back at any frame rate (`FastestWithin`). Every let go
of a torch or a weapon now logs, outside the line budget, its fastest in that
window and in the last 0.5 s with how many ms before the opening it was
(`ThrowPeaksOf`): "its fastest n m/s in the last 0.15 s before it opened, m
m/s in the last 0.5 s (t ms before)". frame_logic_test (opened 0.11 s after
the peak still the peak, 0.22 s after not, 144 Hz the same time, no frame
time a 90 Hz frame, full, the peaks, none, older than the half second),
torch_release_test and weapon_release_test (opened 11 frames after the
peak: thrown with it, the verdict carries the peaks). The ease (a tenth at
rest to all of it at 3 m/s) is unchanged. Headset open: if a throw still
falls, the line says whether its peak was older than 0.15 s (the window) or
in it and slow (the ease).

Harness 2026-10-28 (headless): torch-drop-lit, weapon-drop, weapon-throw,
weapon-throw-swing, weapon-throw-spin PASS, each let go logging its peaks
(weapon-throw-swing: "its fastest 4.63 m/s in the last 0.15 s ..., (80 ms
before)" - 80 ms is within the old 8 frames too). throw-hit.txt FAIL as on
2026-10-26 already: the right grip now takes the dropped sword into the hand
(the weapon grab), so it is never thrown as a held thing - the scenario is
older than the weapon grab, not changed by this.

### 5.14 Handing a thing from one hand to the other (wish, 2026-10-28, not built)

**Wanted (Nadi, 2026-10-28):** "ich kann die waffe schild torch oder gegenstand also im grunde
alles von einer hand in die andere reichen" - the weapon, the shield, the torch or any item.

**Proposed gesture:** the free hand closes its hold on the held thing (on its grip or within reach
of its mesh), then the first hand opens: the thing stays in the second hand. As in real life, and
it extends what exists: a second hold on a two-hander's or a one-hander's handle is a two-hand
hold today (5.11); the first hand opening while the second holds is the handover. The first hand
opening with nothing else holding stays a drop or a throw (WeaponHold, TorchRelease).

**What the engine allows [R, section 1 and holding-objects-spec "Deviations"]:** one weapon slot,
shown only in the weapon hand; a shield and a burning torch only in the other hand. So by kind:

| Thing | To the hand that can wear it | To the other hand |
| --- | --- | --- |
| One-hander | equipped (the world grab's take steps, `BeginWeaponTake`) | unequipped, dropped and held there as an object (decision (b) of 2026-10-28) |
| Shield | equipped (EquipItem) | held as an object, unequipped |
| Torch | equipped, burning | held as an object - burning in the weapon hand is unverified |
| Two-hander, staff, bow | stays equipped; which hand counts follows the two-hand hold | - |
| Any other item (held object) | the grab passes to the other hand | the same |

Back to the hand that can wear it equips it again (the grip on a held weapon's handle already
does, holding-objects-spec decision (b)).

**Exists to reuse:** the world weapon taken at its grip (game/WeaponGrab.h), the torch taken at its
grip by the other hand (c2d5c95), the two-hand hold (vr/GripHold.h, 5.11), the held object's grab
with either hand, the drop straight into a grab (holding-objects-spec, inventory step 4),
EquipItem and the take steps.

**Open, to decide:**
- whether the handover needs the second hand on the grip, or anywhere on the thing;
- a held object (not equipped) handed over: the engine's grab holds one reference - the pass
  from one hand's grab to the other's is new;
- the one-hander's two-hand hold today keeps it in the weapon hand when the left hand opens; the
  weapon hand opening with the left on the handle drops it (WeaponHold) - that becomes the
  handover.

**Cost: medium.** The equips, the take, the drop into a grab and the two-hand hold exist; new
are the handover's decision (pure, unit flows per kind and hand: hand to hand, back, refused
kinds, quest items that cannot be dropped) and the grab's pass between the hands. Risk: the torch
in the weapon hand, and the engine's grab moving between hands without a drop. Test: a harness
scenario per kind (sword, shield, torch, a potion), each handed both ways.

**Narrowed by Nadi, 2026-10-28** ("nene das ist crappy. was ich akzeptieren könnte"): the weapon
hand's sword handed to the other hand is held there as an object, wherever that hand took it (the
grip too) - it can be thrown or let go; the weapon hand's grip on its handle equips it again.
Read for it [R]:
- the drop into the other hand's grab is built and measured: holding-objects-spec step 4,
  inventory-grab-hold.txt PASS (the Iron Longsword dropped at the left hand, in its grab after
  0.05 s, let go when the hold opened); here the drop comes from the equipped weapon
  (UnequipWeapon and `DropEquippedWeapon`, as WeaponHold's drop) instead of the inventory;
- throwing and letting go a held thing are the grab's (GrabPhysics, 5.13's window);
- the weapon hand's grip on a weapon's handle in the world equips it (game/WeaponGrab.h). Not
  checked: whether its pick takes a weapon the other hand's grab holds right now, and the other
  grab let go when it does - the one new piece there;
- "wherever it was held": the grab keeps the hold where the hand took it [unverified for the
  engine's grab - it may pull the thing's centre to the hand; the harness picture tells].
Cost: small to medium. Shield, torch and items handed over: the same path later, not asked now.

**Built 2026-10-28, step 1 (Nadi: "go a" - eating stays at the let go):**
- The sword handed over: the weapon hand's let go (WeaponHold's drop) with the other hand's hold
  closed within `[Hands] HandoverReachMetres` (0.12, a first guess) of the drawn weapon's line is a
  hand-over: dropped as it was held, no throw, and the other hand's grab holds it while that hand
  stays closed (the inventory's step-4 hold, `g_inventoryHold`). Farther, a drop or a throw as before.
- Taken back: the weapon hand's hold closing within the reach of the held one-hander's bound -
  the other hand's hold let go first, the take (TakeIntoInventoryQuietly, then the take's steps)
  only once the engine's grab let go of it, given up after a second (`StepHandBack`).
- game/HandoverLogic.h, handover_test. Log: "Handover: ...".
- handover-sword.txt PASS 2026-10-28 11:33 (Linux, headless): the left hand closed 5.6 units
  from the drawn Iron Longsword, the right opened - "handed over", in the other hand's grab after
  0.05 s; the right hand closed on it - "taken back by the weapon hand - equipping", "equipped
  and drawn - straight into the hand". eat-at-mouth.txt and weapon-drop.txt PASS (no change to the
  drop or the eating).
- Not seen: whether the left hand holds it where it took it - the run's pictures are the
  monitor's, the hands not in them; the headset tells.
- Eating, with it: the held thing's bound within `[Hands] MouthThingReachMetres` (0.05, a first
  guess) of the mouth counts as the hand at the mouth does, the let go still eats (vr::ThingAtMouth,
  stow_test; the "Eat: at the mark" line gives the thing's distance).
- Next (step 3, not built): the bow, the shield and the torch from the other hand to the weapon
  hand as objects and back; any other item from hand to hand.

**Step 3 built 2026-10-28, harness half run (stopped: Nadi tests on Windows):**
- The bow and the torch from the other hand to the weapon hand: the other hand's let go (the bow's
  WeaponHold drop, the torch's TorchRelease drop) with the weapon hand's hold closed within
  `HandoverReachMetres` of a ball of `HandoverThingMetres` (0.15, a first guess) round the other
  hand - dropped as held, in the weapon hand's grab, the torch burning. The shield has no let go
  (the arm wears it): the weapon hand closing there, not at a holster place, takes it off and
  holds it.
- Back: a hand closing on what the other hand holds chooses by `ChooseInventoryTake` - equipped
  when it can wear it (the weapon hand a weapon; the other a bow, a shield, a torch), else passed
  and held there; a thing the ordinary grab holds is taken as the grabbing hand's.
- HandModeFrame.grabHand: the grab's line from the hand OBVR keeps the thing in, whatever grip
  buttons are down (hand_mode_test).
- Harness 12:0x (before the fixes below): handover-torch, eat-at-mouth, weapon-drop PASS;
  handover-sword: the game ended right after the take back (the take came in the frame the grab
  let go, before the grab physics' release - now a frame later, the grab physics standing back);
  handover-shield: taken off and held, the take back "refused by the game" (the taking hand's
  own grab kept off it now - unverified as the cause); handover-item: passed to the left, the
  thing stayed on the right hand's line (grabHand); handover-bow: handed over, never in the
  weapon hand's grab (not explained; the pick may miss it). The fixes are not run yet.
- **Open bug, harness 2026-10-30 12:09-12:14 with the fixes (DLL c7338b0f):** handover-torch PASS,
  the other four FAIL. handover-sword: the game still ends after the take back - and the take did
  not go through the hand-back path at all: "Handover: the weapon hand closed ... the weapon hand
  takes it", then the ordinary pick and "Weapon grab: 205786CC taken at its grip ... equipping",
  "on the hand 0", the holster line, and the log ends. handover-shield: "NOT taken back by the
  other hand (refused by the game ...; the engine's grab on 00000000)" - the own grab was not the
  cause. handover-bow: "the weapon hand opened after 5.47 s - it was never held". handover-item:
  never "passed to the weapon hand". Causes not found yet.

