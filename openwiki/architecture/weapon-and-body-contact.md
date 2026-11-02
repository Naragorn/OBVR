---
type: architecture
title: Weapon and body contact
description: How a drawn weapon, a weapon head and the free hands are swept against walls and against capsules fitted to each actor's skinned meshes, how contact is held, slid off and turned into haptics, and where the weapon's own shape comes from.
tags: [combat, collision, capsules, weapons, full-vr]
sources:
  - id: openwiki-source-a66fb70ef9c23adff057a8e2
    resource: repo://src/core/Config.cpp
  - id: openwiki-source-a49a35d759d588d8b8cd5bac
    resource: repo://src/game/BladeContactLogic.h
  - id: openwiki-source-39f90f3eb3bf6bf77c85fddf
    resource: repo://src/game/CapsuleFitLogic.h
  - id: openwiki-source-162a9d6c9be32402fde385ff
    resource: repo://src/game/DismemberLogic.h
  - id: openwiki-source-1826578918d285166d0617b3
    resource: repo://src/game/GameAddresses.h
  - id: openwiki-source-09639198c1df61aba61714e5
    resource: repo://src/game/ImpactLogic.h
  - id: openwiki-source-8e8cd1c43fde4fc97c8b7665
    resource: repo://src/game/KnockdownRoll.cpp
  - id: openwiki-source-4b30d49c3e4c73b1916dff2b
    resource: repo://src/game/Shove.cpp
  - id: openwiki-source-d69e96073d313fa6adf19408
    resource: repo://src/game/WeaponShapeLogic.h
  - id: openwiki-source-eba6390f286c9b19d7465c49
    resource: repo://src/vr/HandMode.h
generated: { by: "claude-code", at: "2026-11-02T16:46:23.688Z" }
verified:
  - by: openwiki/0.7.3
    at: 2026-11-02T16:46:23.688Z
---

# Weapon and body contact

In Full VR the weapon and the hands follow the controllers, but must not pass through walls or people. The aim is Blade & Sorcery's feel: steel rests on armour, slides off it, and only strikes when it actually touches. All decisions are pure functions in `src/game/*Logic.h`; the reading from the engine is in the `.cpp` files next to them, and the glue is in `src/camera/CameraHook.cpp`. Design rationale and the open-bug lists: `docs/weapon-collision-spec.md` (§25 for the current mesh-shaped contact) and `docs/mesh-fitted-capsules-spec.md`.

## Bodies as capsules

People are represented as capsules per body part (`BodyPart::Head`, `Neck`, `Torso`, arms, legs) in `game::BladeBodies`, at most `kBladeBodyCapsulesMax` (384) per frame.

With `[Hands] FittedBodyCapsules` on, the capsules are fitted to the meshes each actor actually wears (`src/game/CapsuleFitLogic.h`, read by `src/game/FittedCapsules.cpp`):

