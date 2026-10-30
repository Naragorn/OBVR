# Dismemberment: the killing blow cuts (design 2026-10-25; the corpse cut built 2026-10-30)

The tester (2026-10-25): "decapitations und andere limbs wie ist das möglich
für obvr in 2026, durch mods oder durch eigene changes?" - and, after the
options: "B erstmal nur spec". B is the rule of section 1: health decides
whether someone dies, the blade only decides how.

Markers as in the other specs: **Read** (seen in the code, a file or a
source), **Derived** (concluded from what was read), **Unverified** (still
to be shown), **Proposed** (a design).

The engine background, the measured skins and how other engines do it are
in `../OBNG/knowledge/dismemberment.md`; this spec is what OBVR would build.

## Built 2026-10-30 (Nadi: "go"): the rule and the cut on corpses

- **The rule** (`game/DismemberLogic.h`, dismember_test): `WeaponHasEdge`
  (a blade type, or "axe" in a blunt weapon's model path), `NeckTouched`
  (the blade now and the paths of nine points along it from the frame
  before, against Spine2 to the head, `neckRadiusUnits` 6), `DecideCut`
  (off, already cut, alive - the living keep their parts unless
  `DismemberCutKills`, never the essential - no edge, not the neck, too
  slow; else cut). `[Hands] Dismemberment=0` (off until the headset),
  `DismemberMinSpeed=2.5` m/s of the tip, `DismemberCutKills=0`.
- **The cut** (`game/Dismember.cpp`, section 6's reading): the head's joints
  to Spine2 taken out of the world (bhkConstraint vt+0x60) and out of its
  list (0x0067A850) inside the Havok lock, one reference kept; the skins'
  bone pointers redirected - body-side shapes' head bone to an OBVR node at
  the cut following Neck1 (scale 0.001), head-side shapes' other bones to
  one at the head; a shape is head-side when its bound's middle is nearer
  the head than Spine2 (the head mesh is not under the head node - the first
  run classed it by the tree and swapped nothing on the head side); the
  head's body woken and sent off (`ThrowBodyOfNode`) - a corpse's bodies
  sleep, the first run's head stayed at the cut, 0.0 units off.
- **In the game:** each frame with a blade drawn, the necks within 160
  units of the tip (`CollectNecks`, the loaded actors, dead or alive) are
  tested; a cut pulses the weapon hand. A cut actor whose 3D changes is
  forgotten (the co-save redo of section 3 is not built).
- **Harness** `dismember-corpse.txt` PASS 2026-10-30 15:01 (20261030-150109),
  [Debug] TestCut at the mark "cut" (no blade): "1 neck joint(s) released,
  4 skinned shape(s) redirected (1 body-side bone pointers to the neck, 20
  head-side to the head: Plane.004(head), Cuirass.003(body), Plane.00(head),
  FaceGenFace(head)), the head woken and sent off"; at the marks after, the
  head 32.0 units from the cut and resting. **Seen 15:27** (20261030-152704,
  the bandit placed 200 units ahead, the scenario's `MonitorMenusOnly=0` -
  the live INI's 1 had made every world picture black): before the cut the
  corpse lies with its head on; after it the head with its hair rolls
  58.5 units away, the body's collar closed, no stretched neck (accepted by
  Nadi from the pictures: "zeig mir die bilder vom harness reicht mir als
  test").
- **The killing blow** (built later the same day, Nadi: "passt. weiter"):
  the engine's hit kills inside the call (74 -> -401 health) and flings the
  body before the blade's sweep reaches the neck, and the capsules do not
  tell the neck well (a slash at neck height met the right arm first, a
  thrust at it the torso's top), so: `StrikeByMotion` records each blow that
  killed (`StrikeKilled`, the touched part by `NearestTouchedPart`), and the
  next frame cuts it when the part is the neck (`PartTakesHead`) or the
  blade's sweep - the frame before to now, and on its way for
  `kKillingSweepSeconds` 0.08 s more - passes through the neck as it stood
  the frame before (`KillingSweepMeetsNeck`), with an edge and fast enough
  as before. A living neck the blade went through also waits
  `kKillingBlowSeconds` 0.4 s for a death that comes later
  (`ArmsKillingBlow`, `StepPendingCut`). **Deviation from section 1:** the
  "sweep on" is a prediction - the blade had not yet reached the neck when
  the blow killed; without it a side slash at neck height would almost never
  cut.
- **Head-side by bones**, not by the bound (2026-10-30): a standing beggar's
  shirt had its bound's middle a hair nearer the head than Spine2; a shape
  is the head's when a third or more of its bones are the head, under it,
  Neck or Neck1 (`ShapeIsHeadSide`; measured FaceGenFace 2/5, shirts 2/10
  and 2/19, the bandit's cuirass 2/16 and its Plane.* 2/10, 2/8). The bound
  stays the fallback when the bones are not read. The head's push is capped
  at 350 units/s (`CutPush`).
- **Harness** `dismember-killing-blow.txt` PASS 2026-10-30 17:51
  (20261030-175107): a frozen beggar, blade and strength 100, a thrust at
  the neck: "the killing strike on ...'s torso at 20.6 m/s (an edge, its
  sweep on through the neck) - the killing blow cut", FaceGenFace head-side,
  the shirts and feet body-side. The pictures do not show it (the body is
  flung out of view). `dismember-corpse.txt` again PASS 17:52 with the bone
  rule, its pictures as at 15:27 (the collar closed, the head rolled off).
- **Telling Walkers** (section 2.5; the format agreed with the Walkers
  session 2026-10-30): `'WGDM'` one word, 1 when `Dismemberment` is on -
  said at the world frames until Walkers takes it (three frames at most per
  setting) and again when the setting changes; `'WGHD'` {form id, kind}, kind
  1 a cut, at every cut (`CutHead`); kind 2 (a crushed head) is defined, not
  sent. Pure parts in `DismemberLogic.h` (dismember_test), the sending in
  `game/WalkersLink.cpp`. Harness: both dismember scenarios PASS 18:47/18:49
  with "'WGDM' 1 taken" and "'WGHD' FF000F81 1 taken". Walkers' side (its
  spec 4.17) built there the same evening (544d840, fix e25888a): the
  end-to-end runs dismember-walker (a beheaded bandit does not rise) and
  dismember-walker-real (a beheaded walker does not get up again) PASS
  19:18 and 19:19.
- **'WGPS', the power strike told** (Nadi: "Go", 2026-10-30 evening): xOBSE's
  `IsPowerAttacking` looks for a running AttackPower sequence and OBVR's
  strike plays none (it sets the attack group's byte for the hit's call
  only), so Walkers could never see a power kill in VR. Now `ApplyStrike`
  sends `'WGPS'` {form id} right before the engine's hit on every power
  strike (the weapon's, the fist's, the running kick's);
  Walkers keeps it to the next frame tick and ends a walker that dies in
  it. `TellsPowerStrike` (dismember_test). Harness
  `walkers-power-kill.txt` written, not run yet (SteamVR down, 20:12).
- **Not built:** blood, wound caps, save and load, hands. The blade's
  sweep through a real corpse's neck is not run in the harness (aiming a
  hand-script blade at a ragdoll's neck); its geometry is dismember_test's.

## Verdict

**Proposed: a head (later a hand) comes off when a blade's hit that kills
lands on it fast enough, or when a blade cuts a corpse there. The living
never lose a part.** Size: M-L. Nothing is built; three engine unknowns
(section 6) gate the build, and the cheapest steps to clear them come
first (section 8).

- Oblivion's balance stays: health, armour and level still decide how long
  a fight lasts. A fast neck hit in VR is easy; a cut that kills by itself
  would end every fight at the first good swing (rule A, section 1).
- The cut is computed, not authored: it works on every body, armour and
  clothing a mod brings, without new meshes per item.
- No mod needed. Deadly Reflex, the only Oblivion mod that cuts, does not
  fit OBVR (section 7).

## 1. The rule (Proposed)

A cut happens when all of these hold:

1. **The hit kills.** The actor dies (`IsDead`, TESObjectREFR index 0x66,
   GameAddresses.h) within a few frames of a blade contact OBVR recorded on
   it. The contact is kept per actor: the part, the blade's speed at the
   contact, its direction, the frame. A hit that leaves health above zero
   cuts nothing, wherever it lands.
2. **On a part that can be cut.** The contact's `BodyPart`
   (BladeContactLogic.h) is `Neck` or `Head` - the head comes off. Later:
   `LeftArm` / `RightArm` at the wrist end of the forearm capsule - the hand
   comes off. Nothing else at first.
3. **With an edge.** Blades (`WeaponTypeCode` BladeOneHand, BladeTwoHand)
   and axes. Axes are Blunt weapons in Oblivion's data (Deadly Reflex had to
   test the name for "axe" - Read in its readme); how OBVR tells an axe from
   a mace is open (section 6). Daggers: open (Deadly Reflex made a dagger's
   decapitation a throat cut).
4. **Fast enough.** The blade's speed at the contact above a threshold, the
   counterpart of Blade & Sorcery's `dismembermentMinVelocity`
   (weapon-collision-spec 17.1). The value is to be tuned in the headset.
5. **Not refused.** Essential actors do not die, so they are never cut.
   Creatures wait (section 5).

**Corpses:** a blade contact on a dead actor's neck that meets rules 2-4
cuts as well. The ragdoll is already on, there is no timing with the death,
and it is the first harness case (section 8).

**Rule A, later and only as a setting** (`CutKills`, default 0): a cut that
kills by itself, whatever the health, as in Blade & Sorcery. The same code
with rule 1 dropped.

**Why the living keep their parts (Derived; B&S does the same):** on a
living actor the skeleton is driven by animation, not by the ragdoll - the
ragdoll comes on with the death (the Walkers' corpse-to-zombie work,
`../OBNG/knowledge/corpse-becomes-zombie.md`). A severed hand would need its
own bones and body, and the actor its weapon, its casting, its AI and its
animations without the hand. Blade & Sorcery's base game kills on every cut
limb; surviving one is a mod there.

## 2. The cut (Proposed)

### 2.1 What the skins are (Read, pyffi 2026-10-25, tools/nif-skin-bones.py)

- `headhuman.nif`: 1275 vertices; 1205 on `Bip01 Head`, 70 on `Bip01
  Neck1`, about 180 shared with Neck1, both Clavicles and Spine2 - the neck
  is part of the head mesh.
- `upperbody.nif` `UpperBody`: the neck ring is weighted to Neck1 and Head
  (82 vertices Head + Neck1 + a Clavicle + Spine2).
- `hand.nif`: both hands in one shape, each side on its hand and finger
  bones only; `upperbody.nif` `Arms`: both arms in one shape.
- `mouthhuman.nif`: not skinned - the mouth, and with it Derived the eyes
  and teeth, ride the head node.

So hiding a shape is not a cut, and freeing the head alone would stretch
the neck between the body and the head (the artefact Blade & Sorcery
players report, OBNG file).

### 2.2 The cut by bone pointers (Proposed; the technique is built)

OBVR already reshapes skins without touching their vertices or triangles:
the arm stump (game/ArmStump.h) swaps bone pointers in a shape's own
`NiSkinInstance` (+0x14, one pointer per NiSkinData bone) for OBVR's own
nodes with almost no scale, so every vertex hanging from those bones
collapses into one point and the skin tapers into it. The GPU skins from
those pointers each frame; nothing is re-uploaded. A cut is the same
operation on two sides:

- **The body side.** In every skinned shape still on the body (the
  UpperBody, a cuirass, a collar, a hood's lower part), the bones beyond the
  cut (`Bip01 Head`) point to a node at the cut that follows the body's side
  (`Bip01 Neck1`) with almost no scale: the neck ring's Head-weighted
  vertices close in on the cut.
- **The head side.** In the head's shapes (head, hair, helmet, beard), the
  bones before the cut (Neck1, the Clavicles, Spine2) point to a node at
  the cut that follows `Bip01 Head`: the head mesh's own neck tapers under
  the head and goes with it.
- **Shared vertices** (Head + Neck1 and the rest) move by their weights
  between the two, the same blend the arm stump's spine node handles.
- **Rigid children of the head node** (the mouth, the eyes, the teeth;
  maybe hair and helmets - Unverified) go with `Bip01 Head` by themselves
  if the head bone itself leaves (2.3, way 1).
- **Caps:** the taper closes the surface into a point. A wound cap (a disc
  with a flesh texture, sized from the bone's capsule radius - the fitted
  radius of mesh-fitted-capsules-spec, or today's guess) over each side is
  optional and its own asset.

Every swap is kept as the skin-swap records are (SwapRecordHolds, ebd89f3:
the record is used only while the skin still holds the bones it was made
from), so a load or an equip change cannot write into a skin that changed
under it.

### 2.3 The free part (Proposed; the engine call is Unverified)

1. **Way 1, the direct one:** release the ragdoll's neck constraint (the
   `bhkRagdollConstraint` between the head's and the neck's rigid bodies)
   from the Havok world. The head's body falls and rolls as its own; `Bip01
   Head` follows it, and so does everything under it. The call is not found
   yet (section 6).
2. **Way 2, if way 1 is closed:** an OBVR node takes `Bip01 Head`'s place in
   the head's shapes, the rigid children are moved under it, and OBVR moves
   it - a body of its own through the engine's Havok (GrabPhysics' calls), or
   a plain fall with a ground ray. More code and a worse roll; named here so
   it is not mistaken for the fix if way 1 is found.

### 2.4 Blood (Proposed, Unverified)

A spray from the neck at the cut and a decal under it. Whether the engine's
own blood (its impact data, `bDecalsOnSkinnedGeometry`) can be started at a
point and a heading from OBVR is open; an own particle mesh is the
fallback.

### 2.5 Telling other plugins (Proposed)

OblivionWalkers ends a walker only when its head is off (its spec 4.17,
`DieOnlyBeheaded`, default on), so OBVR tells other plugins through OBSE's
plugin messaging: at its start and after an INI reload whether it cuts
(`Dismemberment`), and each cut (the actor's reference and the part). A
second kind (Nadi, 2026-10-25): a killing blow to the head with a blunt
weapon ("head crushed", nothing cut, the contact's `BodyPart` Head), which
ends a walker as a cut does. Without OBVR's cut, Walkers ends a walker on a
power attack's kill instead; whether OBVR's power strike by a fast swing
counts there is for the Walkers harness. Cheap; no reply is awaited.

## 3. Save and load (Proposed)

The engine rebuilds the 3D and the ragdoll on a load, and the head would be
back on. OBVR keeps the cut actors in its co-save (the reference, the parts
cut) and redoes the cut once the actor's 3D is there again, the pointers
found anew by name (the fingerprint rule above). Where the free head lay is
not kept at first: it is laid at the neck and falls again.

## 4. Settings (Proposed)

- `[Combat] Dismemberment=0` until it passes the headset, then 1.
- `[Combat] DismemberMinSpeed` - rule 4's threshold.
- `[Combat] CutKills=0` - rule A.
- `Debug.CutTrace` (off): one line per cut (actor, part, speed, the shapes
  swapped, the way of 2.3) and per refusal (which rule).

## 5. Later

- **Hands** (rule 2's arm contact at the wrist): the same swap at `Bip01 L/R
  Hand` in the Arms, the hand and the gauntlet shapes. Small-medium once the
  head works.
- **Limbs at a joint** (forearm, upper arm, lower leg): the same, more
  shapes; medium each.
- **Creatures:** every skeleton names its head bone its own way; by bone
  names per creature skeleton, after humans pass.
- **Not planned:** cuts at any point along a limb (needs real triangle
  slicing), cutting in half, the living losing parts.
- **A plugin of its own for flat players, without OBVR** (Nadi, 2026-10-30
  evening: "können wir das beheaden aus obvr extrahieren oder replizieren
  in eigen dll für flat gamers"; the Walkers session's note the same
  evening, walkers-spec 4.17 "Open - flat with dismemberment"). The way
  Oblivion Frame Fixes was made: a xOBSE plugin hull after the FoliageAA
  pattern, the logic copied verbatim (no subtree; changes mirrored by hand).
  - *VR-free as it stands (read 2026-10-30):* `DismemberLogic.h` (the rule,
    the Walkers words), `Dismember.cpp` (the joint's release, the skins'
    swap, the head's throw, 'WGDM'/'WGHD'), `ActorRoster.cpp`, and from
    `FirstPersonHide` and `GrabPhysics` only `Named`/`NiClassNameOf`/
    `NodeChildren` and `HavokBodyOfNode`/`SetBodyVelocity` - about 900
    lines plus the hull's ~250 (Memory, Log, INI, Plugin.cpp exist there).
  - *VR-bound, to be replaced:* the trigger. OBVR cuts by the tracked
    blade's sweep and the touched capsule; the flat game has no blade path
    and no hit location. Rule agreed with Walkers: `Dismemberment=1` and a
    POWER-ATTACK kill with an edge (blade or axe) cuts - a detour on the
    player's melee hit 0x005FEBF0 (its third argument is the power flag;
    the health before and after says whether it killed), the head sent off
    along the player's facing; `Dismemberment=0` is a plain kill. The
    plugin sends 'WGDM' and 'WGHD' to "OblivionWalkers" as OBVR does;
    Walkers then listens to its name too.
  - *Coexistence:* with OBVR's hands on, the flat plugin stands down (else
    two cuts) - by OBVR's module or a message, as Frame Fixes yields at
    patched bytes.
  - *Shared gaps:* save and load, blood, creatures - whatever OBVR gets,
    the copy needs too.
  - *Cost:* medium - hull and copy small, the hit detour small to medium
    (the function is read), the risk in the test: OBVR's harness drives
    VR hands, a flat power attack needs the mouse held (the hand-script has
    keys, no mouse yet) or `player.playgroup AttackPower` plus the hit.
    Best after the headset passes OBVR's cut, so the visuals are settled
    before they are copied. Nothing built.

## 6. Unknowns that gated the build (answered 2026-10-30, below)

1. The Havok call that removes one constraint from the world, and how to
   find the neck's constraint on an actor's ragdoll (REA; the ragdoll
   grab's findings, GrabPhysics.h, are the nearest reading).
2. Whether an NPC's skinned shapes have their own `NiSkinInstance` per
   actor (Derived: the bone pointers point into that actor's skeleton, so
   they must; their vertex data is shared - a bandit's and Jauffre's `Hand`
   at one address, `worn-shapes.txt` 2026-10-25). Which head attachments
   are rigid is **measured** there: hair, ears, eyes, mouth, teeth, tongue
   and a Legion helmet are rigid shapes, so they go with `Bip01 Head` if it
   leaves (2.3 way 1); the face (`FaceGenFace`) is skinned to Head and
   Neck1. Beards and hoods not seen.
3. How to tell an axe from a mace (the model's name, the weapon's name, a
   list - none read).

**Read 2026-10-30 (all three answered; the full notes with file and line
citations in OBNG's decompile export are summarised here):**

1. **No neck body; one joint Head-Spine2 (Read, `_male/skeleton.nif` and
   `skeletonbeast.nif` via pyffi):** the `Bip01 Head` rigid body holds one
   `bhkRagdollConstraint(Head, Spine2)`. Found by: Head node -> collision
   object (0x0047FAC0) -> +0x10 the bhkRigidBody wrapper -> +0x10 its
   constraint list (inline first node: item +0x10, next +0x14; filled by
   LinkObject 0x008A50E0 through 0x008A46C0). On a bhkConstraint +0x08 is
   the hkConstraintInstance; on that +0x08 the owner (in a world), +0x10
   entity A, +0x14 entity B; an entity's +0x0C its bhk wrapper. **The
   removal (Read):** bhkConstraint vtable +0x60 = 0x008A0760, thiscall, no
   arguments, bool - GetWorld (vt+0x58), then bhkWorld::RemoveConstraint
   0x0088B600 (queued while the world is locked; hkWorld::removeConstraint
   0x008988F0 defers too). The engine removes single ragdoll joints this way
   itself (0x008A4BA0, 0x008A4D44-0x008A4D68) while the rest simulates.
   **Trap (Read + Derived):** the walker 0x008A5580(root, bool) swaps joints
   for fixed or malleable ones and later re-adds any joint not in the world
   (0x008A53E0; callers the knock state 0x006545E0, 0x006A3820, 0x004E0AB0,
   0x004E31E0) - so the cut must also unlink the joint from Head's list, or
   the head can snap back. Whether the Havok lock 0x00BA7B00 is needed:
   Unverified (both layers defer while locked; main thread derived safe).
   The vanilla grab is a mouse-spring action, no constraint call to reuse.
2. **Own NiSkinInstance per NPC: yes (Read).** The biped update 0x0047BC40
   clones each part's model per use (NiObject::Clone 0x00700610);
   NiSkinInstance::CreateClone 0x0072BFA0 makes a new instance and bone
   array (+0x14), sharing NiSkinData (+0x08) and NiSkinPartition (+0x0C);
   0x0047B090 rebinds the bones by name to the actor's skeleton. The FaceGen
   head's instance being a per-actor clone: Unverified.
3. **Axe or mace: the engine does not tell (Read)** - type +0x90 has only
   Blade/Blunt/Staff/Bow, no axe in the hit or equip sound names, editor ids
   not kept at runtime. Reachable: the model path (`[weap+0x34]`, GetModelPath
   0x00412AB0), the name (+0x28). Measured in Oblivion.esm: "axe" in the
   model path catches every vanilla axe; the name misses four (Shadowrend,
   Destarine's Cleaver, Perdition's Wrath, Defiler); seven enchanted vanilla
   axes are typed Blade. **Proposed:** an edge = Blade type, or "axe" in the
   model path (any type), plus INI lists of form ids to add or remove.

Not a gate any more: the vertex data. The cut by pointers reads no vertices,
so the open question of mesh-fitted-capsules-spec (whether NPCs' vertices
stay on the CPU) matters here only for fitted cap sizes.

## 7. Why not Deadly Reflex (Read, its readmes)

Version 6 (beta, 2010) cuts only through its own power attacks in slow
motion, which OBVR's swings by motion never play; it replaces the
first-person skeleton and animations OBVR's arms rely on; it needs NifSE,
whose state on xOBSE 22.13 I could not verify; it does not work with custom
bodies or robes. Sources in the OBNG file.

## 8. Cost and order

| Part | Size | Why |
|---|---|---|
| Reuse | - | the arm stump's pointer swap and its records, the capsules with their parts, the contact, `IsDead`, the co-save, GrabPhysics' Havok calls |
| Rule of section 1 | S | pure, a decision over plain values (the FrameLogic pattern) |
| The cut by pointers | M | the stump's technique on other shapes and on NPCs; the shared vertices; the rigid children |
| The free part | S-M with way 1, M without | way 1 needs one engine call; way 2 is more code |
| Blood, caps | S-M | assets or the engine's blood; open |
| Save and load | S | the co-save and the redo on 3D |
| Risk | - | the constraint call; NPCs' skins; head attachments; feel in the headset |
| Tests | M | pure flows of every rule and refusal; harness (below); the headset |

Order:

1. Read the three unknowns of section 6 with REA (small-medium, no game).
2. The rule's pure unit and its tests (small).
3. A corpse first: harness - kill a bandit, swing a blade through the
   corpse's neck, the head comes off, the body's neck closed; a slow swing
   or a mace cuts nothing; save and load keeps it.
4. The killing blow: harness - a neck hit that leaves health cuts nothing,
   the hit that kills cuts; an essential actor never.
5. The headset, then hands.

## 9. Tests (the FrameLogic pattern)

Pure, without the game: a lethal neck contact with a blade above the speed
cuts; the same below the speed, with a mace, on the torso, or not lethal
does not; an axe cuts; a contact older than the window does not count; an
essential actor never; a corpse cuts without a death; `CutKills` cuts
without a death; `Dismemberment=0` never. The swap: which bones go to which
node for a shape skinned to Head only, to Neck1 only, and to both; a shape
whose skin changed since its record is left alone.

Harness: the scenarios of section 8, with `Debug.CutTrace` lines and
pictures of the neck from both sides.
