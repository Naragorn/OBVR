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

## Built 2026-10-31: on by default, blood, the crushed head

Nadi, 2026-10-31: "1 hat geklappt ingame ... 3. ja das fehlt 5. go".

- **On by default** (`Dismemberment=1`): tried in the game by Nadi.
- **Blood** (`game/Blood.h`, `[Hands] DismemberBlood=1`; the engine's own,
  read in `../OBNG/knowledge/blood-on-hits.md`): at the cut the actor's blood
  spray (`Effects\BloodSpray.nif` for every NPC, the creature's own or none)
  as a BSTempEffectParticle (0x005713F0) under the cell's effect node, handed
  to the temp-effect manager (0x00B3BD00, 0x00678D30) - three spurts 0.35 s
  apart from the body's neck outward (`BloodPulseDue`, `SprayDirection`),
  the first with one from the head the other way and the actor's decal
  (`Effects\blooddecal.dds`) laid down from the neck (cell 0x004D3B10, the
  engine's own decal gates). Every function checked by its first bytes.
  Harness dismember-corpse PASS 2026-10-31 19:39 (20261031-193950): the four
  sprays and the decal logged, the eye pictures show the spray and the stain
  at the neck.
- **The crushed head** (`TellsHeadCrushed`): a killing blow without an edge on
  the head's capsule tells Walkers 'WGHD' kind 2 - nothing cut, the walker
  ends as by a cut. Unit-tested; not run in the harness yet.
- Corrected: Walkers' 'WGHD' for the cut was built and passed end to end on
  2026-10-30 (dismember-walker, dismember-walker-real).
- **Save and load** (section 3; Nadi: "2 keine ahnung" - checked: the head
  came back): the cut actors' form ids are kept (`CutMemory`, the newest 64,
  `RememberCut`) and written to OBVR's co-save (record `OBCH`, version 1,
  beside the quick-key pages and the holster places; read back through
  xOBSE's ResolveRefID, `CutsOfRecord`). When such an actor's 3D is there and
  not cut, it is cut again once the 3D has stood a second (`RedoDue`, the
  ragdoll built), with no push, no blood and no message. A cut whose skins
  the engine rebuilt is noticed by one bone pointer OBVR wrote no longer
  holding its value (a load may reuse the root's address) and forgotten
  first. Harness dismember-save-load PASS 2026-10-31 19:51
  (20261031-195120): cut, saved, `coc AnvilBenirusManor`, the save loaded -
  "3D changed - its cut forgotten", "1 head(s) cut read from the co-save",
  "cut again after its 3D came back - off again"; the picture after shows the
  body and the head apart. A load in the same cell kept the 3D and the cut
  as they were (19:47).
- **A real blade through a corpse** (item 6): harness dismember-blade-corpse
  PASS 2026-10-31 20:00 - a low sideways sweep at 3.4 m/s through a lying
  beggar's neck cut it, with no TestCut.
- **Wound caps** (item 4, `[Hands] DismemberCaps=1`): vanilla's zombie neck
  plug (`meshes\creatures\zombie\neckplug.nif`, flesh and bone, skinned to one
  bone "Bip01 Head") loaded by the engine's model loader (0x00439EB0),
  cloned (0x00700900), its bone pointed at a node of OBVR's - on the stump a
  node at the cut with the head's turn in the neck's frame (`capSide`), under
  the head the head itself - hung under the actor's root (vt+0x84) and
  updated (0x00707610, 0x00707370); `../OBNG/knowledge/skinned-part-attach.md`.
  A cut forgotten while its root still stands has its caps culled. Harness
  dismember-corpse PASS 2026-10-31 20:16 (20261031-201627): "the stump capped
  (2 shape(s)), the head capped (2 shape(s))"; the eye picture shows the
  stump closed with flesh and bone, the blood under it.
- **The crushed head in the harness** (dismember-head-crushed.txt, an iron
  mace thrust at the head): not passed yet - the beggar's health ran down
  over five or six strikes, the killing one met the torso at the first height
  (20:06), and at the head's height the strikes counted stopped short of the
  kill (20:09-20:13). Unit-tested only.
- **Hands** (item 7, "wenn rest steht dann go"): `CutPartOff(actor, CutPart)`
  cuts a hand at its wrist the way the head is cut - the hand's joint to the
  forearm released, the body's shapes give up the hand and its fingers
  (collapsed at the wrist), the hand's shape keeps its own and the cut arm's
  forearm goes to the hand. NPCs' hands are one shape holding both hands'
  bones (`handRM`, 16 of 36 the cut hand's), so on a hand cut only the bones
  that hold the cut hand and not the other go to the part
  (`SwapBoneOnPartSide`; the first run collapsed the right hand too, 20:21).
  The rule is the head's (`DecideCut`: dead, an edge, fast enough) on the
  wrist - the forearm's last 30 % to a tenth past the hand
  (`WristSegment`, radius 4 units). Blood as at the neck; no cap (no vanilla
  wrist plug), no message to Walkers. Kept in the co-save with its part
  (record `OBCH` version 2; version 1 read as heads). Harness dismember-hand
  PASS 2026-10-31 20:24 (20261031-202452, [Debug] TestCut at "cut_hand"):
  "left hand cut - 1 wrist joint(s) released ... Foot:upperMusc.002(body,
  1/14), Foot:handRM(head, 16/36)", the hand 7 units off the wrist, blood;
  dismember-corpse and dismember-save-load PASS again after (20:26, 20:27).
- **Wrist caps** (Nadi, 2026-10-31: "go 5"): no vanilla wrist plug, so the
  zombie's neck plug serves, scaled to a wrist (`kWristCapScale` 0.45,
  `CapScaleFor`) on nodes of OBVR's: the stump's at the hand's place and turn
  in the forearm's frame, the hand's following the hand (`partCapSide`) - the
  biped's bones all point along their own x, so it sits on the wrist as on
  the neck (derived). Harness dismember-hand PASS 2026-10-31 20:51
  (20261031-205124): "the stump capped (2 shape(s)), the left hand capped (2
  shape(s))"; the eye picture, the body 90 units ahead, does not show the
  caps clearly enough to say they sit right - for the headset.
  A blade through a corpse's wrist: dismember-blade-corpse 2026-10-31 20:35
  (20261031-203510) cut the left hand too - on the scripted hand's jump back
  between two sweeps (100 m/s), not on a sweep of its own.

