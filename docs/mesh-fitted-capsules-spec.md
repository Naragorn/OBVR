# Body capsules fitted to each actor's own mesh, as Blade & Sorcery's colliders (design 2026-10-24, nothing built)

The tester (2026-10-24): "was wäre dann der beste weg für perfekte
kollisionen. schätzungen sind shice" - and, after the options: "ok dann
verwerf die bisheriege spec und mach eine für C also so wie B&S es machen".

Read here as: the capsules the blade, the fist and the stoppers meet on the
living are no longer guessed constants. Each actor's capsules are measured
from the skinned meshes that actor is drawn with - body, head, armour and
clothing - once per mesh, and follow the bones every frame. That is what
Blade & Sorcery's creatures are: simple colliders that fit the body.

This replaces skeleton-capsules-spec.md (2026-10-24, the skeleton's Havok
capsules as the source), dropped the same day; its findings are kept in
section 3.

Markers as in the other specs: **Read** (seen in the code, a file or a
source), **Derived** (concluded from what was read), **Unverified** (still
to be shown), **Proposed** (a design).

## Verdict

**Proposed: fit one or more capsules per bone to the vertices skinned to
it, in the bone's own space, cache them per mesh, and place them by the
bone's world transform each frame.** Size: M-L. Everything that consumes
capsules today (`BladeBodyCapsules`, the hold, the hit-stop, the strike by
touch, the shallow band) stays as it is; only where the capsules come from
changes.

- Fits what is drawn, armour included: a foe in plate is bigger than one in
  a shirt.
- Almost nothing per frame: a limb is rigid in its bone's space, so the fit
  is done once per mesh; each frame is the transform OBVR does already.
- Creatures get real shapes without knowing their bone names.
- No mod needed; works with every skeleton and body mod.
- Not triangle collision (section 3.3).

## 1. How Blade & Sorcery does it (Read, weapon-collision-spec 17.1)

- A creature is a Ragdoll of Ragdoll Parts; each part "requires a
  Rigidbody and CollisionHandler on the same gameObject" with a **Collider
  Group of simple shapes** (kospy.github.io/BasSDK, Ragdoll Part, Collider
  Groups).
- Weapons too: "Even vanilla weapons in Blade and Sorcery use capsule
  colliders for all bladed weapons" (Items - Best Practices).
- The shapes are authored by hand per creature in Unity. OBVR cannot author
  per mesh (every mod's armour differs), so it measures instead. Derived:
  the measured fit is OBVR's stand-in for B&S's hand-placed colliders.
- A part can carry several colliders (a group). Here: a bone may get more
  than one capsule where one fits badly (section 4.2).

## 2. What OBVR has already (Read)

- **The consumers:** `src/game/BladeContactLogic.h` - `BladeBodyCapsules`
  (a, b, radius, actor, part), the capsule maths (`CapsuleDepthAt`, the
  segment entry, `ShallowBand`) with its tests; the bone list
  `BladeBoneName`; `game/BladeBodies.cpp` builds the capsules each frame
  from the bones and the fixed radii (`kBladeHeadRadius` 7,
  `kBladeTorsoRadius` 10, ... around line 490).
- **Reading skins at run time works:** `src/game/BodyView.cpp` reads the
  player's third-person skinned NiTriShapes as the engine holds them -
  NiGeometry +0xB4 the data, +0xB8 the skin instance; NiGeometryData +0x08
  the vertex count (UInt16), +0x1C the vertices; NiSkinInstance +0x08 the
  NiSkinData, +0x14 the bones; NiSkinData +0x40 the bone count, +0x44 the
  bones' data 0x4C each (skin-to-bone NiTransform at +0x00, the weights at
  +0x44, their count a UInt16 at +0x48, a weight {UInt16 vertex, 2 bytes,
  float}). Checked there, not trusted: every vertex's weights add to one
  (`SkinWeightMisses`, BodyViewLogic.h).
- `src/game/NearbyItems.cpp`: the geometry's model bound at +0x0C and
  `VertexInBound`, measured 2026-09-29.
- Pointer caches across loads need a fingerprint (the skin-swap lesson,
  SwapRecordHolds).
- The bones' nodes per actor are cached already for the capsules today.

## 3. Why not the other ways (Read and Derived)

### 3.1 Guessed radii (today)

weapon-collision-spec 4 C: "Not measured on the meshes". The same numbers
for every human, armour or not; creatures fall back to the bound's column.

### 3.2 The skeleton's Havok capsules

Read 2026-10-24 with `tools/nif-skeleton-shapes.py` on
`meshes/characters/_male/skeleton.nif` (vanilla out of
`Oblivion - Meshes.bsa` with `tools/bsa-extract.py`, and Ragdolls for
Oblivion 1.1). Every human body part is a `bhkCapsuleShape` in both.
Radius in game units (Havok x 7.0; Havok units are game units x 0.142877,
game/GrabPhysics.h):

| Part | Vanilla | Ragdolls for Oblivion | OBVR guess |
|---|---|---|---|
| Head | 4.2 | 6.3 | 7 |
| Spine / Spine2 | 4.6 / 5.7 | 7.0 / 8.4 | 10 |
| Upper arm | 2.9 | 3.5 | 4.5 |
| Forearm | 3.3 | 2.8 | 3.5 |
| Thigh | 4.8 | 4.9 | 6.5 |
| Calf | 4.1 | 3.5 | 4.5 |

