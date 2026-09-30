# Physical combat: a Blade & Sorcery feel (2026-09-30, research, nothing built)

The tester asked whether OBVR can get "ein Blade&Sorcery Feeling für VR combat":

- "waffen haben ein gewicht gefühl, ein schwung verhalten, zb 2händer sind schwer, dolche nicht";
- "ich kann items vom boden direkt am grip aufnehmen und direkt als waffe zuschlagen";
- "kann waffen beliebig hantieren".

This spec covers:
1. what Blade & Sorcery (B&S) actually does, from sources;
2. what OBVR already has;
3. each piece of the feel, how it could be done in Oblivion, and how sure that is.

"Read" means seen in a source or the code. "Proposed" is a design. "Unknown" means the Oblivion side has not been researched yet.

## 1. How B&S does it (sources)

**The hand is not the controller.** A B&S developer on Reddit (r/BladeAndSorcery, "Slow mo and the strength multiplier", 2023):

> "The game works by having an invisible object track the position of your controllers exactly, with no delay. There is a joint - think like a really strong rubber band - that connects this invisible object to the player's in-game hands. This allows you to put your controllers through a wall in-game, but your in-game hands get stuck on the wall"

https://www.reddit.com/r/BladeAndSorcery/comments/17jpdkv/

**Weight is mass against that joint.**

