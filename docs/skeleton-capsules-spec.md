# The blade's body capsules from the skeleton (research 2026-10-24, nothing built)

The tester (2026-10-24): "wie war das mit für obvr dass mit dem dynamischen
berechnen der skelette damit klingen besser treffen?" - and, after the
answer: "schreibe all das in eine spec mit offenem todo".

Read here as: instead of guessed radii, the blade's capsules on the living
take their size from the Havok collision shapes of whatever skeleton is
installed (vanilla, or a mod such as Ragdolls for Oblivion).

Markers as in the other specs: **Read** (seen in the code, a file or a
source), **Derived** (concluded from what was read), **Unverified** (still
to be shown), **Proposed** (a design).

## Verdict

**Measure first, then decide.** Reading the shapes is easy (every human
body part is a capsule, vanilla too), but vanilla's capsules are about half
as thick as OBVR's guessed ones at the torso. Switched over on vanilla
data, a blade would have to come closer before it hits. Which is right
depends on how well each fits the visible body mesh, and that is not
measured yet (todo 1).

## 1. What OBVR does today (Read)

- The blade meets capsules from bone to bone (weapon-collision-spec
  section 4 C, section 17). Their radii are fixed constants in
  `src/game/BladeContactLogic.h` (around line 490): head 7, neck 4.5,
  torso 10, upper arm 4.5, forearm 3.5, thigh 6.5, calf 4.5 game units.
- weapon-collision-spec 4 C: "Not measured on the meshes".
- Creatures without the known "Bip01" names fall back to the bound's
  upright column (`HandAtBody`).
- The skeleton's shapes are not read for the blade. The ragdoll hold does
  reach the bone's Havok body (`HavokBodyOfNode`, game/RagdollHold.h).
- Havok units are game units times 0.142877 (the scale at 0x00A39088,
  game/GrabPhysics.h), so a Havok radius times 7.0 is game units.

## 2. What the skeletons hold (Read, 2026-10-24)

Both `meshes/characters/_male/skeleton.nif` read with
`tools/nif-skeleton-shapes.py` (pyffi 2.2.4.dev4). Vanilla taken out of
`Oblivion - Meshes.bsa` with `tools/bsa-extract.py`; Ragdolls for Oblivion
1.1 (nexusmods.com/oblivion/mods/51844) from the installed Data folder.

Every body part in both is a `bhkCapsuleShape`; only vanilla's
`Bip01 NonAccum` is a sphere and its `SideWeapon` a convex shape. So the
earlier guess that vanilla has coarse convex shapes was wrong.

Radius in game units (Havok radius x 7.0), mass in the skeleton's units:

| Part | Vanilla r | Ragdolls r | OBVR today r | Vanilla mass | Ragdolls mass |
|---|---|---|---|---|---|
| Head | 4.2 | 6.3 | 7 | 30 | 6 |
| Spine / Spine1 | 4.6 / 4.6 | 7.0 / 7.0 | 10 | 60 / 50 | 30 / 30 |
| Spine2 | 5.7 | 8.4 | 10 | 40 | 30 |
| Upper arm | 2.9 | 3.5 | 4.5 | 40 | 5 |
| Forearm | 3.3 | 2.8 | 3.5 | 30 | 4 |
| Hand | 2.5 | 2.8 | - | 15 | 3 |
| Thigh | 4.8 | 4.9 | 6.5 | 45 | 7 |
| Calf | 4.1 | 3.5 | 4.5 | 30 | 6 |
| Foot | 3.0 | 3.5 | - | 15 | 4 |
| NonAccum | 7.2 (sphere) | 8.4 (capsule) | - | 60 | 30 |

Derived:
- Vanilla is thinner than OBVR's guesses everywhere, about half at the
  torso. Ragdolls for Oblivion is closer to them at the head and torso; its
  author says the shapes were "reshaped to fit models better" - his claim,
  not measured here.
- The whole body weighs about 190 with Ragdolls for Oblivion against about
  600 in vanilla. OBVR reads the mass at run time (`massInv`, GrabWeight),
  so a held foe is about three times lighter with the mod.
- The capsule's own end points (in the shape, not bone to bone) were not
  read; OBVR draws its capsules bone to bone.
- The tester felt the blade hit NPCs better with the mod installed
  (2026-10-23 evening, next-up.md). OBVR's blade does not read the skeleton,
  so that is not explained - not measured, placebo not excluded.

## 3. The design (Proposed)

- Per actor, per bone OBVR already knows (`BladeBoneName`): the bone's
  collision object, its rigid body, the shape. A capsule gives its two end
  points and radius in Havok units; times the body's transform and scale
  and over 0.142877 it is a game-unit capsule.
- Spheres as zero-length capsules; boxes as their enclosing capsule along
  the longest axis; convex and list shapes keep today's fallback.
- Cached per actor with a fingerprint and read anew after a load (the
  skin-swap lesson, as the bone pointers are now).
- Creatures get real shapes this way, without knowing their bone names:
  every bone with a collision object becomes a capsule.
- A switch for the source: guessed radii (today) or the skeleton's,
  default decided by todo 1.

## 4. Cost: medium

| Part | Size | Why |
|---|---|---|
| Reuse | - | the bone's Havok body (`HavokBodyOfNode`), the capsule maths and its tests in BladeContactLogic.h |
| Shape reading | S-M | capsule and sphere simple, box medium; the shape's layout in memory to read in the disassembly |
| Creatures | M | every bone with a body instead of the Bip01 list; the stoppers and hit zones that name parts |
| Risk | - | the Havok capsules may sit inside the visible mesh (vanilla's numbers point that way) |
| Tests | M | pure units for the shape-to-capsule maths, a harness scenario (bandit and a creature), the headset |

## 5. Open todo

1. **Measure the capsules against the body mesh** (small, offline, no
   game): with pyffi, the distance of the body mesh's vertices
   (upperbody.nif, lowerbody.nif, the head) to each bone's axis, against
   vanilla's, the mod's and OBVR's radii.
   - The skeleton's capsules fit: build section 3.
   - They sit well inside the mesh: re-measure OBVR's fixed radii on the
     mesh instead (cheaper, no mod dependency) and leave the skeleton out.
2. Read the capsules' own end points in both skeletons (the table has
   only radii) and compare them with OBVR's bone-to-bone segments.
3. The same for a few creatures (wolf, goblin, troll) - their Bip01 names
   and whether all their bodies are capsules.
4. Ragdolls for Oblivion and the hold: a held foe is about three times
   lighter - check the feel in the headset, GrabWeight may need its own
   value with the mod.
5. Why the blade felt better with the mod (2026-10-23): a harness swing
   with and without the mod, the contact log (Debug.ContactTrace) compared.
