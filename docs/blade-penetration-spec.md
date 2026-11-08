# Blades that sink in, and cut marks (analysis 2026-11-08; steps 1-4 built, section 8)

Nadi, 2026-11-08: blades are to sink into NPCs as in Blade & Sorcery, but
only through bare skin or light clothing. Leather and the like should let
them in only under a lot of pressure. Cut marks follow the same rules.
This picks up physical-combat-spec section 9 ("nur wenn ohne Rüstung",
2026-10-27) and weapon-collision-spec 17.1 and 18 (penetration: open).

Marks used here: **Read** (seen in a source or the code), **Derived**,
**Guessed**.

## 1. How Blade & Sorcery does it (Read)

From the B&S SDK wiki, Damager JSON
(https://kospy.github.io/BasSDK/Components/ThunderRoad/JSON/Damager.html):

- **Material decides.** Each damager has a `damageModifierID`, and the
  DamageModifier lists per material "how Damagers react to different
  Material. For instance, one damager could penetrate wood, another would
  only be able to penetrate stone"
  (https://kospy.github.io/BasSDK/Components/ThunderRoad/JSON/DamageModifier.html).
  The ragdoll materials are flesh, leather, chainmail and plate (B&S 8.4
  Beta 8 notes on Steam: "Ragdoll materials (flesh, leather, chainmail,
  plate) are now correctly detected").
- **Two ways in:**
  - by speed: `intensityMinVelocity` and `minSelfVelocity`;
  - by pressure: `penetrationPressureAllowed` ("penetration can occur by
    resting this object against a surface and applying force"), sampled
    from `penetrationPressureForceCurve`. `penetrationPressureMaxDot` sets
    how square to the surface the blade must be (1 = perpendicular).
- **Inside:**
  - the blade is held by dampers: `penetrationHeldDamperIn` and `Out`
    while held, `penetrationDamper` once let go;
  - depth is limited by `penetrationDepth` (a dagger to its handle);
  - past `penetrationShortDepth` it can no longer be moved around; below
    it, it may pivot by `penetrationShortDepthAngle`;
  - slashes may slide out: `penetrationAllowSlide`, `penetrationSlideDamper`;
  - `penetrationSkewerDetection` notices a blade that came out the other
    side.
- **Slash or stab:** the damager's `direction` (Forward for a pierce,
  ForwardAndBackward for a slash), and `penetrationLength` (the edge's
  length) and `penetrationDepth` (for a slash: from the blade's middle to
  its edge) (https://kospy.github.io/BasSDK/Components/ThunderRoad/Items/Damager.html).
- **What players feel (Read, Reddit):**
  - "you can stab through leather armour, but you gotta put A LOT of force
    behind it" (r/BladeAndSorcery, 2022-01-27, sds1u4);
  - heavy weapons stab armour worse than light ones, because they lose
    momentum (gz4evk);
  - armour turns slashes into blunt damage up to its level (kq4brd).
  That matches Nadi's rule.
- **Wound marks:**
  - B&S paints wounds into per-character mask textures (the "Reveal"
    system, mask resolution per item, body and outfit layers apart:
    https://kospy.github.io/BasSDK/Components/ThunderRoad/Items/RevealDecal.html);
  - 8.4 added pierce, blunt, slash and burn decals on characters, and
    decals on both sides of a skewer (Steam notes, 2021-02-08);
  - a shallow cut has a different decal from a deep one, and stabs leave
    a slit in clothing and leather (r/BladeAndSorcery lhl4xr).
- **Not read:** B&S's default numbers (the velocities and pressures per
  material). I could not verify them.

## 2. What OBVR has (Read, from the code)

- **The blade meets bodies:** fitted capsules per bone with a radial field
  from the mesh (game/RadialField.h, mesh-fitted-capsules-spec 13). It
  answers only the radius of the outermost layer, not whose layer it is.
- **The shapes per worn item:** body, head, armour and clothing apart
  (game/BodyView.h, game/FittedCapsules.cpp). Whether an armour is heavy:
  `TESBipedModelForm` flags bit 7 (`kFlags_HeavyArmor`), and the armour
  rating is a UInt16 at `TESObjectARMO+0xE4` (xOBSE, via DeepWiki:
  llde/xOBSE `IsHeavyArmor`, `armorRating`). Not read in our EXE yet.
- **What the blade does on contact today:**
  - it stops at the surface, with a shallow band of 3 units
    (BladeContactLogic.h);
  - hit-stop, glancing off (`BladeSlidesOffPeople`), a hit on touch;
  - the blade's speed and direction at the contact are known.
- **Dragging a body:** the ragdoll hold (`GrabRagdoll`) already drives a
  dead body's part, so a stuck blade could pull a corpse by its part.
- **Engine decals on bodies (Read, OBNG knowledge/blood-on-hits.md):**
  - `0x004CF1A0` places a decal on one NiAVObject; on skinned geometry it
    makes a `BSTempEffectGeometryDecal` (only with
    `bDecalsOnSkinnedGeometry=1`);
  - the engine's own melee blood decal on the victim goes this way;
  - limits: `fDecalLifetime` (10 s by default, for all decals) and
    `iMaxDecalsPerFrame`;
  - its 12 arguments are partly derived. Whether the decal can be turned
    about its projection axis, or stretched, is **not read**.
- **Own meshes on bones:** the wound caps load a NIF, clone it and hang it
  under a node that follows a bone (game/Dismember.cpp `AttachCap`). The
  same works for a short cut-mark strip, with no engine decal limits.

## 3. The rules (Proposed)

**What is hit.** The material at the contact point is the outermost layer
there. The field's build already knows which shape gave each bin's radius
("Field bins take the outermost layer"), so each bin also stores its
shape's class:

| Class | What | Way in |
|---|---|---|
| Skin | the body or head shape, nothing over it | a normal swing or stab |
| Cloth | a clothing item | like skin, a little more |
| Light | light armour (leather, fur, chain, elven, glass ...) | only with a lot of pressure, harder with a higher armour rating |
| Heavy | heavy armour (iron, steel, dwarven, ebony, daedric ...) | never: the blade glances off as today |

Creatures without these shapes get one class per creature type (guessed:
skeletons and ghosts never, animals as skin).

**When it goes in** (pure, BladeContactLogic style). Speed and pressure as
in B&S:

- **A stab:** the tip leads, and the blade is no more than about 30° off
  the surface's normal (B&S `penetrationPressureMaxDot`). It goes in
  when:
  - the tip's speed along the blade passes the class's entry speed
    (Skin about 1.5 m/s, Cloth about 2, Light from about 4 up to 6 with
    its rating, Heavy never); or
  - by pressure: the hand has been held past the surface for N frames,
    for Light a long, deliberate push. Here "pressure" is measured as how
    far the controller has gone through the surface the blade is held at,
    over time, because OBVR has no force.
- **A slash:** the edge leads (the swing's direction across the edge
  within about 45°). It goes in when the edge's speed passes the class's
  speed (Skin about 2.5 m/s, Cloth 3, Light 5 to 7, Heavy never). The
  depth is limited to the blade's width (B&S: the middle to the edge),
  and the blade may slide along the cut and out.
- All numbers are **guessed** starting points, to tune in the headset and
  in a settings table.

**Inside** (the visual blade; the hand stays the controller):

- **Stab:** the blade is pinned at the entry point on the struck bone.
  - It can go deeper or come out only along its own axis, with a damper
    in and out (B&S `penetrationHeldDamperIn`/`Out`), down to the blade's
    length.
  - Moving the hand sideways holds the blade (as the hit-stop does). Past
    a limit:
    - a corpse is dragged by that part (the ragdoll hold's drive);
    - a living fighter staggers, and the blade tears out with a second
      hit. Guessed; what the engine's AI does with a held target is
      unknown (physical-combat-spec 9).
- **Slash:** the edge sits up to its width deep and slides along the cut
  as the hand moves on. It leaves when the cut ends, the hand pulls it
  back out, or it passes the far side.

**Heavy armour, and Light without enough pressure:** as today. The blade
glances off or is stopped, there is no mark (or at most a scratch, see 4),
and the hit counts as before.

## 4. Cut marks, by the same rules (Proposed)

A mark is made only where the blade went in by section 3: Skin and Cloth
always, Light only after a pressure entry, Heavy never. Optional, behind
a setting: a light scratch on Light and Heavy without entry, the dent
B&S draws.

There are two ways:

- **(a) The engine's skinned decal** (`0x004CF1A0`, BSTempEffectGeometryDecal),
  one decal per few units along the cut, with a cut texture per class
  (skin wound, cloth slit, leather gash).
  - For: it lies on the triangles and follows the skin.
  - Against: lifetime and limits are global (`fDecalLifetime` 10 s, so
    marks fade); whether it can be turned along the cut is not read; it
    needs `bDecalsOnSkinnedGeometry=1`.
  - Cost **medium**: read the 12 arguments and the rotation, make the DDS
    textures, a harness run that counts the decals, and the headset.
- **(b) Own strips on the bone,** the way the wound caps work: a thin
  textured strip NIF placed along the cut, just above the surface (the
  field's radius at that point), hung under a node that follows the
  struck bone.
  - For: OBVR decides lifetime and number, the mark can be stored in the
    co-save like the cuts, and it can be turned and stretched freely.
  - Against: it does not bend with the skin (a long cut over a joint, a
    breathing chest), and it can flicker against the skin; short
    segments and a small lift help.
  - Cost **medium to large**: the strip mesh and textures, the depth
    against the surface, culling with the body, the co-save.

Recommendation: **(a) first.** It is the cheaper test of whether marks on
bodies look right at all, and its unknown (the rotation) is one read of
the EXE. If it cannot be turned, or the 10 seconds cannot be held per
mark, then (b).

## 5. Steps and cost

| Step | What | Cost | Why |
|---|---|---|---|
| 1 | The class per contact: store it per field bin, read heavy/light and the rating, log it with each contact | small to medium | The field build and the per-item shapes exist; new are the class per bin and the armour flag (offset to verify in our EXE). Unit flows plus a harness run against the armour outfits (armour-touch-*.txt) |
| 2 | The gate: entry by speed or pressure per class, as a pure decision, logged only (no behaviour yet) | small | Pure logic over values the contact has. Unit flows; a harness run with stabs and swings at a bare and a leather-clad NPC |
| 3 | Cut marks (a) on the gate's yes | medium | The engine's decal call to read (args, rotation), textures; harness counts the marks, headset judges them |
| 4 | The slash sinks and slides | medium | The visual blade held at depth along the cut. Feel is the risk; headset |
| 5 | The stab that sticks: pinned, dampers, pull-out, drag a corpse, stagger the living | large | Engine unknowns (a held living target, its AI); the ragdoll hold is the nearest reading. Harness for the corpse, the living case only by measuring |

Recommended order: 1 and 2 first (cheap, measurable, and every later step
needs them), then 3, which is the most visible result for its cost.
4 and 5 after the headset has judged 3.

## 6. Open questions

- B&S's own numbers per material: not read.
- What `0x004CF1A0` takes (rotation, size, lifetime per decal): to read.
- Whether OBVR's EXE has `kFlags_HeavyArmor` and `armorRating` at
  xOBSE's offsets: to verify.
- A living target that holds a stuck blade: the AI's reaction is unknown.

## 7. Step 5: the fighter holds still while the blade is in (research, 2026-11-08)

Nadi, 2026-11-08: for step 5 the AI must not move while a blade sticks in
it, as in Blade & Sorcery.

**What B&S does (Read, as far as it could be read):**
- No source describes a separate "freeze while penetrated" switch. A hit's
  effect on a creature is its brain's reaction to a push: a push level
  0 to 3 and a type (Magic, Grab, Hit, Parry). Each brain chooses a
  reaction per level, from a stagger and a full stagger
  (`StaggerFull`) to collapsing into a ragdoll (`Destabilize`)
  (https://bladeandsorcery.wiki.gg/wiki/Push_Level; nexusmods
  bladeandsorcery mod 13348 swaps "Destabilize" for "StaggerFull" in the
  Brain files).
- The blade holds the struck part physically. Its joint and dampers
  (section 1) keep the part on the blade, so a fighter staggered or
  collapsed stays on it.
- A deep stab to the chest kills, and the death animation plays
  (r/BladeAndSorcery rrt1lj; 8.4 Beta 8 notes).
- Players believe a fighter with a blade left in stays ragdolled, but
  only as a guess (r/BladeAndSorcery 9v1h9w).
- **Derived:** in B&S "does not move" is the stagger or the ragdoll plus
  the blade's hold, not a frozen AI. I could not verify a dedicated
  freeze.

**What Oblivion offers (Read, the CS wiki):**
- `SetRestrained 1`: "Restrained actors will not move from their current
  position, but will continue to 'think' (pick packages, run detection so
  they could yell alarms) and go into dialogue"
  (https://cs.uesp.net/wiki/SetRestrained). On a dead actor it
  resurrects it, so it is for the living only. Fallout 3's GECK page says
  the same flag also keeps the actor out of combat and from using its
  weapon (https://geckwiki.com/index.php?title=SetRestrained). Whether
  Oblivion's does the same is **not verified**.
- `SetUnconscious 1`: the actor does not think at all. But "When called
  during combat, SetUnconscious has no effect. Consider using
  SetRestrained instead" (https://cs.uesp.net/wiki/SetUnconscious), so
  it is no use in a fight.
- OBVR has its own knockdown into the ragdoll (`KnockActorDown`, engine
  0x00654420) and the stagger (`StaggerSides`), which correspond to B&S
  `Destabilize` and `StaggerFull`.

**Proposal for step 5 (B&S's way, with Oblivion's tools):**
- **A stab that sticks in a living fighter:** a stagger on entry, then the
  restrained flag for as long as the blade is in. The fighter stays on the
  spot and still plays its animation; the flag is cleared on pull-out,
  death or a load.
- **A deep stab to the torso, or one past a force:** into the ragdoll
  (`Destabilize`), and the blade then holds the part like the ragdoll
  hold, so the body hangs on the blade and can be dragged.
- **To measure first (harness):**
  - does a restrained Oblivion NPC still attack;
  - is its flag cleared safely in every way out (death, load, a cell
    change);
  - what the engine writes for it, so OBVR sets it directly and not
    through the console.
  If it still attacks, the attack gate OBVR already has (0x00612BD0,
  combat ring) holds it back while the blade is in.
- **Cost: large,** as in section 5, with the restrained flag adding a
  small read of the engine (where the flag lives, which function sets it)
  before anything is built.

## 8. Steps 1-4 built (2026-11-08)

Settings: `[Hands] BladeEntry*` and `BladeEntryCutMarks` (OBVR.ini). Code:
game/BladeMaterial.h, BladeEntryLogic.h, BladeEntry.cpp, CutMarkLogic.h,
CutMarks.cpp. Unit tests: blade_material_test, blade_entry_test,
cut_mark_test (every branch). Harness: tools/hand-scripts/blade-entry.txt,
PASS on run 20261108-110657 (Linux, headless).

**Step 1, the material per contact.**
- Each worn shape is classed by the nearest node named
  "<slot> (<form id>)" above it:
  - no such node: skin;
  - form type 0x14: armour, heavy when the biped model's flags have bit 7,
    light otherwise, with the armour's points (rating / 100);
  - 0x16: cloth;
  - any other form: skin. The race's own body parts once the clothes are
    off read as form type 09.
- The class goes per vertex into the fit and per bin into the radial
  field, from the vertex that is the surface. Each contact reads it where
  it touches.
- **Measured:**
  - the beggar's clothes are type 16, flags 00, so cloth;
  - the Legion soldier's four pieces are type 14, flags 80 to 82, raw
    ratings 1125, 675, 450 and 450, so heavy armour with 11, 6, 4 and 4
    points.
  - That confirms xOBSE's offsets in our EXE: flags at biped+0x06 with
    bit 7 for heavy, and the rating at ARMO+0xE4, stored a hundred times
    over.

**Step 2, the gate.**
- The numbers are as in section 3. The harness saw:
  - stabs in at 2.9 m/s (cloth, leg) and 10.9 m/s (skin, torso);
  - slashes in from 4.5 m/s;
  - "too slow" at 0.2 m/s;
  - "the flat first" with the blade unrolled: the longsword's edge stands
    vertical when held at rot 0;
  - heavy armour refused on every contact (five).
- **Not seen in the harness: an entry by pressure.** The scripted push
  slid off ("moving off the surface"). It is unit-tested only.

**Step 3, cut marks by the engine's decal.**
- Read from the EXE:
  - 0x004CF1A0 is thiscall on the reference's cell (REFR+0x40) with 12
    arguments: point, direction, texture under Textures\, node, a count of
    geometries to skip, a second node, the turn about the direction (in
    radians; 0x0070FDD0 builds it), and one more;
  - the size is a fixed 15 units;
  - the frame comes from the direction (0x006F9290, replicated as
    `EngineDecalFrame`);
  - the call's answer is not whether it laid one: its last path returns
    whatever a release left in al.
- **Measured:** with MotionHits=0, so the game laid no blood of its own,
  the decal shapes under the beggar rose 0, 1, 2 across the cut-mark calls
  of the skin round. The EyeDump picture at "skin_slash" shows two red
  wound marks: a vertical one on the upper back from the stab, along the
  vertical edge, and one on the left arm.
- **Open:**
  - **Lifetime:** the marks last the game's fDecalLifetime (10 s here),
    the same for every decal. A longer life is that game setting, not one
    per mark.
  - **The slash mark's direction is unverified.** The picture's arm mark
    stands along the arm. The u row (the frame's first row) and the space
    the turn is computed in (the actor root's frame) are derived, not
    checked.
  - **Not every call laid one at once:** the first two cloth marks left
    the count at 0.
  - The decal is 15 units across, fixed by the EXE.

**Step 4, the slash sinks.**
- A slash that went in and is held at the surface is drawn deeper toward
  the hand, at most the blade's width (twice the read half-width; 3.2
  units on the longsword, a guess to tune).
- It comes out when the hand is drawn back. The slide along the cut is
  BladeSlidesOffPeople's.
- **Measured:** "the slash sinks into ...'s right leg 3.2 units (at most
  3.2)". How it feels is for the headset.

## 9. Blood where a blade went in (Nadi, 2026-11-08; built)

- **What:**
  - a spurt and a stain where a cut begins;
  - a stain trail along a slash, every 10 units with a drip to the
    ground every second stain, at most 10 a cut;
  - a spurt as a stab comes out.
- **Where:** only on what the gate let in: skin and cloth, light armour
  only after an entry by pressure. Never a blade glancing off armour
  (not in), never heavy armour.
- **How:** game/Blood.h, the engine's own spray (BloodSpray.nif) and stain
  (cell 0x004D3B10). An actor that does not bleed (Bleeds 0x005E1B30, or
  no spray or decal path: skeletons, ghosts) gets none; that is the
  engine's own answer, asked there.
- **Code and tests:** game/BladeBloodLogic.h (pure, blade_blood_test),
  BladeEntry.cpp `BleedCut`, `[Hands] BladeEntryBlood`.
- **Harness blade-entry, run 20261108-111234, PASS:**
  - cloth round, three cuts: 3 spurts, 5 stains, 1 drip;
  - skin round: 3 spurts, 3 stains, and "a spurt as the stab comes out"
    after the stab;
  - soldier round: six contacts refused as heavy armour, and no blood
    line at all.
  - "laid" means the engine's function answered yes. Whether each stain
    landed on the body or on the floor behind it was not counted.
- **Open:** a bloodless creature tried in the harness; how the trail
  looks in the headset.

## 10. Step 5 built: the stab that sticks (2026-11-08)

Nadi, 2026-11-08: "go schritt 5". `[Hands] BladeSticks` (default 1);
game/BladeStickLogic.h decides (pure, blade_stick_test), game/BladeStick.cpp
acts, called after the entry's step (BladeEntry.cpp `StepStuckBlade`) and
watched once a frame (CameraHook `WatchStuckBlade`).

**Deviation from section 7's proposal, and why (Read):** the fighter is held
not by the engine's restrained flag but by what the grapple's standing hold
already uses:
- `game::HoldStill`, which stands their movement flags still through the
  rerouted HighProcess setters (game/SlowApproach.cpp; their turn is kept);
- `game::SetHeldFoe`, which stops their blows at the player whole
  (game/Parry.cpp, ParryLogic.h BlowStopFor).

Both are measured since the grapple (2026-10-13, physical-combat-spec 8)
and need nothing new read from the EXE. `SetRestrained` would have needed
its flag and handler read. It also resurrects a dead actor (CS wiki), and
whether Oblivion's version keeps the actor out of combat is not verified.
The attack gate 0x00612BD0 is not needed: SetHeldFoe stops the blows. The
cost is one slot: the grapple and a stuck blade share HoldStill and
SetHeldFoe (`HeldStillActor`/`HeldFoe` added), and a stuck blade never takes
the slot from the grapple's holder.

**What it does:**
- **Start:** a stab that went in (the entry's verdict, kind Stab, its fresh
  frame) and the struck part's bone with a Havok body found
  (`FindRagdollPart`).
  - The living start standing: staggered (`StaggerActor`), legs and blows
    held.
  - A corpse starts hanging, and so does a living fighter stabbed in the
    torso at `BladeSticksRagdollSpeed` (5 m/s, guessed): they are taken
    down with no push (`KnockActorDown` force 0), and their get-up clock is
    stamped each second while they hang.
- **Standing:** the entry point is held on the bone, in the bone's frame,
  so it follows the body. The shown blade keeps the hand's turn and is put
  on the line through the entry point.
  - It moves only along that line: depth from 0 to the blade's length,
    eased toward the hand's depth at `EaseIn` 6 and `EaseOut` 12 a second
    (B&S penetrationHeldDamperIn/Out; guessed).
  - It is torn out when the hand's tip leaves the line by `TearUnits` (14),
    or the hand turns the blade `TearDegrees` (40) from where it went in.
  - The body moving under the blade does not tear it out. The first build
    pinned the blade's turn to the bone too: a fighter set on the player
    took his stance and tore it out within 0.3 s (run 20261108-123448:
    "torn out ... the blade turned to 0.76 of it").
  - Their heading is held too. HoldStill keeps the turn, so the heading the
    engine gives them each frame is put back to the one at the stab
    (REFR +0x28). The engine turned them back by at most 2.4 to 4.8 degrees
    in the runs.
- **Hanging:** the struck body is driven so the entry point stays on the
  hand's blade (`DriveRagdollPart`, the ragdoll hold's drive).
  - It follows sideways only: pushed on, the blade goes deeper; drawn
    back, it comes out. The first build drove the body along the blade
    too, so a hanging corpse could not be pulled off (run 123448: 98 units
    dragged).
  - It is torn out past `RagdollTearUnits` (30).
- **Ends:**
  - drawn back past the surface (`PullOutUnits` 1);
  - torn out;
  - its bone no longer reads;
  - a load (the world counter, `CutInWorld`'s rule, read before anything
    else);
  - switched off;
  - the blade's step not run for a frame (weapon put away).
  A death while standing lets go of the hold and turns it into hanging.
  Every start and end is said ("Blade sticks:"), each second how far a held
  fighter moved since the stab, and 3 s after an end how far the one let go
  moved.
- **Log:** the entry's line now also says how far the blade is off square
  and the blade's and the surface's directions (needed to aim the harness'
  thrusts).

**Measured (harness, Linux headless; `tools/hand-scripts/blade-sticks.txt`
on a stripped Legion soldier, every round after the first from the save
made before it):**
- **PASS 20261108-125553:**
  - **stand1:** a 2.0 m/s stab into the torso stuck standing. It was held
    3 s at 19.6 to 19.7 units deep, "moved 0.0 units since the stab" each
    second, and "pulled out" after 3.4 s.
  - **stand2:** stuck the same way. Then his AI was switched on and he was
    set on the player. He was held 4 s: moved 0.0 units, turned back from
    at most 3.2 degrees, no blow at the player in the log. Pulled out
    after 5.3 s.
  - **kill:** stuck standing, then killed with the blade in: "now hanging
    on it". The body hung on the blade (moved 68 to 88 units with it). The
    hand drew it from 23 to 4.7 units deep, and the load let it go
    ("let go - a load").
- **PASS 20261108-124633:**
  - stand1 and stand2 as above;
  - **deep:** a 15.2 m/s stab into the torso: "stuck, hanging in its
    ragdoll ... taken down", hung 4 s, then "let go - a load".
- **Not measured:**
  - **After the release:** the one let go moved 0.0 units in the 3 s after
    in every run. He stood within reach of the player, so this does not
    show that he can walk again. Not proven either way.
  - **A slow stab met from inside:** a slow stab was sometimes met first
    from inside the torso (153 to 168 degrees off square, refused as
    "moving off the surface"). That was 1 to 3 of 4 or 5 stabs a run, so
    the scenario passes or fails with it (runs 124122, 125130: FAIL on
    that alone). The contact's own matter, not this step's; open.
  - **A corpse stabbed:** not in the harness, because the lying body is
    below the thrust. Covered by unit flows only (StickStartFor: a corpse
    starts hanging).

**Open:**
- the second hit when it is torn out (none is handed to the game);
- the tear limits and the ease, in the headset;
- writing the heading straight into the reference (REFR +0x28): measured
  only as the log's numbers;
- the kill round's pull-out ended at 4.7 units deep, not out, before the
  load: the falling body may have followed the blade; open;
- a settings row (the settings list's file is at its size limit; INI
  only);
- `SetRestrained` stays unread.

## 11. Hold with the left, stab with the right (2026-11-08)

Nadi, 2026-11-08: "links halten, rechts zustechen".

**The bug (read, in the code and the harness):** the off hand's standing
grab and a stuck blade both held the fighter through the game's two slots
(`HoldStill`, `SetHeldFoe`), each written directly. Whichever let go first
cleared them for both. Run 20261108-132518, before the fix:
- the grab took him ("Grapple: the off hand holds ... legs held");
- the blade did not stick in that run (see the open bug below);
- he was let go 6.5 s later, after the left opened ("legs let go").

The ordering bug itself could not be shown in that run, because the stab
never stuck. It is read from the code: `LetGoOfHold` and `EndGrapple`
each wrote 0.

**The fix (game/HoldClaims.h, pure, hold_claims_test):**
- Each holder claims the fighter in its own right: the grab (with whether
  its part stops blows) and the blade (always stops them).
- The slots get the effect of all claims: the legs of the grab's fighter,
  or else the blade's; the blows stopped when any holder of that fighter
  stops them.
- A release leaves the other's claim standing: "Hold: ... let go by the
  blade, still held - by the grab".
- There is one slot. A grab of another fighter takes it, because the hand
  places him; a blade stuck in someone other than the one the grab holds
  is refused.
- The unit flows cover every order of letting go, a grab that does not
  stop blows, two fighters, and a claim of nobody.
- The callers: CameraHook's grapple (`ClaimHold`/`ReleaseHold`, owner
  Grab) and game/BladeStick.cpp (owner Blade).
- The diagnostics added for this: "Slow approach: ...'s legs let go", and
  "Parry: a held foe's blows are stopped / the held foe's blows land
  again".

**Harness:** `tools/hand-scripts/blade-hold-both.txt`. A stripped soldier is
placed close after the draw, set on the player and frozen. The left closes
on his chest, the right thrusts along the blade's line; it is played in
both orders of letting go.
- **After the fix: PASS 20261108-134754.**
  - **grab_first:** the grab held him ("legs held"), and a 2.3 m/s stab
    stuck standing ("a held foe's blows are stopped"). Drawn out after
    4.0 s: "let go by the blade, still held - by the grab, his blows land
    again"; his legs stayed held until the left opened 6.5 s later ("legs
    let go").
  - **blade_last:** the grab held him and the stab stuck. When the left
    opened: "let go by the grab, still held - by the blade, his blows
    stopped". He stayed standing, moved 0.1 units, for 5.4 s more, and was
    let go only when the blade was drawn out ("legs let go", "blows land
    again").
- **Before the fix (2ba24b7 with only the two diagnostic logs; runs
  132224, 132518, 140014):** the grab held him every time, but the stab
  never stuck in those runs, so the bug's order never came about in the
  harness. The bug stands as read in the code; the scenario's "still held"
  lines did not exist before the fix.

### Open bug: a slow thrust is taken up as slid off, then held from inside

Seen in every scenario with slow stabs on 2026-11-08: blade-sticks
(124122, 125130), blade-hold-both (132518), and with round capsules too
(zz-hold-round 132914, `FittedBodyCapsules=0`).

**What happens (measured, Contact traces):**
- A thrust along the blade's own line at about 2 m/s goes 2 units a frame
  toward the torso with no contact.
- The first contact frame is event 6 (`SlidOff`, "the blade none something
  - it passes through until it is free"), and its normal points away from
  the player, e.g. 0.69 0.72 0.08 or 0.54 0.84 0.05, against the blade's
  -0.16 0.97 0.17.
- `SlideOffPeople` finds the push not along the blade (across share
  > `kTurnOffLeastSideways` 0.15, because of that normal) and frees the
  blade at the hand's pose, inside the body.
- From then on the contact holds it from inside, and the entry gate reads
  "moving off the surface", 127 to 168 degrees off square.

**What is ruled out:** the radial field's normal. Round capsules show the
same reversed normal (the round normal points from the axis to the point),
so the tip was already past the capsule's axis when the first contact was
found.

**Guessed cause, not proven:** the shallow band (3 units, BladeContactLogic)
lets the tip into the capsule unregistered. The next steps then start
inside, so the tip's ray never crosses the surface, until a deeper check
finds it past the axis with the lane normal reversed.

**Measured with led speeds (2026-11-08, `blade-entry-speed-skin.txt`,
`blade-entry-speed-cloth.txt`; the hand steps v x 0.0111 m a frame):**

| Material (need) | Led | Measured at the first contact | Verdict |
|---|---|---|---|
| skin (1.5) | 1.2 | - | met from inside, 156 degrees: "moving off the surface" |
| skin (1.5) | 1.4 | - | met from inside, 168 degrees |
| skin (1.5) | 1.6 | 1.6 | stab goes in |
| skin (1.5) | 1.9 | 1.9 | stab goes in |
| cloth (2.0) | 1.7 | - | "moving off the surface" at 35 degrees, then a 16.3 m/s slash as the hold let go |
| cloth (2.0) | 1.9 | - | met from inside, 164 degrees |
| cloth (2.0) | 2.1 | 2.7 | stab goes in |
| cloth (2.0) | 2.4 | - | met from inside, 165 degrees, then a 13.5 m/s stab as the hold let go |

Runs: skin 20261108-135013 FAIL; cloth 20261108-135633 FAIL, and
135308 FAIL with the aim further left, where the beggar's chest is 50
degrees off square and every stab read "the flat first".

What this shows:
- On skin the led speed is met exactly (1.6 and 1.9).
- On cloth the one clean stab measured 2.7 for 2.1 led.
- Every led thrust at or below 1.4 m/s on skin, and three of four on
  cloth, was taken up from inside first. So the gate's "too slow" refusal
  of a square stab cannot be measured until this bug is fixed: any stab
  slow enough to be refused is met from inside.
- The slower the thrust, the surer the bug (1.1 to 1.6 units a frame
  against the shallow band of 3). That backs the guessed cause.

**To settle it:** log each frame's tip depth in the struck capsule and the
sweep's verdict for a slow thrust. Then a sweep that starts within the band
should count as entering at the surface, with a unit flow for a thrust
that steps into the band and then deeper. Not built.

## 12. Blood: seen, the neck bleeding on, deep and through (2026-11-08; built, not run)

Nadi, 2026-11-08: "go Blut".

**0. Where the stains land:** after each stain of a blade's blood
(BladeEntry.cpp), the decal shapes under the actor's 3D are counted before
and over the next 4 frames (`DecalsUnder`, game/CutMarks.h). One more and
the stain is on the body; none and it went elsewhere (the engine's decal
call ray-picks the first thing along its way, the ground or nothing).
- Each stain is said with running totals ("landed on the body / not on
  the body ... N on the body, M elsewhere so far").
- `tools/hand-scripts/blood-visible.txt` takes EyeDump and window pictures
  at the mark right after a stab's entry ("in") and right after it is drawn
  out ("out"), and a second after each.

**1. The neck bleeds on (game/NeckBleedLogic.h, pure, neck_bleed_test;
game/NeckBleed.cpp):**
- **What opens a wound:** a blade that went into the neck - skin or cloth,
  light armour only pushed through - with the head still on (the cut's own
  blood stands for that). A second blade in the same neck opens its wound
  anew.
- **How it bleeds:** with the dismember's pulse rule (`BloodPulseDue`):
  every `NeckBleedPulseSeconds` (0.8), at most `NeckBleedPulses` (25, some
  20 s) while alive. After a death the heart goes on `NeckBleedDeadSeconds`
  (6) and stops; a corpse's neck gives one spurt.
- **Where:** each spurt is the engine's spray at the wound, kept on the
  neck's bone (`FindRagdollPart`, the point and its normal in the bone's
  frame), with a stain at the first.
- **When it ends:** a load (world counter), the actor unloaded, off.
- **How many:** at most `NeckBleedMostWounds` (4); past that the oldest
  gives way.
- **When it runs:** once a frame from the camera hook, whether a blade is
  drawn or not. Said as "Neck wound: ... opened / spurt n / stops - why".

**2. Deep and through (game/BladeStickLogic.h `DeepCrossed`,
`FindSkewerExit`; game/BladeStick.cpp `BleedDeep`):**
- **Deep:** a stuck stab whose depth passes `BladeSticksDeepUnits` (15,
  guessed) gives one spurt more at the entry (B&S penetrationDeepEffect).
- **Through:** the blade's line is walked from the entry to the tip
  through the actor's capsules (their fields). Inside and then out before
  the tip means it came out the other side (B&S
  penetrationSkewerDetection). Then, once a stick, an exit wound is laid
  (the material's cut mark, projected back along the blade) and a spurt
  goes out along the blade there.

**Settings:** `[Hands] NeckBleeds`, `NeckBleedPulseSeconds`,
`NeckBleedPulses`, `NeckBleedDeadSeconds`, `NeckBleedMostWounds`,
`BladeSticksDeepUnits`, `BladeSticksSkewer` (OBVR.ini, README).

**Measured (harness, Linux headless, 2026-11-08):**
- **The soldier faces away.** `placeatme` gives him the player's heading
  (the pictures show his back), so every scenario stabs into his back.
- **Seen (blood-visible):**
  - The coordinator's run 144602: the spurt shows on OBVR-Eye-Left-in.png
    as a spray round the wound (crop-in.png); none on "out"; the stains
    went "0 on the body, 1 elsewhere".
  - **Body stains:** the cell's decal ray (`StainBlood`, 0x004D3B10) did
    not lay the stain on the body. The body stains now go through the
    decal on the actor's 3D that the cut marks use (`StainBodyBlood`); the
    drips stay on the ground.
  - Run 150003 PASS: "landed on the body (decals under it 1 -> 2)". On
    OBVR-Eye-Left-in.png the spray streaks and a red splat show on his
    back; on out.png the splat stays; the pull-out spurt itself does not
    show.
  - In the neck run 150208, all 10 stains landed on the body, 0 elsewhere.
- **The neck (neck-bleed):**
  - Run 144733: every stab hit the torso.
  - The neck's capsule is a ball 4.6 round at z -146, a few units of it
    showing between the torso's top and the head. The thrust was aimed at
    it from those numbers: the hand 0.16 m higher and 0.07 m right of the
    chest's thrust.
  - Run 150208: the first thrust met the neck and went in as a slash (40
    degrees off square). "Neck wound: ... opened by a slash (skin,
    alive)", spurts 1 to 10 while alive (one each 0.8 s). He was killed,
    spurts on to 15 dead, then "stops - the heart stopped, after 19
    spurt(s)".
  - The scenario FAILS on "- a load": after the reload the second thrust
    met the torso and the head, not the neck, so no wound was open at the
    load. The load's end of a neck wound is covered by unit flows only.
  - "No neck bone found - kept where it was": `FindRagdollPart` found no
    Havok body on the neck for him, so the wound stays where it opened in
    the world. It would not follow him if he moved.
- **Deep and through (blade-skewer):** runs 145022 and 150451 PASS: "deep
  - 15.2 units in, past 15.0: a spurt laid"; "out the other side at 249
  -1539 -170 (13.2 units in) - an exit wound asked of the engine (it
  answered 0) ... a spurt laid".
  - Turned to face the player 7 s after the exit (turned_a.png), his chest
    shows no exit wound and no blood. **The exit wound is not seen.**
  - The cause is not known. Guessed: the exit point is the capsule's
    surface, and the decal, projected from 2 units outside it, may start
    inside the mesh's skin.
  - The entry wound and the entry stain show on his back (run 145822,
    turned_a there).

**Open:**
- the exit wound;
- the neck's load end and a neck bone for some bodies;
- the pull-out spurt's visibility;
- a stab square enough at the neck to count as a stab.
