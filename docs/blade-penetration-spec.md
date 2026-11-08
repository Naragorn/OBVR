# Blades that sink in, and cut marks (analysis, 2026-11-08; nothing built)

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