Derived: they are physics stand-ins, made for a stable, cheap ragdoll (the
mod's author: "Replaced Convex Shape collisions with less demanding ones"),
one per skeleton for every body and armour. Vanilla's are about half as
thick as the guesses at the torso. Not used for the blade; Ragdolls for
Oblivion stays a recommendation for how bodies fall and weigh (a body
about 190 against vanilla's about 600 - a held foe about three times
lighter).

### 3.3 Triangle collision

The engine skins on the GPU; the moving vertices do not exist on the CPU.
Per frame, thousands of vertices an actor skinned by OBVR on x86-32, and
the blade swept against their triangles - against a frame budget that is
already the bottleneck. Huge, and B&S does not do it either.

## 4. The design (Proposed)

### 4.1 Which meshes

The actor's drawn skinned NiTriShapes under its third-person root (the walk
BodyView.cpp does for the player): body parts, head, hands, feet, armour,
clothing. Left out: hair and the helmet's crest (they swing and stand off
the head; by shape name or by the bone - only "Bip01 Head" with a bound far
above the skull), the weapon and shield (they have their own capsules),
shapes hidden (the hidden bit, as BodyView).

### 4.2 The fit, per bone (pure, BodyViewLogic-style)

Input: for one bone, the vertices with that bone's weight at least 0.5
(the bone that owns them), each taken into the bone's space by the
skin-to-bone transform. Output: one capsule, or two.

1. The axis: the bone's own long axis (to its child bone); for a bone with
   no child (head, hand, foot) the principal axis of its vertices (PCA, as
   the shield's disc is fitted already).
2. Ends: the vertices' extent along the axis, trimmed at both ends by the
   radius (a capsule's caps reach past its segment).
3. Radius: a high percentile of the vertices' distance to the axis (95th,
   to be tuned), not the maximum - one stray vertex does not fatten a limb.
4. Flat parts: where the vertices' cross-section is clearly flatter one way
   (the torso: wide, not deep), a single round capsule would be too deep.
   Then two parallel capsules side by side, each fitted to its half - B&S's
   collider group. The threshold from the measurement (todo 1).
5. Several meshes on one bone (body and cuirass): fitted together, the
   outermost wins (the percentile over all their vertices).

Vertices shared between bones (joints) go to the bone with the larger
weight; the capsules overlap at the joints by their caps, as today.

### 4.3 Cache and refresh

- Per mesh (the NiGeometryData pointer plus a fingerprint: vertex count,
  bound): the bone-space capsules. The same cuirass on ten bandits is
  fitted once.
- Per actor: the list of meshes it wears and their bones. Refit when the
  list changes (equip, unequip, a load) - read anew the way the bone
  pointers are, with the fingerprint.
- The fit runs off the hot path: at most N meshes a frame (budgeted, as
  NearbyItems limits its walk), the guessed capsules standing in until an
  actor is fitted.

### 4.4 Each frame

Unchanged consumers. `BladeBodies.cpp` places each fitted bone-space
capsule by the bone's world transform (and the actor's scale) instead of
building bone-to-bone segments with the constant radii.

### 4.5 Creatures

Every bone that owns vertices gets its capsule(s); no Bip01 names needed.
The parts that name a body part (head for the hit zones, hands for the
parry) use the names where they exist and leave the rest as body.

### 4.6 Settings

- `[Hands] FittedBodyCapsules=1` (on by default once it passes the
  headset; 0 = today's guessed radii).
- `Debug.CapsuleTrace` (off): one line per fitted mesh (shape, bones, radii,
  the capsules' count) and per actor (meshes, refits).

## 5. Cost: M-L

| Part | Size | Why |
|---|---|---|
| Reuse | - | the skin and vertex reading (BodyView.cpp), the weight check, the capsule maths and its tests, the bone caches, the fingerprint pattern |
| The fit | M | PCA, percentile, the flat split; pure, tested without the game |
| NPCs' meshes | M | BodyView reads the player only; other actors' third-person roots and their worn shapes to walk (the same structure, unmeasured) |
| Cache and refit | S-M | per mesh and per actor, the equip change as the trigger |
| Creatures | M | every vertex-owning bone; the part names for hit zones |
| Risk | - | NPCs' vertex data could be released after upload (the player's is there - Read; others' Unverified); the hair/helmet filter; joint vertices |
| Tests | M | pure units for every fit flow; a harness scenario (a naked NPC, a bandit in plate, a wolf); the headset |

## 6. Tests (the FrameLogic pattern)

Pure, without the game: a cylinder of points gives its radius and ends; a
stray point does not move the percentile; a flat box splits into two
capsules, a round one does not; a bone without a child takes the PCA axis;
two meshes on one bone take the outer; a joint vertex goes to the larger
weight; no vertices - no capsule, the guess stays; a bone-space capsule
through a transform and a scale lands where it should.

Harness: the capsules drawn or logged against the bound for a naked NPC, a
bandit in plate and a wolf; a slow blade stops at the cuirass, not inside
it; a swing hits the plate where the guessed capsule missed.

## 7. Open todo

1. **Offline prototype (small, no game):** the fit of 4.2 in Python with
   pyffi on vanilla `upperbody.nif`, `lowerbody.nif`, `hand.nif`,
   `foot.nif`, the head and one cuirass, against the skeleton's bones.
   Gives the measured radii next to vanilla Havok, Ragdolls for Oblivion
   and OBVR's guesses (section 3.2), the flat-split threshold, and the
   reference numbers for the C++ fit's tests.
2. Read an NPC's worn shapes at run time (BodyView's walk on another
   actor) and confirm their vertices and weights are there (the risk in 5).
3. The fit in C++ (pure, its tests), then the cache, then BladeBodies.cpp
   placing the fitted capsules behind `FittedBodyCapsules`.
4. Creatures (wolf, goblin, troll) once humans pass.
5. Harness scenario, then the headset.
6. Why the blade felt better with Ragdolls for Oblivion installed
   (2026-10-23, next-up.md): OBVR's blade does not read the skeleton - not
   explained, not measured; a harness swing with and without the mod
   (Debug.ContactTrace) would tell.