## Built 2026-11-05: the limbs (Nadi: "go 4")

- **The parts** (`game/DismemberLogic.h`, dismember_test `TestLimbs`): the
  forearm at the elbow, the whole arm at the shoulder, the lower leg at the
  knee, each side (`CutPart` 3 to 8; the head and hands keep 0 to 2). One
  table, `BonesOfCut`, gives each cut its bones: the part's own, the
  ragdoll body its joint holds it to, the stump's bone and the other side's
  match. The joints were read from the vanilla skeleton.nif's constraints
  (pyffi): UpperArm-Spine2, Forearm-UpperArm, Hand-Forearm, Calf-Thigh,
  Head-Spine2 - so the arm's joint is released against Spine2 and its stump
  collapses on the collarbone. `CutPartOff` takes the table instead of the
  head/hand branches.
- **One cut per limb** (`CutPartsClash`): a hand, its forearm and its arm
  are one chain; once one is off, the others are refused ("a part of that
  limb is cut already").
- **The blade** (`camera/hands/Dismember.cpp`): the hands' rule on each
  joint's stretch (`JointSegment`: the elbow from 15 % up the upper arm to
  20 % down the forearm, ending before the wrist's stretch; the shoulder the
  upper arm's top 30 %; the knee around the calf's top), within
  `LimbRadiusUnits` (elbow 4.5, shoulder 6, knee 5.5 - estimated, not
  measured). A killing strike on an arm's or a leg's capsule cuts the limb
  whose joint the blade's sweep on passes through as it stood the frame
  before (the neck's rule). The living keep their limbs, as their heads
  (`DecideCut`). The cut trains the hand's bonus (`DismemberBonus::HandOff`).
- **Caps**: the zombie's neck plug at `kElbowCapScale` 0.55,
  `kShoulderCapScale` 0.8, `kKneeCapScale` 0.7 (estimated from the body's
  girth; for the pictures and the headset).
- **Co-save**: 'OBCH' version 3, the layout of version 2 with the limbs'
  part values. Version 2 and 1 records are read as before; a part a version
  does not know is skipped, never read as a head. A DLL of version 2
  refuses a version 3 record whole.
- **Harness** `dismember-limbs.txt` PASS 2026-11-05 14:54
  (20261105-145432), [Debug] TestCut at "cut_forearm", "cut_arm", "cut_leg"
  on one dead bandit: "left forearm cut - 1 elbow joint(s) released ...
  Cuirass(body, 3/18)", "right arm cut - 1 shoulder joint(s) released ...
  Cuirass(body, 5/18)", "left lower leg cut - 1 knee joint(s) released ...
  Foot:upperMusc.003 1(head, 2/6), Pants.001(body, 1/8), Foot(head, 3/8)",
  the stumps and parts capped, blood; the left hand after was refused. At
  the end each part lay 8 to 11 units from its cut. The eye pictures do not
  show the limbs clearly: a light wedge covers most of the left eye at that
  pitch - not the player's hair (the head cull was on in the run, and the
  wedge is the same with it off, the OBVR session's runs); derived by that
  session, not verified: the holstered Elven Longsword's blade at the left
  hip seen from above. Cheapest check: the same run with Holsters=0. The blade's sweep through a
  corpse's elbow, shoulder or knee and the killing strike's limb are not run
  in the harness (unit-tested geometry only).
- **Open**: the headset (do the parts and caps look right), a limb cut on
  a creature (human skeletons only), the thigh at the hip (not asked for).
- **Explained, not a bug**: "3D changed - its cut forgotten" after the
  script ended was the actor's 3D gone at the game's shutdown (root ->
  00000000); the line now names the part and why.

### 2026-11-05 evening: the hand stretched to the horizon, harder cuts, the cut's sound

- **Bug (Nadi, Windows headset run 19:07): a hand cut off stretched to the horizon.** Derived from
  the code and the log, then measured: the hand's mesh weighs the wrist on `Bip01 L ForearmTwist`
  as well, a child of the forearm beside the hand. The part's side moved only the bones that hold
  the hand (`SwapBoneOnPartSide`), so the twist bone stayed on the body and drew the wrist after
  the flying hand; the forearm cut had the same hole at `Bip01 L UpperArmTwist`. Fixed: the bones
  under the stump's own bone that are not the part's go to the part too. The cut's log line now
  names them ("to the part: Bip01 L Forearm, Bip01 L ForearmTwist, Bip01 L UpperArm").
  Harness dismember-hand-far (the right arm first, then the left hand thrown 375 units) PASS;
  the right eye shows no strip from the wrist. Open: the headset on Windows (another mod list).
- **The light wedge in the Linux eye pictures is not dismemberment's.** Measured 2026-11-05: it is
  the harness player's own upper body, stretched to infinity from the first mark on - with the
  single pass off, with OBNG's route off (shadow-run, obng-measure-original.ini), with beta 8's
  OBVR.dll, with a shirt (MiddleShirt07) or an iron cuirass on (both stretch with it). Read: the
  Linux `femaleupperbody.nif` (2014, a BBB body) is weighted to `Bip01 R Op1` / `Bip01 L Op1`,
  which the Linux `skeleton.nif` (Ragdolls, 2026-10-23) does not have; OBVR's soft push logs "0 of
  2 BBB breast bones on this skeleton". Derived, not verified: vertices on a bone the skeleton
  lacks are this stretch. The run of 2026-11-03 without it was on Windows. A fix is the mod
  setup's (a BBB ragdoll skeleton, or a body without the Op1 bones), Nadi's call.
- **Harder cuts (Nadi: "cuts dürfen nicht so einfach sein").** The neck 5 m/s
  (`DismemberMinSpeed`, was 2.5), a hand, an arm or a leg 6 m/s (`DismemberLimbMinSpeed`), a
  two-handed blade 0.8 of either (`DismemberTwoHandedShare`); `CutSpeedNeeded`, every flow in
  dismember_test. Felt only in the headset.
- **The cut's sound.** WPNHitBladeFlesh (0x0018B9F1) and PHYDamageBone (0x00025417), read from
  Oblivion.esm's SOUN records, at the part on the actor; none for a cut done again after a load.
  `[Hands] DismemberSound`. Harness: "the cut's sound ... 2 of 2 played".
- **Open bug: dismember-blade-corpse fails since the limbs (993c3e6).** Its low sweep meets the
  lying beggar's knee before the neck: the lower leg comes off, the neck is never touched. The
  scenario is pinned at its own speed (2.5 m/s); its sweep wants to aim at the neck again.

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

- **Open bug (Nadi, 2026-11-03, Windows headset):** the head comes off well
  ("decaps: auch gut"), but the wound caps do not sit quite right - "die
  kappen sitzen nicht ganz richtig aber das fixen wir später". Which cap
  (the stump's, the head's or both) and in which way (offset, turned, too
  big or small) was not said and is not measured. Since a51eaf9 the head's
  cap is turned to face out of the head; its place is still the neck
  plug's own. First step when picked up: eye dumps of a cut corpse from
  the side (dismember-corpse.txt) and Nadi's word on which cap is off.
- **Open bug (harness 2026-11-05 09:55, Linux, DLL f8d79e51):** a light
  killing slash on the beggar crashed the game in the skins' swap -
  "access violation reading 3F80001C at OBVR.dll+103899", `SwapSkins` +0x229
  (ecx 3F800000, a float 1.0 read as a pointer), right after "Dismember: the
  engine's joint and list functions as read"
  (artifacts/hand-script/stagger-sides/20261105-095542, the first
  stagger-sides.txt run; the beggar unarmed, 74 health, the fourth slash
  killed). Not reproduced, cause not looked at; the run had [Hands]
  StaggerSides' mirrors on the beggar's animation list, which touch no skin.
  First step: rerun strike-at-touch.txt to the kill.
- **Open bug (Nadi, 2026-11-07 23:18, Windows, DLL bc71568): a load after
  a beheading crashes the game.**
  - *What happened (read, OBVR.log and CrashLogger.log):*
    - Nadi beheaded a walker, `490B6108` ("Dismember: 490B6108's head
      cut", caps, blood).
    - Then they loaded a save from the Message menu ("Refs: a load runs ...
      (world 131)").
    - In the load's first frames OBVR crashed: "access violation reading
      490B6144 at OBVR.dll+F92EA". The map resolves that to `StepCuts`
      +0x7A, called from `StepDismember` <- `UpdateHandMode` <-
      `OnPresent`.
  - *The instruction (read, dumpbin):* `mov edi,[ecx+3Ch]` with ecx =
    `c.actor` = 490B6108. That is the cut's own reference read at
    `kRefNiNodeOffset` (0x3C) for its 3D root. `LooksLikeObject` let it
    through: it checks only the range and the alignment, not whether the
    memory is still there.
  - *Cause (derived):*
    - `g_cuts` keeps raw actor pointers across a load. The load freed
      the walker, a reference created at runtime, and its page was no
      longer readable.
    - `StepCuts` is meant to notice a changed 3D (root, skins) and forget
      the cut, but it has to read the dead actor to find that out.
    - `CutStands` (from `RedoCuts`) and `LogCuts` read `c.actor` the same
      way.
    - Earlier loads with cuts did not crash, probably because the old
      actor's memory was still mapped. Guessed, not measured.
  - *Fix (proposed, small):* the cut stores `CurrentWorld()` (NearbyItems,
    the world count each load advances) when it is made. `StepCuts` drops
    every cut from an older world without reading it.
    - No `CullCaps` either: the caps hung in the old 3D, which the load
      took with it.
    - The co-save memory (`g_memory`, by form id) and `RedoCuts` redo the
      cut once the actor's 3D is back, as before.
    - A readability check (`RefBytesReadable` on `c.actor`) would only be
      a workaround. A freed page that is used again reads as some other
      object: in this crash eax already held a Better Cities creature 64
      KB below.
  - *Tests:* a unit flow (cut from world N, world N+1: dropped, never
    read) and a harness scenario that cuts, saves, loads and cuts again
    (dismember-corpse.txt plus a load step). Not built yet.
  - *Files:* artifacts/crash/20261107-231839 (OBVR.log, CrashLogger.log;
    not in Git).
  - *Built (2026-11-08), deviating from the proposal:* dropping every cut
    of an older world would have broken the case this spec measured on
    2026-10-31 19:47: a load in the same cell kept the actor, its 3D and
    the cut, and the skins still point at the cut's own nodes. Zeroing that
    slot would leave them pointing at emptied nodes. So after a load each
    cut asks the engine's form table for its form id (`CutWorldFor`,
    `RecheckCutsAfterLoad`):
    - the same pointer comes back: the cut is taken into the new world and
      read from the next frame, where the root and skin checks decide as
      before;
    - another pointer or none: dropped unread, the waits reset, and the
      co-save memory cuts the part again.
    `Find`, `CutStands` and `LogCuts` read only cuts of the current world.
    - Unit flows in dismember_test (all five branches). 225/225 tests pass.
    - Harness 2026-11-08, both PASS on Linux:
      - `dismember-load-same-cell.txt` (run 20261108-095742): load in the
        same cell. The form table answered with the same bandit, so the cut
        stayed on it, still 28.5 units apart at the marks after the load.
        That is the Recheck branch.
      - `dismember-save-load.txt` (run 20261108-095937): load from another
        cell. The bandit came back at a new address (2278CE7C, then
        225AA25C), the old cut was dropped unread, and the co-save memory
        cut the new one again ("off again"). That is the Drop branch, the
        crash's own case.
    - After the runs, the log line was cut to once per load for kept cuts
      plus one per drop, with its own budget: the first version said it on
      every loading frame and used up the redo lines' budget. Each scenario
      now expects its branch's line. Both reran PASS with it (runs
      20261108-101457 and 20261108-101638): one "still on their actor" line
      per load and one "dropped unread" for the load from another cell.
    - The crash itself needs a freed page, so a harness run can prove the
      path and that no crash follows, not the original crash.
- **Hands** (rule 2's arm contact at the wrist): the same swap at `Bip01 L/R
  Hand` in the Arms, the hand and the gauntlet shapes. Small-medium once the
  head works.
- **Limbs at a joint** (forearm, upper arm, lower leg): built 2026-11-05,
  see "Built 2026-11-05: the limbs" above.
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

**Fixed 2026-11-03: the head's underside seen through.** In the headset the
cut head's underside stayed see-through. Measured [M] (dismember-corpse,
the new log line from the caps' vertex normals, NiGeometryData +0x20, under
their skin-to-bone): the zombie neck plug faces up the neck (the stump's cap
0.82 to 0.92 along the head bone's way up the neck - right for the stump),
so the same plug on the head faced into the head (-0.82 to -0.92 down the
neck) and showed its back from below. The head's cap now follows a node of
its own (`partCapSide`, as the hand's) and, when read facing into the head,
is turned half round about its centre (`FlippedCap`, DismemberLogic.h;
`CapFacing`, `CapFacingAlong`): placed, it reads 0.90 to 0.92 down the neck.
Log: "the caps' facing read - the stump's X up the neck ..., the head's Y down
it ... - turned over; placed, the head's Z down the neck". dismember_test
TestCapFacing, TestCapFlip; harness dismember-corpse, dismember-killing-blow,
dismember-hand PASS. Open: the hand's own cap is not read for its facing
(same plug, scaled); the headset.