- The B&S SDK FAQ gives the mass ranges: "Small items like daggers and small axes can have a lower weight (like 0.8 mass), swords have a medium sized weight (1.0-1.4 mass) and larger weapons like mauls and greatswords can have a higher mass to account for two-handed handling (2.0-8.0 mass)." It also moves the centre of mass for top-heavy items. https://kospy.github.io/BasSDK/Components/Guides/FAQ/ItemFAQ.html
- A heavy weapon lags behind the hand when swung too fast for its weight: "weapon dragging behind your hand means you are swinging it too fast for its weight" (Steam discussion "Weapon Weight", https://steamcommunity.com/app/629730/discussions/0/1640913421084764621/).
- A two-hander swung with one hand "just kind of flops" (r/BladeAndSorcery, "nerfs the weight of claymores", https://www.reddit.com/r/BladeAndSorcery/comments/nkjmqk/).
- The joint's spring and damper can be tuned in the options: a stronger spring means lighter weapons (https://www.reddit.com/r/BladeAndSorcery/comments/als2br/).

**Damage comes from the collision.**
- Items carry "damagers" and colliders, and whatever hits hard enough does damage. That is how anything can be a weapon (the SDK FAQ above).
- Handles define where an item can be held, and an item without one cannot be grabbed ("If you can't grab your weapon, it is usually an issue with handles").

So the feel comes from three things: a spring between the controller and the hand, the weapon's mass on that spring, and damage from what the moving body hits.

## 2. What OBVR has already (read in the repo)

- **Hands and the drawn weapon** are keyframed Havok bodies that follow the controllers exactly (`game/HandBodies`, hand-weapon-collision-spec.md).
  - They push the world, but nothing pushes them back.
  - The weapon has no weight: a dagger and a warhammer move the same.
- **The strike by motion** (`game/MeleeHits`) tests the drawn blade against the bodies along the hand's swing.
  - It hands the hit to the engine's own hit function 0x005FEBF0.
  - Light or power depends on the swing's length, the power kind on the blow's direction.
- **Two hands on a two-hander** (controls-spec 4.10): the weapon points along the line between the hands.
- **Held objects** go through the engine's grab (the mouse spring, `bhkMouseSpringAction`), driven to the hand (`game/GrabPhysics`).
  - Let go, they are thrown with the hand's speed.
  - A thrown thing staggers or knocks people down (`game/ThrowLogic.h`).
- **Oblivion's weapons carry the numbers the feel needs.** Read from Oblivion.esm's WEAP DATA on 2026-09-30:

  | Weapon (iron) | Type | Weight | Speed | Reach | Damage |
  |---|---|---|---|---|---|
  | Dagger | blade 1H | 3 | 1.4 | 0.6 | 5 |
  | Shortsword | blade 1H | 8 | 1.2 | 0.8 | 7 |
  | War axe | blunt 1H | 12 | 1.1 | 0.8 | 8 |
  | Mace | blunt 1H | 15 | 0.9 | 1.0 | 10 |
  | Longsword | blade 1H | 20 | 1.0 | 1.0 | 10 |
  | Claymore | blade 2H | 22 | 0.8 | 1.3 | 12 |
  | Battle axe | blunt 2H | 27 | 0.8 | 1.2 | 12 |
  | Warhammer | blunt 2H | 30 | 0.7 | 1.3 | 14 |

  These are from the Shivering Isles copies of the iron weapons (`SEWeapIron*`, `SEEnchIron*`). The base game's own iron weapons were not listed separately.

## 3. The pieces, and how each could be done

### A. Weight: the weapon lags the hand (proposed; feasible, OBVR only)

**The idea:** the weapon hand's rotation, and with it the weapon, follows the controller through a critically damped spring instead of exactly. The spring's stiffness falls with the weapon's weight.
- A dagger (3) follows almost at once.
- A warhammer (30) trails a fast swing and overshoots a little at the end.
- Holding it with both hands (4.10) makes the spring stiffer, for example ×2.5. A two-hander held in one hand stays sluggish, which is the B&S "flop".

**Where it goes:** OBVR already decides the weapon hand's rotation before the bones are pinned (`StepTwoHands`, `PinAdjustableHand`). The spring goes there.
- The strike by motion takes the lagged blade, so a hit lands where the weapon is seen.
- The Havok weapon body follows the drawn node already.

**Risks:**
- The gap between the controller and the drawn hand must stay small, or it reads as lag rather than weight. It needs a cap (B&S's joint has one too).
- The view is never touched, so there is no comfort risk for the head.

**Tests:** the spring as a pure function (step response per weight, cap, two-hand factor).

### B. Swing behaviour: heavy weapons need a longer swing and hit harder (proposed; partly unknown)

- **Light and power per weight (feasible, OBVR only):** the power-attack length (`PowerSwingMetres`, `PowerThrustMetres`) and the swing speed (`SwingLight`) can scale with the weapon's weight and its speed value.
  - A dagger swing counts sooner.
  - A warhammer needs a full swing but always lands as a power attack once it has one.
- **Damage by momentum (unknown):**
  - B&S's damage comes from the collision's energy. In Oblivion the damage is computed inside the hit handler from the weapon, skills and power attack.
  - A multiplier by the blade tip's speed needs a place in 0x005FEBF0 where the final damage can be scaled. It has not been looked for.
  - Until found, the only lever is light against power.
- **Stagger by momentum (feasible):** a heavy weapon's power hit could add the shove's knockdown when the tip is fast enough (`ShoveActor`, already used by throws).

### C. Picking a weapon up from the ground by its grip and striking at once (proposed; feasible, all parts built)

- **The flow:**
  1. The grip closes on a weapon lying in the world (a WEAP reference) near its handle end.
  2. OBVR takes it into the inventory the way activating would. Owned means stealing, with the crime rules the stow already uses (`TakeIntoInventory`).
  3. OBVR equips it (`EquipWeaponForm`) and draws it.
- **The draw is already fast:** `[Hands] WeaponDrawSpeed` (`game/WeaponDrawSpeed.h`, built 2026-09-27, default 10) runs the draw animation's time ten times faster. The draw takes about a tenth of the game's second. The first version of this spec called it an unknown, which was wrong (the tester, 2026-09-30: "dachte das haben wir bereits gefixt").
- **Where the handle is:** a weapon's grip end is its "Weapon" node origin, the attach point. The blade runs along the node's axis (the blade capsule already measures that). A grip near the origin takes the handle; a grip on the blade could take it by the blade (see E).

### D. Anything held is a weapon (proposed; feasible, reuses the throw)

- A held object (a chair leg, a bottle, a skull) swung into someone should hit them like a thrown one.
- The throw's check already follows an object and tests it against the living bodies. Run it on the held object too, with the hand's speed, and it staggers or knocks down at the throw's thresholds.
- **Damage:** the shove's effects carry no damage. A real hit would call the hit handler with no weapon, as a fist (hand-to-hand), which the strike by motion can already do for fists. The held object's weight could scale it; the "unknown" of B above applies.

### E. Free handling (partly feasible, partly an engine limit)

- **Where on the handle the right hand holds (feasible):** slide the weapon along its own axis relative to the hand, the node's local offset, so a grip lower or higher on a long handle is kept.
- **Half-swording, holding a sword by the blade (feasible, visual):** the same offset, past the guard.
- **Turning the weapon in the hand, a reverse grip (feasible):** a twist of the wrist with the grip half open (a gesture to define) flips the node's local rotation 180°. The strike by motion follows the drawn node.
- **The weapon in the left hand, or passing it between hands (engine limit):**
  - Oblivion has one weapon slot and draws it on the right hand's "Weapon" node, and the left-handed mode (`LeftHanded`) swaps whole controller roles.
  - Moving the drawn node under the left hand bone is possible in the scene graph, but the engine's own code (the sheathe, the hit, the animation) expects it on the right. It is risky and unresearched.
  - Dual wielding is not possible.
- **Throwing the drawn weapon (feasible with C in reverse):** unequip it, drop it at the hand as a world reference, and give it the hand's velocity like a thrown object. It then hits by D.

### F. Hands that stop at walls and bodies, B&S's physics hands (large; unknown)

- B&S's hand is a dynamic body on a joint, so it stops at a wall while the controller goes through.
- OBVR's hands are keyframed and go through everything; the weight in A only lags them.
- Making them dynamic bodies driven by a spring or constraint towards the controller would give walls, parries (blade on blade) and resistance on a hit.
- It is the largest piece: Havok constraints or a motor, and the engine's own collision for the player's hands. Nothing of that has been read yet.
- A cheaper part of it is feasible: a blade's cast along its swing, stopped where it meets static geometry (the hand bodies' world queries exist).

### F2. The held weapon against bodies: the blade stops at a body (researched 2026-09-30)

The tester asked whether the carried weapon could get body collisions. Read here as: the drawn weapon meets a person's body (living or dead, and the player's own) and stops at it or slides along it, instead of passing through, with the hit taken from that contact.

**What is in Havok's world** (read in Oblivion.exe 1.2.0.416):

- **The living have no bone bodies in the world.**
  - An actor's bones carry `bhkBlendCollisionObject`s (RTTI 0x00BA7A20).
  - `bhkRigidBody`'s AddToWorld (0x008A48C0) takes a bone body out of the world, and does not add it, while it is fully keyframed: blend +0x14 and +0x18 ≥ 1.0, +0x24 ≤ 0 (0x008A493C–0x008A4962).
  - The only exception is the flag `[0x00BA7909]`, copied from the INI setting `bAddBipedWhenKeyframed:HAVOK` (0x00441A66, default 0; STEP's INI guide agrees: https://stepmodifications.org/wiki/Guide:Oblivion_INI/HAVOK).
- **So a living person is only their character controller** in the world (layer 20): a coarse capsule of the actor's radius, not the shape of the body.
- **The dead, the knocked-down and the ragdolled** have their bones in the world on BIPED (layer 8). That is why the weapon pushes ragdolls today.
- **The player's own bones** follow the same rule (keyframed while alive, not in the world). The player's controller has the same group as OBVR's bodies, and the filter lets no two bodies of one group collide (0x008A7F70), so nothing touches the player's own body. Whether the first-person skeleton carries blend objects at all was not read.

**What a keyframed weapon can do:**
- OBVR's weapon body is keyframed, so it has infinite mass. Havok's `hkpMotion.h` says the velocity of a keyframed body "is NOT changed by the application of impulses or forces" (https://github.com/nitaigao/engine-showcase/blob/master/etc/vendor/havok/Source/Physics/Dynamics/Motion/hkpMotion.h, a later Havok than Oblivion's 3.1).
- It pushes a controller (seen in the headset, now off by default via `PushPeople`) and a ragdoll. Nothing can stop it.
- **Havok alone will not stop the blade at a body.**

**Three ways:**

1. **OBVR's own body capsules and a clamp (recommended; no Havok needed).**
   - Each frame, build capsules from the nearby actors' drawn skeleton nodes: head, spine, upper and lower arms and legs. The hand body's span is already built this way (`HandSpanFromBones`).
   - Sweep the blade, the drawn span from `BladeSpanFromNode`, from last frame's pose to this frame's against those capsules.
   - At the first contact:
     - the drawn weapon and hand are held at the surface and slide along it;
     - the hit goes to the engine from that contact (0x005FEBF0, as the strike by motion does now), replacing the bound-sphere test in `MeleeHits`;
     - which body part was hit is known, for a later damage or effect by part.
   - The gap between the drawn hand and the controller has a cap; past it the weapon lets go of the body, like B&S's joint. The view is never moved.
   - It works for the living and the dead alike, because it reads the drawn bones. The player is left out of the capsule set.
   - It is pure arithmetic, so every flow can be tested (the FrameLogic pattern).
   - Cost: about ten capsules per nearby actor and one swept segment per frame. The actor walk already exists (`MeleeHits`).
2. **A dynamic weapon on a spring** (B&S's joint; `GrabPhysics` already drives held objects this way).
   - Havok would stop it at walls, statics and ragdolls. It would likely pass through a living person's controller: the proxy is a phantom, and phantoms take no part in the solver. That is derived, not verified in 3.1.
   - It carries the known split between the picture and the physics, and fast swings tunnel between 60 Hz steps (tips measured at 5–40 units a step).
   - It is for walls and parries later, not for bodies.
3. **The living's bones put into the world** (`[0x00BA7909]` = 1 at runtime; never in the INI, see the iSize lesson).
   - Their bones would enter the world as keyframed layer-8 bodies, for queries to find.
   - It only acts when a body is added, so the actors already loaded would need re-adding.
   - STEP warns that on Skyrim the same switch causes "characters to fly" and "funky animations".
   - It costs 15–20 bodies per actor.
   - High risk; worth an experiment only if the capsules of way 1 are too coarse.

**Havok queries known or found** (for the world, and for way 3):
- Built and used: the bhkWorld ray pick (vtable +0x88, `PickWorldSegment`).
- Found, not yet usable: `bhkSimpleShapePhantom` (constructor 0x00531FC0), `bhkCachingShapePhantom` (vtable 0x00A9840C) and `hkClosestCdPointCollector` (vtable 0x00A967A8, used at 0x00894A33 and 0x009022B5).
- Not found: the phantom's cast and closest-point slots in 3.1, their input and collector layouts, and `hkWorld::linearCast` or `getClosestPoints`.
- These are needed for the blade against walls, not against bodies.

**Unknowns:**
- What blend +0x24 counts: above 0, the bones are added even while keyframed, perhaps during a hit reaction.
- How an arrow picks a body part (0x00609DF0 resolves a hit node's collision object against the blend RTTI); this could give the body part names for way 1.
- Whether creatures without ragdolls have blend objects.
- Whether a dynamic body passes through character proxies in Oblivion.

**Proposed place in the order:** after A (weight). Way 1 replaces the strike's bound-sphere test with contacts. The lag spring of A and the clamp of way 1 then work on the same drawn weapon: the controller pulls, the body stops, and the spring lets the weapon trail.

### G. Out of reach in this engine

- Stabbing that sticks (impaling), cutting and dismemberment: Oblivion has no support in its meshes or code.
- Grabbing an enemy's weapon out of their hand: unknown. The disarm drops it, and the weapon on the ground could then be picked up by C.

## 4. Proposed order

1. **A, weight as a lagging spring,** with the two-hand factor. The largest part of the feel, all OBVR's own code. Harness: the lag step per weight in the log; the headset judges the feel.
2. **B, swing thresholds per weight.** Small, on top of A.
3. **D, held objects hit.** Reuses the throw.
4. **C, pick up by the grip.** The draw is already fast (`WeaponDrawSpeed`).
5. **E, grip position and reverse grip.**
6. **B's damage by momentum.** First look for the scale point in the hit handler.
7. **F, physics hands:** a spec and a research pass of their own.
8. **F2, the blade stops at bodies** (way 1, OBVR's body capsules and a clamp): after A, it replaces the strike's bound-sphere test.

## 5. Open questions

- How strong the lag should be at each weight (tuning in the headset: settings per weight class, or one "weapon weight" factor).
- Whether the drawn hand should lag with the weapon (B&S) or only the weapon should turn around the hand. Both are feasible. The hand lagging is truer to B&S; the weapon alone keeps the hand on the controller.
- Where exactly in the hit handler the damage can be scaled (B).