- each vertex goes to the bone group that weighs it most, taken into that group's bone space through the skin's bind data;
- one capsule runs along the bone to its child (or along the vertices' principal axis for head, hands and feet);
- the radius covers the 95th percentile of the cross-section (`kFitPercentile`), so stray vertices do not inflate it;
- a flat torso (flatness 1.3 or more) gets two capsules side by side;
- a long group is split along its length when one end is clearly wider than the other (`SplitAlongLength`, ratio `[Hands] FitLengthSplitRatio`, 0 disables it);
- capsules stay in bone space and are placed by the bone's world transform each frame.

A fit is started per actor (`StartActorFit`); blood decals are left out by geometry name before their skin is read, and skin reads check that a bone's memory region is readable (`ReadableRegions`) and that its first word is a vtable in Oblivion.exe's `.rdata` (`addr::IsGameVtable`) — both guard against freed or foreign bones. `tools/capsule-fit.py` runs the same fit offline.

## The weapon's shape

With `[Hands] FittedWeaponShapes` on, `game::FitWeaponShape` (`src/game/WeaponShapeLogic.h`) reads the weapon mesh (`src/game/WeaponShape.cpp`) into node space and finds:

- the line from grip to tip through the middle of the cross-section;
- a per-slice reach over 16 slices (`kWeaponSlices`);
- a **head** — slices near the tip that stand out over the shaft (more than `kWeaponHeadOverShaft` × the shaft, and by an absolute margin) — with its own section: flat for an axe blade, round for a mace's flanges.

The hands are fitted the same way (`[Hands] FittedHands`). A sparse haft with few vertices falls back to simpler sections rather than failing.

## The sweep

`SweepBlade` (`src/game/BladeContactLogic.h`) sweeps rays per blade point from last frame's pose to this frame's and returns the earliest hit with a *safe fraction* — how far the pose may move:

- **Walls** hold the blade `kBladeContactMarginUnits` (1.5 units) short, or the head's reach toward the surface when that is larger.
- **People:** each capsule is grown by the blade section's reach toward it (`PersonMargin`, reach + 0.1 spare). A hit on the grown capsule is held there with no further back-off, so the steel rests on the armour rather than hovering.
- A point already only just inside a capsule (`ShallowBand`) is held where it is.

`SweepShape` sweeps both spans (line and head) and keeps the one that must stop soonest — smallest safe fraction, ties by fraction.

After the sweep, `SlideOffPeople`, `TurnOffPeople` and `ShareOffPeople` let a held blade slide along a body or turn off it instead of sticking. `StepScrape` produces controller pulses only while the held point slides faster than `kScrapeMinUnitsPerSecond` (10.5 units/s, about 0.15 m/s), scaled by speed.

## Measuring the fit

`[Debug] MeshGapTrace` logs, for a resting blade or hand, the gap between the capsule surface and the actual skinned mesh (`src/game/MeshGapLogic.h`): above zero the point is in the air over the mesh, below zero inside it. Harness scenarios `mesh-gap-*.txt` use it to check fits against real armour.

## Blunt blows: one impact speed

Every blow by a body - the open hand's shove and slap, the off hand's punch, the torch, the shield bash, the pommel, the half-sword's hilt, a held person thrown, a thrown thing - is judged by one pair of speeds since 2026-11-02: `[Hands] StaggerSpeed` (3 m/s at the contact) staggers, `KnockdownSpeed` (6 m/s) knocks down (`src/game/ImpactLogic.h`, `ImpactTier`). `vr::ApplyImpact` (`src/vr/HandMode.h`) hands the pair to each way of striking's own settings after the INI is read, so the older per-way keys are gone. The values follow measured punch speeds; the reasoning is in `docs/physical-combat-spec.md`. The kick is the exception: it has no measured speed and keeps its standing/running rule (see [Full VR hands](full-vr-hands.md)).

Every one of these blows lands through `game::ShoveActor` (`src/game/Shove.cpp`). With `[Hands] VanillaKnockdown` on (default off) a knockdown there first rolls the engine's own chance (`game::RollKnockdown`, `src/game/KnockdownRoll.cpp`): the formula of `0x005475D0` over the player's fist damage and the target's Agility, Luck and fatigue, read from the game's loaded GMSTs after checking each slot's name; a failed roll staggers instead.

## Dismemberment

A blade or an axe swept fast enough through a dead neck or wrist cuts the head or the hand off (`src/game/DismemberLogic.h` decides, `src/camera/hands/Dismember.cpp` steps it, `src/game/Dismember.cpp` releases the ragdoll joint and redirects the skins). The living keep their parts unless `DismemberCutKills`; a living neck the blade went through waits up to 0.4 s for the killing blow's death. Two guards stand before the cut: an **essential** actor never loses a part, checked before the death because an essential at no health lies unconscious and reads dead; and a **blunt blow** (kick, bash, pommel, hilt) forgets a waiting killing-blow cut on that actor and holds their neck for 0.5 s, so a kick never takes a head. Rationale: `docs/dismemberment-spec.md`.

## Tests

`tests/BladeContactTest.cpp`, `CapsuleFitTest.cpp`, `WeaponShapeTest.cpp`, `MeshGapTest.cpp`, `GameVtableTest.cpp` and `RefFingerprintTest.cpp` cover the pure logic; scenarios such as `blade-wall.txt`, `blade-slides-off.txt`, `fitted-capsules.txt`, `mesh-gap-soldier.txt` and `mace-wall.txt` in `tools/hand-scripts/` run it in the game (see [Hand-script harness](../testing/hand-script-harness.md)).
