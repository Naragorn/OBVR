# Physical combat: a Blade & Sorcery feel (2026-09-30, research; section 6 built 2026-10-09)

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

- **Open (Nadi, Windows, 2026-10-24): fewer swings; a light swing needs more travel, a
  heavy one too** ("Schwünge reduzieren. Leichter Schwung braucht mehr Strecke. Schwere
  auch mehr Strecke"). Today [R]: a swing starts at `SwingLight` 2.6 m/s and counts from
  `kSwingMinMetres` 0.08 m (vr/HandInput.h, a constant); power from `PowerSwingMetres`
  1.2 m, a thrust from `PowerThrustMetres` 0.45 m. His log of that evening: 37 light swings
  (0.60 m on average, 6 under 0.30 m) and 6 power [R]. To decide: the light minimum (0.08 m
  to perhaps 0.25 to 0.35 m, then a setting) and the power lengths; per weight as above.
  Cheap: settings and hand_input_test flows; the feel is the headset's.

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

The weapon's part of F and F2 (walls, bodies, parries, shields) is designed as one contact step in [weapon-collision-spec.md](weapon-collision-spec.md) (2026-10-09, nothing built).

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

- Stabbing that sticks (impaling): Oblivion has no support in its meshes or code. Dismemberment has none either, but a cut by bone pointers on the killing blow is designed in dismemberment-spec.md (2026-10-25, not built).
- Grabbing an enemy's weapon out of their hand: unknown. The disarm drops it, and the weapon on the ground could then be picked up by C.

## 4. Proposed order

1. **A, weight as a lagging spring,** with the two-hand factor. The largest part of the feel, all OBVR's own code. Harness: the lag step per weight in the log; the headset judges the feel.
2. **B, swing thresholds per weight.** Small, on top of A.
3. **D, held objects hit.** Reuses the throw.
4. **C, pick up by the grip.** The draw is already fast (`WeaponDrawSpeed`).
5. **E, grip position and reverse grip.**
6. **B's damage by momentum.** First look for the scale point in the hit handler.
7. **F, physics hands:** a spec and a research pass of their own.
8. **F2, the blade stops at bodies** (way 1, OBVR's body capsules and a clamp): after A, it replaces the strike's bound-sphere test. Now part of [weapon-collision-spec.md](weapon-collision-spec.md).

## 5. Open questions

- How strong the lag should be at each weight (tuning in the headset: settings per weight class, or one "weapon weight" factor).
- Whether the drawn hand should lag with the weapon (B&S) or only the weapon should turn around the hand. Both are feasible. The hand lagging is truer to B&S; the weapon alone keeps the hand on the controller.
- Where exactly in the hit handler the damage can be scaled (B).

## 6. Weight as one slider: the proposal of 2026-10-09 (built 2026-10-09, headset open)

The tester (2026-10-08): "wie würden wir nun den waffen ein schwere gefühl
geben ähnlich zu blade & sorcery ... vorschlag? und stärke muss per
prozent regler einstellbar sein 1-100%". Section A above is the mechanism;
this is the shape it should take, with the knob he asked for.

### What is built on, read in the repo (2026-10-09)

- The weapon hand is pinned to the controller every frame
  (`PinHandBone`, game/HandBones.cpp): the forearm is placed so the hand
  bone lands on the controller; the "Weapon" node hangs under the hand
  bone and goes with it. The two-hand grip (`StepTwoHands`, CameraHook)
  and the adjustable hand (`PinAdjustableHand`) decide the hand's pose
  first; the pin writes it.
- The strike by motion tests the blade's capsule along the hand's swing
  (`g_hand.swingActive`, `game::MotionStrike`); the swing is the HAND's
  speed (vr/HandInput.h, `StepSwing`), the blade follows.
- The weapon's Havok body (`HandBodySlot::Weapon`, game/HandBodies.cpp) is
  keyframed to the drawn weapon node each frame: wherever the node is
  drawn, the body is.
- The weapon's weight is the form's (TESObjectWEAP, its TESWeightForm), as
  the item rows show it; iron dagger 3, longsword 24, claymore 36,
  warhammer 42 (the table in section 2).

### The feel, in one sentence

The weapon does not sit on the controller; it is pulled after it by a
spring whose stiffness falls with the weapon's weight, so a dagger is on
the hand and a claymore trails a fast swing by a hand's breadth and swings
through at the end - and one slider, **Weapon weight 1-100 %**, scales
how much.

### The design

1. **A lagging pose for the weapon hand.** Each frame the wanted hand
   pose is the controller's (as now). The DRAWN pose is a critically
   damped spring following the wanted one, position and rotation apart:
   `drawn += (wanted - drawn) * share`, `share = dt / (tc + dt)`, with
   the time constant `tc = tcMax * weightFactor * strength`:
   - `weightFactor = clamp((weight - 3) / 40, 0, 1)` - a dagger 0, a
     warhammer 1 (the table's spread);
   - `strength` is the slider, 0.01-1.00 (`[Hands] WeaponWeight`, 1-100 %,
     default 40 %);
   - `tcMax` 0.12 s: at 100 % a warhammer lags a 4 m/s swing by ~0.5 m
     without the cap below; at 40 % by ~0.2 m.
   Two hands on the grip (4.10): `tc` × 0.4 - a claymore in two hands
   follows nearly like a sword in one.
2. **A cap on the gap**, so a slow hand never reads as lag: the drawn
   hand is never more than `capMetres` (0.25 m at 100 %, scaled by
   strength) from the controller, and never more than 35 degrees off its
   heading; beyond the cap it is clamped to the cap. B&S has the same cap
   in its joint limits.
3. **Swing-through**: when the hand stops, the drawn weapon carries on to
   the cap and comes back - the spring does it on its own, nothing to add.
4. **The strike takes the drawn blade.** `MotionStrike` is fed the drawn
   weapon's capsule and speed, not the hand's: a hit lands where the
   weapon is seen, and a claymore swung too fast arrives late and softer
   (its drawn tip speed is the lagged one).
5. **The Havok body follows the drawn node** as it does now; nothing to
   change.
6. **The swing's thresholds per weight** (section B): the power swing's
   length × (1 + weightFactor × 0.5) and the swish's speed likewise - a
   warhammer wants a longer, fuller swing. Optional, after 1-4.

### What it does not do

- The hand stays on the controller for everything but the weapon: the
  reach, the ring, the laser, the grab are the controller's. Only the
  drawn hand/weapon pair lags. (Section 5's open question, decided: the
  drawn hand lags WITH the weapon, as in B&S, so the grip is seen in the
  hand - but the controller's own place drives everything else.)
- No damage scaling by momentum yet (section B's unknown); the weight
  changes WHEN and WHERE the hit lands, not how hard.
- Bows and staffs: no lag (the bow is aimed; a lagging bow would miss).
  Fists: none.

### The slider

- `[Hands] WeaponWeight=40` (1-100), settings "Weapon weight (%)", hot
  reloaded. 1 % is as now (on the controller); 100 % is B&S's heaviest
  feel. Nothing else to tune in the INI; `tcMax` and the cap are code
  constants scaled by it.

### Tests and evidence

- The spring as a pure function (vr/WeaponWeight.h): the step response
  per weight and strength, the cap, the two-hand factor, a NaN weight,
  dt 0.
- The log, once per draw: "Weapon weight: <name> <weight>, time constant
  n ms at n %, cap n m" and, limited, the largest gap of a swing.
- The headset judges the feel; the harness can only show the gap.

### Order and size

A day's work for 1-5 (the spring, the cap, the strike fed the drawn
blade, the slider, the tests); 6 an hour after. Built on the
`hand-tracked-mode` line as everything else; off at 1 % by setting, so a
tester who dislikes it loses nothing.

### Built (2026-10-09): 1-5, as designed, with these decisions

The tester's one condition: "Mir ist nur wichtig das der ingame weight
wert der waffe in die gewichtung mit einfliesst" - it does, and nothing
else sets the lag.

- **The weight is the item's own.** `game::WeaponWeightOf` reads the WEAP
  form's TESWeightForm float at +0x7C (`addr::kWeaponWeightOffset`; xOBSE's
  GameForms.h: TESValueForm 0x70, TESWeightForm 0x78 = vtable + `float
  weight`, TESHealthForm 0x80, TESAttackDamageForm 0x88, then the type at
  0x90 and reach at 0x98 the hit function was seen to read). A figure that
  cannot be a weight (not a number, negative, 1000 or more) is refused and
  the weapon does not lag. **Open: the figure against the item's row** -
  the log says "Weapon weight: Iron Dagger 3.0, ..." at each draw, and the
  first headset run checks it against the inventory (dagger 3, longsword
  24, claymore 36, warhammer 42). The camera hook fills
  `HandModeFrame::weaponWeight` only with a swung weapon drawn (not the
  fists, which have no form; not a staff; not a bow).
- **The lag runs in the room, not relative to the head.** The spring
  (`vr/WeaponWeight.h`, `StepWeaponLag`) follows the controller's pose in
  tracking space; the drawn pose is then put relative to the head the way
  the controller's own is. Lagged head-relative, a head turned with the
  hand held still would have swung the weapon; and walking moves the game's
  camera, not the room's poses, so the weapon rides along without lag.
  Position and orientation apart: a straight share of the gap for the
  position, a slerp by the same share for the orientation (no acos in the
  cross build: the angle is atan2 of the turn quaternion's sine and
  cosine).
- **Two ways to follow, by `[Hands] WeaponSwingThrough`** (on by default;
  the tester the same day: "ja machen wir das nachschwingen auch aber
  hinter einem feature toggle (default on)"):
  - *On, the swing-through*: the plain lag's spring and drag, and the
    weapon's mass besides (`x'' = omega^2 (hand - x) - 2 zeta omega x'`,
    the drag against the room). A moving hand is trailed exactly as far as
    the plain lag trails it, speed x tc - the weight; a hand that starts
    leaves the weapon up to half as far again behind for a moment (its
    inertia); a hand that stops is overrun by it, by about half the trail,
    and it swings back. Damping ratio 0.4 (a release from rest overshoots
    by e^(-zeta pi / sqrt(1 - zeta^2)) = 25 %), omega = 2 zeta / tc so the
    drag over the spring is the plain lag's tc: the heaviest at 100 %
    swings at 6.7 rad/s (a 1.0 s period), at 40 % at 17. Each frame is
    the damped oscillator's closed form (Ryan Juckett, "Damped Springs",
    2012, the four coefficients) about the trail's rest point: exact for a
    hand that moves evenly within the frame (the test checks it against a
    Runge-Kutta integration of the equation), stable however long the
    frame. `exp` was added to core/MathFns.h for it - msvcrt exports it
    (dumpbin on SysWOW64\msvcrt.dll, ordinal 1213, beside
    sin/cos/sqrt/atan/tan). Switched on in the middle of a draw it takes
    over without a push: the plain lag keeps the drawn pose's motion.
  - *The first swing-through, replaced the same day*: a spring and a damper
    on the weapon's motion RELATIVE to the hand, as B&S's joint does, with
    omega = 1 / tc. The headset run (2026-10-09): "Weapon weight war da
    aber erst nachdem ich nachschwingen ausschaltete. mit dem an war
    einfach ohne weight." Why: relative damping lags by acceleration x
    tc^2 and not at all at a steady speed. Run on swings shaped like the
    log's (0.25-0.8 m at 3.2-4.5 m/s, an Iron Warhammer 30 at 40 %), it
    trailed by about half the plain lag at the swing's fastest moment, and
    with both hands on the handle (tc 13 ms, two of the four logged swings)
    by under 1.2 cm and 1.2 degrees - on the hand. The drag against the
    room is what the plain lag's weight is; the mass only adds to it.
  - *Off, the plain lag*: first order, `drawn += (wanted - drawn) * dt /
    (tc + dt)`. It trails a moving hand by speed x tc, carries on to where
    the hand stopped and never past it.
- **The cap**: 0.25 m and 35 degrees at 100 %, both scaled by the slider;
  beyond it the drawn pose is pulled onto the cap along the line (and the
  arc) towards the controller, and the momentum that would carry it further
  out is taken away, so the weapon rests at the cap rather than presses on
  it. A jump of the raw poses (SteamVR's seated zero reset) is bounded by
  it and caught up in ~0.1 s.
- **Who takes the drawn pose** (`HandModeResult::weaponHandRotation` and
  `weaponHandOffsetUnits`, equal to `rightHand*` while nothing lags): the
  weapon hand's pin, the two-hand grip (the left hand holds the handle
  where it is seen), the strike by motion (the hit lands where the weapon
  is seen, with the lagged tip speed), the hands' Havok bodies and the
  push segment. **Who keeps the controller's**: the laser, the ring, the
  reach, the grab, the swing detector (the controller's speed starts a
  swing; the thresholds per weight of item 6 are not built), the
  controller models drawn into the eye.
- **Not while adjusting the hands** (the fit measures the controller), not
  in third person (no hand pose there), taken up where the hand is at each
  draw (no jump), let go at the sheathe.
- **The slider**: `[Hands] WeaponWeight=40` (1-100; the settings row
  "Weapon weight (%)", step 5), hot reloaded; the time constant is
  `0.12 s * clamp((weight - 3) / 40, 0, 1) * slider`, times 0.4 with both
  hands on the handle.
- **The log**: "Weapon weight: <name> <weight>, time constant n ms at n %,
  cap n m / n degrees, swinging through | the plain lag" once per draw,
  and "Weapon weight: swing n - the weapon was up to n m and n degrees
  from the hand, within half a second of it" for the first six swings
  with a lag (the half second past the swing is where the swing-through
  shows).
- **Tests**: `weapon_weight_test` (the weight's share, the slider's
  clamps, the tuning per weight, grip and model, the slerp, the angle and
  the rotation vectors, the oscillator's step composing and decaying as
  the closed form says, the plain lag's step: taken up, a frame behind by
  the share, caught up and never past, no time, both caps, both hands,
  let go and taken up again; the swing-through: taken up without
  momentum, every frame against a Runge-Kutta integration of the equation
  (a sweeping hand with a jump in it), a release from behind that passes
  the hand by the damping's quarter and settles, a hand at a steady speed
  trailed exactly as the plain lag trails it and further as it starts, a
  stop overrun by the analytic peak, a jump, the cap taking the momentum
  and a hand too fast for it ridden at the cap, no time, let go, the
  switch from the plain lag mid-draw without a push, the rotation's
  overshoot, steady trail and cap, 40 % against 100 %) and
  `TestWeaponWeightInMode` in `hand_mode_test` (through the mode: the
  controller's pose untouched, the drawn pose behind in units and
  heading, sheathed, a dagger, adjusting, both hands, 1 %, third person,
  and the swing-through overrunning a jump). All unit tests pass.
- **Open**: the feel in the headset (the time constant and the caps are
  code constants to retune from there); item 6.

### A two-hander in one hand (wished 2026-10-24, built 2026-10-26, harness PASS, headset open)

The wish of 2026-10-24: "2-Hand-Waffe ist mit einer Hand getragen schwerer als mit zwei
Händen." Nadi, 2026-10-26, the full request: "2 händer, ausser staffs: mit nur einer hand
getragen hat dann 40% (default) mehr "Gewicht" in vr. Mit beiden Händen gehalten: normales
Gewicht wie bisher. Mit nur einer hand 40% (default) mehr Ausdauer kosten auf alles. und neu:
Mit nur einer hand hochhalten über der Hüfte kostet jz bei 2 händern auch schon Ausdauer
(default on). Runter gehalten unter der hüfte aber kostet auch mit nur einer hand nix. Mit
beiden Händen kein Ausdauer verlust nur beim halten auch über der hüfte. den 2 händer auf der
schulter ruhen lassen mit nur einer hand kostet auch keine ausdauer."

Built (vr/OneHandTwoHander.h, game/FatigueCost.h, one_hand_two_hander_test; settings in
`[Hands]`). "A two-hander" is a two-handed blade or blunt weapon (types 1 and 3), never a
staff; "in one hand" is drawn without the left grip on its handle (`TwoHandHolds`) - the open
left hand hovering near the handle no longer stiffens the weapon as if it held it.

- **The weight** (`OneHandTwoHanderWeight`, 40): the lag's time constant and both caps x1.4
  (`WeaponLagHeavier`, the caps held to 0.5 m / 90 degrees as exhaustion's), so a heavy
  weapon past the weight's full share is heavier too and a fast swing is not cut off at the
  old cap. Both hands: as before (x0.4). The draw's "Weapon weight:" line gives both.
- **The costs of fighting** (`OneHandTwoHanderFatigue`, 40): an attack, a block, a swing, a
  parry, a shove, a grab x1.4 (`vr::OneHandScales`). The engine pays every fatigue cost
  through 0x005E07D0 [R]; of its seven calls (game/FatigueCost.h has the addresses and the
  formulas) the attack's and the block's are pointed at OBVR, which scales the player's
  change; OBVR's own swing, parry, shove and grab go through `PlayerFatigueCost` by kind.
  The first build (0a374f4) made every cost dearer, running, a jump, a spell, a bow and the
  teleport too; Nadi, the same evening: "nur auf angriff block, swung, parade, schubsen,
  packen" - those five cost as the game has them again. Fatigue others take - a fist's blow,
  a spell that damages it - is not paid through 0x005E07D0 [R: none of the seven is] and
  stays as it is. Resting on a shoulder or held low the costs of fighting are still x1.4:
  the request ties them to "one hand", the shoulder and the hip to the holding.
- **Holding it up** (`OneHandTwoHanderHoldCosts` 1, `OneHandTwoHanderHoldFatigue` 5 a
  second): with the weapon hand's controller over the hip **the bar falls** 5 a second. The
  game gives fatigue back all the while - 0x00547F20, cdecl float(int Endurance) =
  Endurance x fFatigueReturnMult + fFatigueReturnBase, called once, by the actor's update
  at 0x005F27DB with actor value 5 and times the frame's time [R, 2026-10-27]; vanilla 10
  a second (Mult 0) [W: CS wiki "Fatigue Game Settings"], 5 on Nadi's Windows install [M,
  derived from his log: held up, 15 fell 10 a second, 10 fell 5]. So OBVR burns the fall
  plus that return (vr::HoldBurnPerSecond, game::PlayerFatigueReturnPerSecond). The first
  build burned a fixed 15 - the game's figure for a novice's bow held drawn
  (fMarksmanFatigueBurnPerSecond, 0x005FACFC) [R]: vanilla's bar fell 5, Nadi's 10, and his
  5 not at all (180.0 of 180.0 over 3.9 s held up, Windows 2026-10-26). Nadi, 2026-10-27:
  "go A. ziel: mit nur einer hand hochhalten kostet ausdauer".
  Held low, in both hands at any height, or resting on a shoulder: nothing. The hip is
  `HipBelowEyesMetres` (0.65, the holster's adult with eyes 1.6 m and hip 0.95 m), set by
  the T-pose with the other places; a 2 cm band keeps the line from flickering. Resting:
  the weapon - 0.12 m and more up from the grip - within 0.10 m of a shoulder's top (0.13
  to stay) with its tip behind it; the shoulder's top 0.15 out, 0.08 behind, 0.17 under the
  eyes at the default hip [D: body proportions, not measured on a player].

**Measured (harness, two-hander-one-hand.txt, Linux, 2026-10-26)**: in one hand raised 15.0 a
second burnt (63.2 in 4.2 s); a light swing 12.88 = 9.20 x 1.40, in both hands 9.20; the
game's own attack (MotionHits 0, two-hander-attack-cost.txt) 9.20 x 1.40 = 12.88; with every
cost scaled (0a374f4) the game's jump 10.00 x 1.40 and running x 1.40 - since the fix neither
is (no scaled line, `reject x1.40 = `); the block not run in the harness (it needs a blow
blocked), its call checked byte for byte at the start; held low 0.0 burnt in 5
s; both hands 0.0 in 4 s; the claymore laid back over the right shoulder rests at pitch 110
and 125 (0.02 and 0.09 m from the shoulder's top), not at 140 and 155 (0.15, 0.21). **The
game's own return on this install is 5 a second, not 10** (fFatigueReturnBase read 5.00 at
0x00B37540; the CS wiki's default is 10 - a mod sets it): the bar falls 10 a second here
while held up, 5 with vanilla's settings. Open: the headset - the burn's figure, the
shoulder's radius, the feel of the weight.

## 7. Strength and fatigue in the weight (idea, 2026-10-09, nothing built)

The tester asked whether the character's Strength should enter the lag.
Kept here to decide later, after the headset has judged the swing-through
of section 6.

### How Oblivion uses weight and Strength (sources read 2026-10-09)

- **Weight, in combat, costs fatigue per attack**: `fatigue burned =
  (fFatigueAttackWeaponBase + fFatigueAttackWeaponMult x WeaponWeight) x
  fPowerAttackFatiguePenalty`, defaults 7.0 and 0.1, the penalty only on a
  power attack (5 by the UESP talk page) - an Iron Warhammer (30) costs
  10, a dagger (3) 7.3 (cs.uesp.net "Fatigue Game Settings"; UESP
  "Oblivion talk:Fatigue"). Not read in the engine's code.
- **Weight and stagger**: UESP "Oblivion:Combat": blunt weapons' weight
  "causes enemies to be staggered more often". The formula: could not
  verify.
- **Weight and attack speed**: none; the swing speed is the weapon's Speed
  value. Only from GameFAQs posts (2009), not from UESP - weaker evidence.
- **Strength**: carry weight = Strength x 5 (UESP "Oblivion:Strength");
  melee damage factor `0.75 + Strength x 0.005` (fDamageStrengthBase
  0.75, fDamageStrengthMult 0.5), Strength capped at 100 for it (UESP
  "Oblivion:Weapons", Damage Calculations; "Oblivion:The Complete Damage
  Formula"); max fatigue = Strength + Agility + Endurance + Willpower.
- **Fatigue in damage**: `x (Fatigue / MaxFatigue + 1) / 2` (The Complete
  Damage Formula) - a tired character hits for down to half.

So vanilla never sets Strength against a weapon's weight: Strength scales
damage and carry weight, weight scales the fatigue an attack costs.
Bringing Strength into the lag is a design choice, not a port.

### The idea

1. **Strength, mild**: divide the time constant by the game's own damage
   factor, normalised to Strength 50 - `tc x 1.0 / (0.75 + min(Str, 100)
   x 0.005)`: Strength 20 trails ~18 % more, 50 as now, 100 and above 20 %
   less. Weight and the slider stay the main lever; a level-up and
   Fortify Strength become something felt.
2. **Fatigue, the vanilla-shaped one**: scale the time constant by the
   inverse of the damage's fatigue term, `2 / (Fatigue / MaxFatigue + 1)`
   - a fresh character as now, an exhausted one up to twice the lag.
   Since vanilla takes `7 + 0.1 x weight` fatigue per swing, the weight
   then enters twice, by the game's own rules: heavy weapons tire faster
   and a tired arm drags them more.

### Against, and what is unknown

- Not vanilla behaviour; a Fortify Strength potion or a fatigue drain
  would change the swing's feel in the middle of a fight.
- OBVR reads the player's fatigue and its base since the teleport's cost
  (`game::ReadPlayerFatigue`, game/PlayerTeleport.h); Strength is not read
  yet - that path is not looked up and not verified.
- The hard end of idea 2 - at low fatigue the arms exhausted instead of the
  collapse, with the breath, the same for paralysis - is the tester's
  proposal of 2026-10-12 in docs/combat-comfort-spec.md section 2.
- Each needs its own INI switch (default to be decided) and pure, tested
  factors next to `WeaponLagFor` in vr/WeaponWeight.h; the log line per
  draw would carry the factors.

## 8. The off hand: strikes and grabs (built 2026-10-09/10, the ragdoll hold 2026-10-13, headset open)

The tester (2026-10-09): "man kann mit der off hand gegner packen oder
hauen wie in blade & sorcery" - and, asked whether to note it for later,
"ne starte jetzt". B&S lets the free hand punch and grab an enemy by a
limb or the neck (held, they stop fighting; thrown by the hand's speed).

Built as game/GrappleLogic.h (pure, grapple_test) on what section 6 and
weapon-collision-spec sections 10-12 already had - the capsules on the
fighters' bones (now each a named part), the shove's blow, the movement
setters, the parry's forced block:

- **The strike**: with a blade or a blunt weapon drawn, the off hand -
  grip loose, not on a two-hander's handle - driven at the shove's speeds
  into someone staggers and pushes them or knocks them down; a fist is a
  punch (the hand-to-hand hit sound, never a slap). `[Hands]
  OffHandStrikes`.
- **The grab**: the grip closed within 6 units of a fighter's capsule holds
  them by that part: staggered, their legs held (no step, no run), dragged
  by the hand - placed where it wants them each frame, SetPos's way, at up
  to 2 m/s; the character proxy's knockback did not move them - and, held by
  the head, the neck or the weapon arm - their blows at the player blocked
  whole. Let go fast they are thrown as a shove throws (staggered or
  knocked down along the hand); they break free after `GrabHoldSeconds`
  (4); a hand 40 units from where it wants them loses them. `[Hands]
  OffHandGrabs`, `GrabFatigue` (15).
- Decisions taken without the tester: only fighters can be grabbed (the
  lead by the hand keeps everyone else); the weapon arm is the right one
  (Oblivion's actors draw in the right hand); a shield on the player's
  left arm, or a two-hander's handle in the left grip, makes the off hand
  not free.
- Hand script `grapple.txt` PASS 2026-10-10 (controls-spec 4.34 has the
  numbers and what the first runs taught).
- Open: the headset (the drag's feel, the hold's length, the throw), creatures (their capsules are the
  bound's column; a wolf held by "the column" stops its blows not at all),
  and a held one's own hands - a bandit held by the torso still swings.
- **Open bug (the tester, 2026-10-14 evening): "linke faust trifft nicht:
  erwartet wäre wie ein slap nur ohne slap sound dafür schlag sound."** With
  the fists up, the left fist's punch lands nothing in his play. Expected: as
  the slap (the shove's blow by the hand's speed, section 6 of
  docs/hand-weapon-collision-spec.md) with the hand-to-hand hit sound
  instead of the slap's. What the code claims: the left fist swings by motion
  (`leftStrikeByMotion`, "Hands: a left-fist ... swing") and the off hand's
  strike above ("a fist is a punch") - his session's log has no left-fist
  line at all, which proves little: the swing lines share one budget with
  the right hand's (`g_handSwingLinesLeft`), spent early. Not built ("nichts
  coden!"). **The cause with a weapon drawn, read in his Windows session of
  2026-10-14 (21:39 to 21:49, the longsword in the right hand; the tester
  again: "linke faust sollte auch wie ein slap wirken nur ohne slap sound
  sondern mit schlag sound"):** "Shove: the left hand at 1.6 m/s towards
  192D0E68 refused - the weapon or the fists are readied; the grip is held".
  The off hand's strike refuses a held grip (`OffHandStrikeFor`,
  `hand.gripHeld` - written so a grab is not also a punch), and a fist on the
  Index squeezes the grip. The four "the left grip closed and held nobody -
  ... the nearest 7.4 / 10.7 / 8.6 / 13.7 units off the hand (the torso)"
  lines of the session are, by this, his punches: the grip closed, the grab
  found nobody within its 6 units, and nothing else ran. The open hand did
  land: "the left hand at 1.5 m/s towards 192D0E68 - light (the off hand, a
  weapon drawn)" twice. With the fists readied (the weapons away) one "a
  left power swing, 5.3 m/s at its fastest, 1.90 m long" is in the log and
  no strike line for it - that budget is still shared with the right hand's.
  Proposal: a grip closed on nobody within reach falls through to the strike
  when the hand is at the shove's speed towards someone - the punch with the
  hand-to-hand hit sound (as this section's fist already means), hard from
  ShoveHardSpeed as the slap's knockdown is; and the left swing's lines on a
  budget of their own.
  **Built 2026-10-15 (the tester: "ok dann geh an: linke faust"):**
  `OffHandStrikeFor` no longer refuses a held grip; it refuses a hand that
  holds someone (`g_grapple.actor`) and a grip closing this frame - the
  grab's frame, since the grab's step runs after the shove's and takes the
  closing edge (grapple_test, three flows). The punch is the shove's blow by
  the hand's speed with the hand-to-hand hit sound, as before for a fist
  with the grip loose. The refusal line names "the hand holds someone" now.
  The left-fist swing lines have a budget of 40 of their own. Harness
  `left-punch.txt` PASS (Linux): a longsword drawn, the left fist with the
  grip squeezed punching ahead at a bandit - "the left hand at 6.7 m/s
  towards ... - hard (the off hand's punch, a weapon drawn)", a second at
  3.4 m/s, no refusal. **With the fists up** the left fist strikes by motion
  as the code claimed: harness `left-fist.txt` PASS, a frozen beggar met on
  every lap - "the blade met ... swing 2147483650, held to the swing's end",
  "struck ... light attack ... health 26.0 -> 25.0", then two power swings
  (25.0 -> 22.4 -> 19.8); the hand's reach 57 units past the fist. What the
  tester saw with the fists up is not reproduced by that; the headset decides
  (the lines now have their own budget, so his log will say).

### The grab and the throw as a ragdoll (the tester, 2026-10-12; built 2026-10-13, harness PASS, headset open)

The tester: "gegner greifen und werfen, wie könnten wir das verbessern also
mit ragdoll irgendwie" - and the next day: "npc ragdoll halten und mit kraft
wegschubsen oder sogar auf dem boden zu schubsen mit linker hand während des
kampfes wie in der spec".

**Built [R]:** `[Hands] GrabRagdoll` (1, "Grab takes them down") and
`GrabThrowStrength` (1.5, "Grab throw strength"); game/GrappleLogic.h (the
decisions, grapple_test), game/RagdollHold.h (the Havok side), game/Shove.h
(the knockdown, the knock state, the time stamp):

1. A fighter grabbed by the torso, an arm or a leg goes down into their
   ragdoll: the engine's own knockdown (`0x00654420`) with the source at the
   hand and force 0 - the impulse (`0x005364B0`) is the force times the
   direction from the source (times a factor by the body's collision layer
   from the table at 0x00B11760, times 0.25 x the motion's damping at +0xC8
   + 0.75 - so it hardly depends on the mass), so nothing pushes them; the
   knock state, the
   get-up, the AI's recovery and the crime stay the engine's. A knockdown
   the engine refuses (no high process, already down) or takes the
   ragdoll-less way (its vtable +0x278 says no ragdoll: a push through the
   proxy, the knock state left 0) falls back to the standing hold.
2. The part under the hand is held by OBVR, not by the engine's grab spring:
   the bhkRigidBody on the part's bone nearest the hand - skeleton.nif
   carries a bhkBlendCollisionObject on each of the spine (60, 50 and 40
   kg), the head (30), the upper arms (40), the forearms (30), the hands
   (15), the thighs (45), the calves (30) and the feet (15); the collision
   object getter `0x0047FAC0` walks the RTTI parents, so a blend object
   passes as the hands' own bodies do (HandBodies' HavokBodyOfNode) - is
   activated and given the velocity that closes the gap to the hand each
   frame (GrabPhysics.h's drive, 0.8 of the gap a frame, capped at 15 m/s;
   the body's motion type read as 4 once the ragdoll is on). The rest of the
   body hangs from it through the ragdoll's joints, its weight Havok's. Why
   not the engine's spring: it wants the engine's grab ray and its filter
   (unread), and the drive was already there for held objects.
3. Let go: the held part sent off with the hand's speed - in the air, not
   across the ground - times GrabThrowStrength when the hand moves at
   ShoveSpeed; a slow hand, or the strength 0, drops them (the body's own
   motion left alone). A hand going along the ground is the shove along the
   ground the tester asked for; one going up throws them up.
4. While held, the engine's get-up is kept off: the process's time stamp
   (xOBSE LowProcess::curHour, +0x14, and the packed date, +0x18; set by the
   process vtable's +0x20, `0x00643640`, which the knockdown itself calls at
   `0x00654548`) is set anew every 0.5 s while they lie (knock state 1). The
   knock state's step (`0x006545E0`, the state-1 case at `0x00654913`)
   stands them up 2.4 s after the stamp, or 5.4 s with "Bip01 NonAccum"
   lifted more than 16 units off the proxy (`0x005F0270`; the constants at
   `0x00A72D40` and `0x00A72D38` are hours over the timescale, so real
   seconds whatever the timescale). Let go, the clock runs from the last
   stamp. The hold also ends when the get-up begins anyway (state 5 or 6:
   "stood up"), after GrabHoldSeconds ("broke free"), with the body 40
   units from the hand ("lost"), or with them dead, far, or the body
   unreadable ("gone").
5. One already lying knocked down (state 1 to 4) is taken as they lie - no
   knockdown, the same hold; one getting up (5, 6) is refused. The head and
   the neck keep the standing hold - B&S's choke; so do the column (a
   creature read without the bones) and a part whose bones carry no Havok
   body (said once: "no ragdoll hold on ... - no Havok body on the part's
   bones; the standing hold instead").

**The knock state, read [R]** (`0x006545E0`; its switch at `0x00654C58`):
0 standing; 2 the lead-in the knockdown sets (the ragdoll on through
`0x0088D070`); 1 down - from 2 once the ragdoll's blend value
(`0x0088FA30`) has reached 0; 3 and 4 the fatigue collapse's and the
ragdoll-less knock's; 5 the get-up animation (found through `0x00521450`,
played by `0x00475300`, the ragdoll still on); 6 its end, then 0. With
force 0 the lead-in lasted one frame.

**Harness `grapple-ragdoll.txt`** (grapple.txt's scene and grid with
GrabRagdoll=1 and GrabHoldSeconds 6), PASS 2026-10-13 on the first run,
two holds in three sweeps: "takes 1BCE9E08 by the left arm (-1.2 units off
it) - knocked down into its ragdoll by the engine's knockdown, no push
(knock state 2); the Bip01 L Hand bone's body ... is held by the hand", the
knock state 2 -> 1 after 0.01 s, "holding ... as a ragdoll 0.2 s - the body
3.2 units from the hand, driven at 3.2 m/s (motion type 4)", 2.3 to 3.6
units through the 0.8 s of the hold (the second hold, by the torso through
Spine1's body: 1.0 to 4.9 units), the engine's clock stamped anew once, the
drive never refused, "thrown - the hand at 27.1 m/s ... the held part sent
off at 2844 units/s (40.6 m/s, the hand's 27.1 times 1.50)". The bandit
stood up between the holds and fought on (the second take was a knockdown
from state 0 again). Not seen in the run: a grip on the lying body (the
grid runs at head height), a hold ended by the engine's get-up, the
standing-hold fallback.

**The weight (the tester, 2026-10-13, the hold seen in the headset: "ja
geht geben wir den gehaltenen gegnern noch gewicht"; built the same day,
`[Hands] GrabWeight` 1, "Grab weight (x)", harness rerun open).** The
drive above gave the held body the velocity that closes the gap whatever
it took - an unlimited force, so a forearm snapped to the hand and the
whole ragdoll came along as if it weighed nothing. Now (GrappleLogic.h
`PullLimitedVelocity`, grapple_test; RagdollHold's drive) the change of the
body's velocity is cut to a budget over the body's mass inverse, once per
Havok step: the budget is a force in Havok's terms, from the player's
Strength (10 to 200 counted) - at 100 it lifts `kGrabLiftMassAtHundred`
(300) of the ragdoll's mass units against the world's gravity, at 50 half
that - divided by GrabWeight (2: twice as heavy; 0: no limit, the hold as
first built). Havok's joints carry the rest of the body on the held part
and its gravity pulls it down, so a heavy chain lags behind a fast hand and
sags under a still one, and one the budget cannot lift stays on the ground
and is dragged. The throw is unchanged (the velocity set outright).
- **Read [R]:** the mass inverse is at motion+0xC0 - hkMotion::setMassInv
  (`0x008EAC60`, the box motion's vtable `0x00A9AF38` at +0x24; setMass at
  +0x20, `0x008EAA90`, divides 1 by the mass and calls it) stores it there
  and again at +0xFC, the w of the inertia inverse at +0xF0 that the box and
  sphere motions of 0x100 bytes carry (the keyframed motion is 0xF0 bytes).
  The body's velocity reads at motion+0xD0 (GrabPhysics.h). The world's
  gravity: hkRigidBody+0x08 is the hkWorld, its gravity at +0x20 - the
  constructor `0x0089A230` copies the cinfo's (+0x10) there at `0x0089A472`;
  the cinfo's default constructor writes Havok's -9.8 (the only -9.8 in the
  image, the immediate at `0x008A9529`). Whether the engine sets another is
  read from the world at the take and logged.
- **Once per Havok step:** the velocity set is read back the next frame;
  exactly the one set means Havok has not stepped in between, and the same
  velocity goes again with no new change (the budget's time is the time
  since the last change given, so a 60 Hz world under a 90 Hz hold gets the
  budget per step, not per frame). A set of nothing is never "still the
  same": a body at rest on the ground reads back nothing after a step too,
  and a hand moving off would never get it going.
- **The masses and the pull are logged at the take** ("Grapple: the pull on
  <ref> - the <bone> body's mass n, the ragdoll n in n bodies (Havok's
  masses), gravity n; Strength n, GrabWeight xn: a budget of n, which lifts
  n (0: no limit)") and the state line carries "pulled at n m/s^2 (n of n
  changes cut to the budget)". The masses in Havok's terms are skeleton.nif's
  (a forearm 30, the body about 530) unless the engine scales them - the
  first run with the weight will say; `kGrabLiftMassAtHundred` is to be set
  on what it says and on the headset (a Strength-50 character should drag a
  bandit along the ground and lift an arm, not the torso).
- **Harness:** `grapple-ragdoll.txt` sets GrabWeight=1 and expects the pull
  line and the budget's count: PASS 2026-10-13 15:54 (the run of 13:18
  ended before the script started, the tester's own session; the one of
  15:50 quit to the main menu at the drawn mark - a Pause menu, a Message
  box, the game gone - with nobody at the keyboard known, not seen again):
  "the pull on 1D274CF0 - the Bip01 R UpperArm body's mass 40.0, the
  ragdoll 530 in 16 bodies (Havok's masses), gravity 73.6; Strength 25,
  GrabWeight x1.00: a budget of 5518, which lifts 75" - so the masses are
  skeleton.nif's as they are, the world's gravity is 73.6 Havok units a
  second squared (not 9.8: the engine sets its own), and the harness
  character's Strength of 25 lifts an arm (40) but not the body (530);
  "holding ... right arm as a ragdoll 0.2 s - the body 19.7 units from the
  hand, driven at 0.5 m/s, pulled at 13.8 m/s^2 (19 of 19 changes cut to the
  budget)", 20 to 31 units behind through 0.8 s (every change cut: the
  hand's 0.4 m/s steps outrun a 13.8 m/s^2 pull with the ragdoll hanging
  on; the hold was never lost at 40), "thrown - the hand at 27.8 m/s ...
  sent off at 2915 units/s (41.7 m/s)", a second hold by the left arm. The
  harness's Strength 25 is the low end; at 50 the pull doubles.

**Open:** the headset - how the drag feels with the weight (a Strength-25
hand sees the arm 0.3 to 0.45 m behind it; too heavy, GrabWeight under 1,
or `kGrabLiftMassAtHundred` up from 300), whether the
thrown body flies or slides, and how long a hold should last
(GrabHoldSeconds counts for the ragdoll hold too; the stamp keeps the
engine from standing them up before it). The AI while held (its combat
state) is the engine's knockdown's own and was not read. The ragdoll hold
on creatures: their parts are the bound's column, so the standing hold.

**Open bug (the tester, 2026-10-14 night): "grab mit linker hand sollte
grabben und werfen lassen. bisher sinken die nur in sich zusammen."** His
Windows session of 21:39 to 21:49 (Strength 25, GrabWeight 1 - "a budget of
5518, which lifts 75" of the ragdoll's 530): five ragdoll holds of one
bandit, four by the torso and one by the left arm, each knocked down into
the ragdoll as designed. Then the held body fell behind the hand - 21 units
at 0.2 s, 39 at 0.4, 40 at 0.6, every velocity change cut to the budget
("18 of 18", "38 of 38", "56 of 56") - and four of the five holds ended
"lost - the hand too far from them" after 0.3 to 0.6 s (`kGrappleLoseUnits`
40); the fifth was "thrown" with the hand at 2.0 m/s, the part sent off at
3.1 m/s. So what he sees is the knockdown and a body that cannot follow the
hand and is dropped within half a second: no drag, no throw. Four more grips
closed 7.4 to 13.7 units from the torso's capsule held nobody
(`kGrappleReachUnits` 6). And the loss line's "the hand at 0.0 m/s" is no
measurement: the ragdoll hold's loss sets no speed (GrappleLogic.h, the
`Lost` branch; only the standing hold's release reads the hand across the
ground), so a hand pulling away fast loses them where it should throw them.
Ways on; B, C and D built 2026-10-15 (weapon-collision-spec 19: the loss as
a throw at the shove's speed, `kGrabLiftMassAtHundred` 1200, the reach 14;
grapple-ragdoll PASS, the torso still 25 to 38 units behind the hand), A
open:
- A. No loss while the ragdoll is held: the body hangs at the budget's end
  and is dragged, as B&S drags a heavy body, and only the grip opening ends
  the hold - a fast hand then throws, a slow one drops.
- B. The loss as a let-go at the hand's speed over the last 0.1 s, so a hand
  that outruns the body throws it.
- C. A stronger pull: GrabWeight under 1 by default, or
  `kGrabLiftMassAtHundred` above 300, so a Strength-25 hand drags the torso
  instead of losing it.
- D. The take's reach widened from 6 units (his four misses at 7 to 14).

Controls-spec 4.34 has the controls and the log lines.

## 9. Stabs that stick, and hit locations (the tester, 2026-10-14; nothing built)

Nadi, 2026-10-27: the sword is to go into the body as in Blade & Sorcery,
"nur wenn ohne Rüstung" - into flesh, not into armour. This rework decides
when a blade may be inside a person's capsule on purpose (a cut or a stab
into a bare part) and when not (armour, a blade only resting on them);
the shallow band that today leaves a resting blade up to 3 units inside a
capsule without pushing it out (mesh-fitted-capsules-spec 8, open) is to
be settled with it. Which parts are bare needs what each body part wears
(the fit reads the shapes per bone group already, game/FittedCapsules.cpp).
Whether Blade & Sorcery itself blocks penetration by armour is not verified.

The tester, after the evening's play: "stabs in den gegner wo die waffe im
gegner hängen bleibt wären cool wie in blade and sorcery. treffer
locations/locational dmg wie in B&S." Noted for later ("nichts coden!"):

- **A stab that sticks.** In B&S a thrust into a body penetrates and the
  blade stays in until pulled out, the body dragged with it. Here the blade
  already meets the living (weapon-collision-spec section 10: the capsules on
  their bones, the hit-stop, a swing passes through) and a thrust is a power
  attack of its own (`PowerThrustMetres`). The piece missing: a thrust that
  enters a capsule along the blade's own axis holds the blade there - the
  weapon pinned to the body's part (the ragdoll hold's drive, section 8, on
  the blade instead of the hand) until the hand pulls back along the axis -
  and the fighter staggered or held while it is in. What the engine does
  with a target that is "held" by a blade (its animation, its AI) is
  unknown; the ragdoll hold's findings (knock states, the standing clock)
  are the nearest reading. Unmeasured; a design to write before any code.
- **Hit locations.** B&S scales damage by the part struck (head, neck, limbs)
  and dismembers. Oblivion has no locational damage: `AttackHandling`
  (0x005FEBF0) takes a target, not a point. OBVR knows the part the blade
  met (`BodyCapsulesFromBones`, each capsule a named bone), so a factor by
  part could scale the damage before the hit is handed over - the way the
  power attack's direction is handed over (next-up 2, the attack group) -
  or the hit's damage could be reread and adjusted after. Not designed yet.
  Dismemberment: dismemberment-spec.md (designed 2026-10-25, not built).

## 10. The target: Blade & Sorcery's combat, rebuilt (the tester, 2026-10-15)

"neues zielbild: nachbau vom B&S combat in OBVR." The list of what B&S does
and where OBVR stands is weapon-collision-spec section 18 (the bodies stop
the blade and the fist: built; the knockback of the player's hits: off by
default, scalable; penetration, hit locations, the own body: open;
dismemberment: designed, dismemberment-spec.md). What B&S does, read on its SDK wiki, is in
weapon-collision-spec 17.1. The open items of sections 8 and 9 here are
steps towards it; each gets its cost stated when proposed.

## 11. The kick (the tester, 2026-10-25; nothing built)

"ein kick vom player wie in B&S auf button druck (whl rechts B und Esc menü
wandert zu Links B + long press) ein fuß schiesst dann voraus wie in B&S
und staggert gegner". The buttons are controls-spec "Open wish (the tester,
2026-10-25)": the right B kicks, the pause menu moves to the left B held.

**B&S has kicks** - a mod of 2024 makes "your kicks" stronger ("Insane
kicks", mod.io/g/blade-and-sorcery/m/insane-kicks, its comments: knock
downs "even kicking shields"), and the Steam forum speaks of enemies that
do not react much "to parries or pushes or kicks"
(steamcommunity.com/app/629730/discussions/0/598538723409365897). How B&S
starts a kick (a button, a tracked foot) and what it does to a body were
not read: I could not verify them, so the design below is OBVR's own.

**Decided (the tester, 2026-10-25)**, to the four questions of the first
draft: "1. ja 2. ja soviel wie in h2h angriff 3. ja und auch während eines
sprungs, dmg dann schwerer h2h angriff 4. wir brauchen wohl einen kick
sound entweder aus dem spiel oder online. also add to spec: mehrere gegner
gleichzeitig treffen (cleave) soll möglich sein."

1. A kick counts as an attack (a guard calls it a crime, a friend fights
   back as for a blow).
2. It does the damage of a hand-to-hand attack.
3. A kick while running or in a jump knocks down and does the damage of a
   hand-to-hand power attack.
4. A kick sound of its own, from the game or from a free source.
5. One kick can hit several people at once (cleave).

### Proposed

1. **The press.** The right B in the world (no menu up, not in dialogue,
   not on a horse, not swimming, not knocked down): one kick, none again
   for `[Hands] KickCooldownSeconds` (proposed 0.8 s). Proposed too: no
   kick while exhausted (`game/ExhaustionLogic.h`).
2. **The foot shoots ahead.** The weapon side's leg (the right one,
   left-handed the left) is driven by the body's leg IK (player-body-spec
   "The legs": two bones, the knee to the pole, the foot keeping its angle)
   to a target in front of the hip: out along the body's facing (the
   head's yaw, level) by the leg's length (the thigh 33.67 and the calf
   27.33 units, so about 0.85 m from the hip) at about hip height, in
   about 0.12 s, held 0.08 s, back in 0.2 s (all proposed, to tune in the
   headset). Without the body (`[Body] Visible=0`) or with `IKLegs=0` the
   kick still strikes; nothing is drawn.
3. **The hit.** While the foot is out, its path (a sphere of about 6 units
   round the foot bone, swept from the frame before) is tested against the
   living's capsules the hands collect (`CollectBladeBodies`, the fitted
   ones with `FittedBodyCapsules=1`) - `CapsulesTouched`, as the fist's
   hit by touch does. **Every** living person the foot meets while it is
   out is kicked, each once per kick (decision 5, the cleave): the kick
   keeps the list of those it has met, as the blade's ledger of the passed
   does (`BladeContactLogic.h`).
4. **The stagger, standing.** Each kicked person gets the shove's light
   kind (`game::ShoveActor`, ShoveKind::Light, section 8): the engine's
   stagger (0x005F4FD0, action 8) and the knockback through their
   character proxy away from the foot (0x008907A0) - the stagger alone
   was not seen in the headset (2026-09-29), the push makes it show. Of
   the shove's rules (`ShoveSettings`) the kick takes the fatigue it costs
   the player (15 for the light kind; the kick's own setting) and the
   disposition (-5); and unlike the light shove it is **always told as a
   hit** (decision 1): the victim's reaction to a blow (0x005FE380, vtable
   +0x3A8, `ReactAsToAHit` in Shove.cpp - a crime with witnesses, or
   fighting back), as the shove's `countsAsHit` does for its hard kind.
5. **The knockdown, running or jumping** (decision 3). A kick while the
   player runs (OBVR presses the run itself, so it knows) or is in a jump
   is the shove's hard kind for each kicked person (`ShoveKind::Hard`:
   0x00654420 through the process, the ragdoll away from the foot,
   `hardForce`). "In a jump" is **not read yet**: OBVR presses the jump
   (the right stick flicked up) but does not read when the player lands -
   the character controller's state is the place to look (not read); until
   then "within the jump's time after OBVR's jump press" would stand in,
   to be measured in the harness.
6. **The damage** (decision 2). Hand to hand in Oblivion (UESP,
   en.uesp.net/wiki/Oblivion:Hand_to_Hand, read 2026-10-25): "Hand to Hand
   attacks damage both the opponent's Health and Fatigue";
   `Health_Damage = 1 + 10.5 * (Strength/100) * (HandToHandSkill/100)`
   (11 at most, before Luck and Fatigue's modifiers - "The Complete Damage
   Formula" there), `Fatigue_Damage = 1 + 0.5 * Health_Damage`; a power
   attack 2.5 times (the Novice's, all directions) - the running or
   jumping kick's (decision 3). Two ways:
   - **A. The engine's own hit** (`AttackHandling` 0x005FEBF0, which takes
     an explicit target - OBVR's strike by motion uses it): armour,
     resistances, the skill's experience, the hit's sound and blood come
     with it. But it reckons the damage from what the player holds: with a
     sword drawn it would be the sword's. **Unknown**: whether the engine
     can be asked for the fist's blow while a weapon is drawn (the
     journal's fist number is reckoned somewhere - not read).
   - **B. OBVR reckons the damage** by the formula above (Strength and the
     skill read as the fatigue is, GetActorValue_F vtable +0x288) and
     takes it off the victim's health and fatigue itself; the hit's
     reaction (decision 1) as in 4. The target's armour, its resistances
     and the skill's experience (0.6 a hit on the living, UESP) are then
     OBVR's to add, each a read or a call not looked up yet.
   - **Recommended: A** where the player is unarmed (the engine's blow
     whole, no formula of OBVR's), and finding the fist's damage path for
     the armed case before choosing B; B is the fallback with its gaps
     named.
7. **What it feels and sounds like.** A pulse on both controllers when the
   foot meets someone, and a kick sound (decision 4). The shove plays none
   of its own (only PiiiP's slap noise for a slap). In the game's
   `Oblivion - Sounds.bsa` (listed 2026-10-25) nothing is named "kick";
   the candidates are the fist's own hit, `sound/fx/wpn/weaponhit/hand/
   wpn_hit_hand_*` (what a hand-to-hand blow plays - the obvious one: it is
   the game's, every install has it, a sound mod replaces it with the
   rest), the heavy armour hit `sound/fx/phy/combat/armorhit/heavy/
   phy_armorhit_h_*`, and the blunt-on-flesh `wpn_hit_bluntflesh_*`; with
   way A of 6 the engine plays its own hit sound anyway. A sound from
   outside the game needs a licence that lets OBVR ship it (CC0 or the
   like) - none looked up. **Recommended: the game's hand hit first**, an
   outside sound only if it does not read as a kick in the headset. The
   foot through the air alone gives nothing.
8. **Open, not asked:** kicking clutter and ragdolls (the blade's
   WorldPush kick on Havok bodies); damage by the boots' weight.

### Cost

| Piece | Cost | Why |
|---|---|---|
| The button and its gate | small | `vr::PlanHandControls` turns presses into actions; the cooldown and the gate (menu, dialogue, horse, swimming, exhaustion) are pure flows for a new kick_test |
| The hit, several at once | small | `CollectBladeBodies` and `CapsulesTouched` exist; new is the foot's sphere, its sweep and the list of those met |
| The stagger and the knockdown | small | `ShoveActor` (light and hard) exists, harness-proven for the shove; the reaction to a blow (`ReactAsToAHit`) too |
| Running or in a jump | small to medium | the run is OBVR's own press; the jump's landing is not read (the character controller's state, unread) |
| The damage | medium | way A reuses the engine's hit but its fist-while-armed path is unread; way B is a formula plus armour, resistances and experience to find |
| The sound | small | the game's hand hit played by `PlaySoundFormAt` (game/GameSound.h, as the slap's noise) once its sound form is found; an outside sound adds the licence check and an asset |
| The foot ahead | medium | the leg IK exists for standing feet on the floor; a target at hip height ahead and its timing are new, and how the knee bends there (the pole) is unknown until seen; the body view's picture (`OBVR-BodyView-<mark>.bmp`) shows it in the harness |
| Tests | small to medium | kick_test (the press, the cooldown, the gate, the foot's path over time, each person once per kick, several in one, light or hard by running and jumping, the damage formula of way B); harness: two bandits side by side in front, the right B pressed, "Kick: ... staggered" for both and their action 8 read, their health down by the hand-to-hand damage, a running kick knocking one down, the body view's picture at the mark; the headset for the feel |

**Overall medium**, the foot's picture and the damage its largest parts;
without the picture (the hit, the stagger, the reaction) small. Recommended
order: the button, the hit on several and the stagger with the reaction
first (proves the effect in the harness), then the damage (way A unarmed,
the armed path read), the running and jumping knockdown, the sound, the
foot's pose last - the feel of the timing needs the headset either way.

## 12. Ideas from Combat Additions (Nexus 49939), sorted for VR (2026-10-27; nothing built)

Nadi, 2026-10-27: "welche features von https://www.nexusmods.com/oblivion/mods/49939 könnte man
in obvr nutzen oder umsetzen, welche nicht?" - and "schreibe das in eine spec".

**The mod [read].** "Combat Additions" by Legion, version 2.4 (11 July 2023), xOBSE scripts,
"inspired by games such as Dark Souls, Sekiro, Nioh, and various Skyrim combat mods", every
feature switchable in its INI, "Works best if playing in third person". Read: the Nexus page
through Exa (about, permissions, the changelog 1.9 to 2.4) and the description Nadi pasted.
Not read: its manual (an in-game book) and its scripts. Not installed on either install
(checked 2026-10-27: only "Combat Additions - only audio files for parry sound" in the A
Painted World folder; neither Plugins.txt lists it).

**Permissions [read].** Modification and asset use only with the author's permission, no
re-upload. So nothing of its scripts, animations or meshes goes into OBVR; a mechanic rebuilt
in OBVR's own way is fair, with a credit as for B&S.

### 12.1 Not for VR

| Feature | Why not |
| --- | --- |
| Dodging (dash, roll, backstep, i-frames, perfect and timed dodge) | moves the camera in jumps - sickness in a headset; the author: with Enhanced Camera "dodging in first person causes the camera to go crazy", and OBVR drives the camera at least as deeply; in VR the body dodges |
| Lock-on | turns the camera onto the target |
| Finishers | third-person animations take over the body and the camera; the author: "No first-person animations" |
| Attack chains, attack cancel, the player's attack commitment, its attack animations | built on attack animations; OBVR's swings are physical, so chained and cancelled by the hand anyway |
| Marksman: charged and power shots, Steady Hand, denock | OBVR's bow is drawn by hand (vr/Archery.h) |
| Consumables with a healing animation, combat rating, the light spell's colour, Purge Cell Buffers, equipment remover and requirements | not about VR; whoever wants them can use the mod for those alone |
| The third-person camera fix | OBVR is first person |

### 12.2 OBVR has them already, physically

| The mod's feature | OBVR's |
| --- | --- |
| Hitstop | the hit-stop at the blade's contact (weapon-collision-spec) |
| Attack cleaving | a strike at the touch, every body met (0533da7) |
| Dark Souls stamina | fatigue per swing by weight, the two-hander's costs, exhaustion (section 6, combat-comfort-spec) |
| Enemy AI: reaction, attack spam | [Hands] EnemyAttackSpeed and EnemyAttackFrequency, the combat ring's turns |
| Chase idles (enemies running in swinging) | the ring's slow approach |
| Enemy health bars | [Hands] EnemyHealthBars |
| Parry by key and timing | the parry by the blade's real contact and angle (BlockByAngle), the torch's parry |

### 12.3 Worth rebuilding

| Idea | In OBVR | Cost | Why, and what is unknown |
| --- | --- | --- | --- |
| **Parry/counter** ("staggers enemies, allows the next attack to deal more damage") | a real parry staggers the attacker; the next blow within about 2 s hits harder | small | the parry's detection and the stagger exist; new: the window and the bonus. Unknown: the bonus as a power strike (exists) or a damage factor of OBVR's own (the strike's damage path unread) |
| **Poise**, count-based with a cooldown | an enemy reels only after N hits in a short time, not after each; later weighted by the blow (weight times speed) | small counted, medium weighted | today every hit staggers, which B&S does not do; the hits' times and speeds are known; the feel needs the headset |
| **Backstab** (bonus against those who cannot see you) | a blow from behind, or on someone unaware, hits harder | small to medium | "behind" from the target's heading and the blow's direction, both readable. Vanilla has sneak attacks: whether they still apply through OBVR's strikes is unverified - a harness run settles it, and if they do it is nearly free |
| **Bashing** (a shield bash breaks a block and a power attack) | the shield driven fast into someone: they reel, their power attack breaks off | medium | shield contact and the shove exist; unknown: where the engine ends an attack under way |
| **Weapon clash** (both attack at once) | blade meets blade while both swing: sparks, both recoil | medium | blade against the foe's weapon is detected (the parry); new: "both attacking", the recoil, the sparks; unknown: how the engine makes its block sparks |
| **Power attack effects by weapon type** (a blade bleeds) | a blade's power strike bleeds, a blunt one knocks down more easily | small each | the power strike is known (StrikeAtContact); unverified: a damage-over-time effect without a plugin of OBVR's own |
| Vanilla knockback on hits removed | only if it still shows in OBVR | cheap | nothing seen of it so far |

### 12.4 Running the mod beside OBVR

Not advised for the whole mod: its first-person dodge (see 12.1), its poise script that
suppresses "vanilla/random stagger" against OBVR's stagger and knockdown, and its enemy AI
(dodges, counters) against the combat ring. Only its enemy-side features with the player's
switched off might coexist - unverified; a harness run with it would tell, and it hardly pays
once poise and the counter are OBVR's own.

### 12.5 Recommended order

1. Parry/counter - the cheapest felt gain, and it rewards what VR does well: a real parry.
2. Poise, counted first - ends the reel on every hit.
3. Backstab - after measuring whether the vanilla sneak attack already reaches OBVR's strikes.
Then clash, bashing and the power effects. Not dodge, lock-on or finishers - a VR rule, not a
matter of cost. Waiting for Nadi's go.

Sources: https://www.nexusmods.com/oblivion/mods/49939 (read through Exa, 2026-10-27; the
description as Nadi pasted it), https://www.nexusmods.com/oblivion/mods/47908 (names it
"Legion's Melee Combat Additions").

## 13. Strikes train the weapon skill (built 2026-10-27, headset open)

Nadi via the Walkers session, 2026-10-27: "VR-Schläge sollen Waffenskill trainieren, mit einer
Ausnahmeliste, die ein anderes Plugin setzen kann". Read in Oblivion.exe:
- **The engine splits the skill use in two.** AttackHandling `0x005FEBF0`, on a landed hit, counts
  the hit and notes the attack's skill on the attacker (`0x005FF53D`: vtable +0x214 reads the count,
  +0x210 writes it +1; `0x005FF55C`: +0x218 the skill; for the player `+0x648` and `+0x64C`). The
  attack's update `0x005FCAB0` later credits them (`0x005FD7E0..0x005FD85B`): for each counted
  hit, unless the skill is `0x48`, UseSkill (vtable +0x39C, `0x00668C30`, thiscall skill, use 0,
  0.0 times), then the count back to 0.
- **A strike by motion skipped the second half.** game/MeleeHits.cpp hands the target to
  AttackHandling straight; the update never credited, so the count only grew and no weapon skill
  was trained. The armour skill of the one struck was (AttackHandling does that itself).
- **Built.** `[Hands] StrikesTrainSkills` (default on; Hands: melee, "Strikes train skills";
  game/SkillCreditLogic.h, game/SkillCredit.cpp, skill_credit_test): right after the
  AttackHandling call the strike credits the count as the update does - UseSkill through the
  player's table (another plugin hooked on the slot sees it, Oblivion Walkers is) - and clears it.
  At most 8 uses per strike. Each of the four slots must hold the engine's function or one outside
  the game's code - another plugin's hook, called through (Oblivion Walkers holds +0x210, where it
  does not count a hit on a walker, and +0x39C); some other function inside the game is refused
  and nothing is done ("Skill: the player's hit count or skill is not where it was read").
- **The exception list.** Another plugin sends OBVR an xOBSE message: receiver "OBVR", type
  `0x58454B53` (the bytes 'S' 'K' 'E' 'X'), data the running game's base form IDs as UInt32s,
  dataLen in bytes; each message replaces the list, an empty one clears it (at most 256 ids). OBVR
  listens to every plugin from xOBSE's PostLoad on. A strike on a listed base trains nothing and
  clears the count all the same. Logged "Skill exceptions: N base(s) from <sender>" and per
  strike (the first twelve) "Skill: a strike on base X - skill N, k hit(s) counted,
  trained|excepted". Oblivion Walkers sends its walker bases with `[Zombies] NoSkillGain=1`.
- Not built: an INI list for hand-written exceptions (asked as optional).
- `skill-train.txt` 2026-10-27 PASS: the beggar struck four times with an iron longsword, each
  "skill 14, 1 hit(s) counted, trained" (through Walkers' hooks on +0x210 and +0x39C, which its log
  shows passing the use on); `skill-train-walker.txt` PASS: a walker struck, its hit not counted by
  Walkers' hook, nothing trained; "Skill exceptions: 64 base(s) from OblivionWalkers" in both.
  `strike-at-touch.txt` PASS after it.
