# Hands and weapon as physics bodies (HIGGS-style collision)

Status: **built 2026-09-28 (`[Hands] BodyCollision=1`, game/HandBodies.h),
harness-tested, not yet tested in the headset.** See "Built" below for what
differs from this plan and what the harness showed. Addresses are
Oblivion.exe 1.2.0.416, read with `dumpbin /disasm` from
`C:\Steam\steamapps\common\Oblivion\Oblivion.exe`. Each finding is marked
**Read** (seen in the disassembly), **Derived** (concluded from what was
read), or **Unverified** (still to be shown in the game).

This spec replaces the "Not found" list in
[holding-objects-spec.md](holding-objects-spec.md) ("Hands, weapon and held
objects that push the world — research"). Every address that list was
missing has now been found.

## Goal

Each hand, and the drawn melee weapon, becomes a **keyframed rigid body** in
the Havok world. OBVR drives the body to the tracked pose every frame, and
Havok's solver handles the contacts. This replaces the ray-based
`game::WorldPush` for the hands and the weapon. The held object keeps its own
drive, which is already built.

What this gives over `WorldPush`:

- The whole shape collides. Today it is four thin rays, and a thin object
  between them is missed.
- The contact has a real normal, friction and angular response. A plate is
  lifted or pushed sideways, a cup tips over.
- A contact holds while the hand is still. `WorldPush` only acts on
  movement above 0.5 units a frame.
- Constrained bodies (hanging signs, chains) are handled by the solver
  instead of by one velocity kick.

What it does **not** give, same as in HIGGS: the hand does not stop at walls
or statics. A keyframed body has infinite mass, and against a fixed body
nothing gets resolved. The HIGGS author explains this and chose it on
purpose ("there is no interaction between 'fixed' objects like walls and
your hands", reddit.com/r/skyrimvr/comments/100k7jp).

## Built (2026-09-28)

Code: `game/HandBodies.h/.cpp` (the engine calls), `game/HandBodyLogic.h`
(the decisions, pure, `hand_body_test`), wired in CameraHook next to
`WorldPush`. Settings: `[Hands] BodyCollision` (default 1, the row "Hand
collision") and `PhysicsRate` (default 0, the row "Physics rate").
Scenario: `tools/hand-scripts/hand-bodies.txt`.

What was built, and where it differs from the design below:

- **Three bodies:** the right hand, the left hand, and the drawn melee
  weapon. While a blade is drawn, the right hand's body is out of the world
  and the blade's is in. `WorldPush`'s rays still run, but only for a
  pusher that has no body in the world this frame.
- **Capsules, not a box for the hands.** One constructor
  (`bhkCapsuleShape`, 0x00563BB0) serves hands and weapon.
  - The hand: from 4 units behind the grip to 9 ahead, 2.5 round.
  - The blade: from the grip to the tip, 1.5 round.
  - **Read** for the capsule: its data (default 0x00564030) has +0x04 the
    radius, +0x10 and +0x20 the ends. The constructor builds the hkShape at
    once (0x008B6B90), at shape+8.
- **Made as the NIF loader makes a body:**
  - the factory 0x008A41F0;
  - the creation block from vtable +0x74, with the cinfo at +0x20: filter,
    shape, pose, mass 1, motion type 6;
  - vtable +0x70.
  - Every body came out with the keyframed motion vtable 0x00A9AE10
    (harness log).
- **In and out of the world:** 0x0089F470 to enter or move; vtable +0x60
  to leave.
  - A body is in the world only while Full VR runs in first person with no
    menu. A loading screen is a menu, so the bodies are out before a load
    tears the world down.
  - An add the world defers is not asked for again for 5 frames.
  - A body whose world no longer points back at itself is abandoned, not
    touched.
- **References:** OBVR takes one on each wrapper (+4) and never releases
  it. A weapon body replaced for another blade length is taken out of the
  world and kept. It leaks a few hundred bytes per weapon change, because
  the release path of a bhkRigidBody was not read.
- **The drive:** hard keyframe with gain 1.
  - Velocity = gap / (the planner's steps × step length this frame), capped
    at 30 m/s and 75 rad/s.
  - Placed instead of driven when more than 42 units from the hand.
  - **Read** from the planner 0x00889810: with `iUpdateType` set, the frame's
    whole time is split into 1–3 equal steps. With it 0, fixed steps of
    fMaxTime run and the rest carries over.
  - The harness game runs the first way: 1 step of the frame time per
    frame (0.0113–0.0116 s), or 2 on a slow frame.
  - Derived from that: in that mode `PhysicsRate` changes little, because
    it only sets the size the planner splits by.
- **Verified at start-up:** the first bytes of 14 functions, 7 slots of
  bhkRigidBody's vtable and 2 of the keyframed motion's. The log says
  "14 functions and 9 table slots match what was read".

**Harness (`hand-bodies.txt`, PASS, 2026-09-28):**

- **Created:** both hands' bodies keyframed, filter 0x00090016 (group 9,
  layer 22), in the player's world.
- **Driven:** driven each frame; out of the world in a menu and back in
  after it.
- **Pushes:** an Iron Longsword on the floor, PushWorld off, the hand swept
  over it:
  - before, the sword lay at 252, -1585;
  - after, it lay at -162, -1581, about 6 m away.
  - The scripted sweep is far faster than a hand (1.2 m in one frame on its
    return passes, which the body is placed across rather than driven),
    so the distance says nothing about strength. It does say the body
    alone moved it.
- **The weapon:** a drawn sword got its own capsule (60, then 75.5 units as
  the bound settled: one rebuild), and the right hand's body left the
  world while the sword was drawn.
- **The exterior:** after `cow Tamriel 0 0`, the bodies stood at the hands
  in the exterior world.
- No crash in any of it.

**Observed, not understood:** a body is made at its pose, and the first
drive finds it there (0.0 units). A few frames later it stands at the
world's origin (1643 units away) and is placed back. It happens once per
body, after the first entry into a world. Guess (not checked): the
deferred add, or the wrapper's own transform, applies later. Risk: a
body at the origin for a step or two could push what stands there.

**Regression runs with the bodies on (2026-09-28):** `activate-takes`,
`holster`, `teleport`, `fist` and `quick-menu` PASS. `stow` FAILS, and it
fails the same way with `BodyCollision=0`. It has failed since the run of
2026-09-27 18:44: the harness save now stands the player elsewhere, and the
script's hand no longer reaches the dropped sword (80 units above it). This
is not caused by the bodies; the scenario needs new hand positions.

**Not tested yet:**

- a load from a save while the bodies are in the world;
- NPCs touched by a hand;
- whether activating something behind a hand still works: `activate-takes`
  PASSES with the bodies on. That run puts the hand beside the sword, not
  between the eyes and it, so it does not settle the question;
- the feel in the headset.

**Headset test (Nadi, 2026-09-29):** "wirkt alles fein ... ansonsten sehr
überzeugend". Plates, cups and a swept sword push as they should, and NPCs
can now be pushed away. Three open bugs:

- **Open bug: some objects react before the hand reaches them.** "manche
  objekte interagieren teilw zu früh. weiß nicht ob das an den objekten
  liegt oder an uns." **Measured 2026-09-29 - both, and ours is the larger
  part.**
  - **Ours: the hand capsule stands in front of the drawn hand.** The
    `Measure:` lines of `hand-measure.txt` (the scripted hand, its default
    pose), along the hand's forward from the grip: the wrist bone
    (`Bip01 R Hand`) at -11.1 units (7.2 aside), the middle finger's base
    (`Finger2`) at -7.5, its last joint (`Finger22`) at -5.9; the left hand
    the same (-5.6). The capsule runs from -4 - 2.5 to +9 + 2.5 = **+11.5**:
    it covers the space from the fingers to about 17 units (some 24 cm) in
    front of them, and leaves the palm and the wrist behind it out. The
    game units per metre are 70 (`[Tracker] UnitsPerMetre`). Not measured:
    a flat, open hand in the headset (the scripted curl 0 still showed the
    animation's curled fingers) and a user's own `HandAdjust` offsets,
    which move the drawn hand against the grip.
    **Built 2026-09-29 (the tester: "ziel wäre ja zur laufzeit: Kapseln aus
    den gezeichneten Knochen"):** the capsule runs from the wrist bone to
    as far past the middle finger's base as the base is from the wrist (a
    finger about as long as the palm - a human proportion, not measured on
    the model), taken into the grip's frame each frame
    (`HandSpanFromBones`, `ToBodyFrame`). Harness: "the right hand's body
    made ... 15.2 long, from 3.7 -11.1 6.2 to 0.7 -3.9 -6.9" - behind the
    grip now, about 21 cm long. Points further than 40 units from the grip
    are refused (the first frames read the bones before the hand is placed:
    50 units off); then the constants are used until the bones can be read.
  - **The objects': their Havok shapes stand out beyond their meshes.**
    `hand-shapes.txt` places items beside the player and compares the
    world box round the mesh's vertices with the Havok shape's own box
    (hkShape getAabb, vtable +0x0C, the convex radius included):

    | object (base) | mesh (units) | Havok (units) | Havok beyond the mesh, per side |
    |---|---|---|---|
    | cup UpperCupCeramic01 (0002507B) | 11.8 x 9.0 x 7.3 | 14.4 x 13.6 x 11.7 | 1.2 to 2.7 |
    | goblet MiddleMetalGoblet (000CD401) | 7.9 x 11.9 x 7.7 | 13.8 x 14.1 x 13.1 | 1.1 to 3.0 |
    | tankard MiddleMetalTankard003 (000CD403) | 14.5 x 15.3 x 9.6 | 21.1 x 21.4 x 15.9 | 1.7 to 5.0 |
    | longsword on the floor (00000C0C) | 13.8 x 79.4 x 4.3 | 13.9 x 80.4 x 5.0 | 0.0 to 0.8 |

    So clutter's collision is 1 to 5 units (1.5 to 7 cm) larger per side
    than what is drawn; a weapon's is tight. That is the game's data (the
    NIFs' collision), the same for everything that touches them. The
    plate's run placed nothing (the console placement in the harness is
    unreliable), so plates are not measured.
- **Open bug: the sword's tip sometimes passes through.** "wenn ich ein
  schwert habe kollidiert die spitze vom schwert teilweise nicht."
  **Measured 2026-09-29 (`hand-measure.txt`, the longsword):**
  - **The capsule does not lie along the drawn blade.** The drawn weapon
    node's own y axis is the blade (0.99 with the line from its attach
    point to its bound's centre); the capsule's axis (the controller's
    forward) is **11 degrees** off it. The capsule is 75.5 units long
    (1.9 x the bound's 39.7, `BladeLengthFromBound`); the drawn blade's far
    end lies 57.7 units ahead of the grip and **14.0 units to the side** of
    the capsule's axis - 12.5 units outside a capsule 1.5 round. The
    weapon's attach point is 8.3 units behind the grip. So the drawn tip
    and the body's tip are in different places.
    **Built 2026-09-29:** the blade capsule runs from the drawn weapon
    node's attach point along its own y axis to the bound's far end on
    that axis (`BladeSpanFromNode`). Harness: "the weapon's body made ...
    67.2 long, from -0.0 -8.3 1.6 to -3.2 57.7 13.7" - the measured drawn
    blade. The push segment of PushWorld and the melee hit test still use
    the controller's forward.
  - **When a drawn span moves** (another weapon, `HandAdjust` changed, the
    first frames) further than 2 units at either end for 90 frames in a
    row, the body is made anew (`SpanMoved`, `StepSpanWatch`; the bones are
    a frame behind the controller, so a fast hand alone does not count).
    The old body is taken out of the world and kept, not freed (its
    release path is still unread): a few bodies a session.
  - **Passing through between two steps - measured in the headset
    (2026-09-29, the tester's log):** the fastest tip per physics step of
    each swing read 2.3 to 40.6 units, most of them 5 to 17, three of them
    81, 144 and 155 (a fast turn or a jump of the hand). Over 4 a 1-unit
    plate is passed without a contact, over 7 a 4-unit cup: **most real
    swings are fast enough to pass thin clutter.** Not built: a finer
    physics step (`[Hands] PhysicsRate` 120 halves the travel of 60),
    or continuous collision for the blade (its quality, cinfo +0xB3; what
    Havok 3.1 does with it against DEBRIS clutter is unread).
    A keyframed capsule of radius r passes an object t thick without ever
    overlapping it once one physics step carries it further than t + 2r:
    4 units for a 1-unit plate, 7 for a 4-unit cup (`PassThroughTravelUnits`).
    Each swing now logs its fastest tip per physics step (`Measure: a swing
    - the blade's tip moved up to ...`, 16 lines a run). The harness's
    swings are jumps between positions, so its numbers (15 to 60 units a
    step) say nothing about a real swing; a headset log will.
  - Read 2026-09-29: CreateHavok makes a clutter body DEBRIS quality (3)
    when its largest extent is over fDebrisMinExtent (2.0 Havok, about 14
    units; around 0x008A43C3..0x008A43DB, the branch direction derived); what Havok 3.1 does with DEBRIS against
    a keyframed body (continuous or not) is not verified.
- **Open bug, changed 2026-09-29: pushing NPCs made hand-to-hand combat
  hard.** "npcs kann man nun wegschieben. das macht aber h2h combat
  schwierig da man nicht mehr nah genug ran kommt zum hauen." The tester
  decided: "wegschieben aus im Kampf, und mit geballter Faust".
  **Built:** in combat no body pushes people; out of combat a hand made a
  fist does not either (the weapon out of combat still does).
  `game::HandBodyPushesActors`, `HandBodyFist` (the four fingers past
  "Fist at", open again under the open limit, as the fist of 4.3). Such a
  body moves to **layer 23**, the other unnamed layer, whose row in the
  layer matrix OBVR keeps without layer 20 (CHARCONTROLLER: the controller
  cinfo 0x00890C00 builds `(group << 16) | 0x14`, read 2026-09-29); the
  matrix is checked every step because 0x008A83C0 fills it anew. The
  change reaches Havok the way the engine's own setters do it (0x0089F4D0,
  0x0089F520): the filter written at hkRigidBody+0x30, then the wrapper's
  vtable +0x80 (0x008B0060 -> hkWorld::updateCollisionFilterOnEntity,
  0x0089B630). No call to 0x008A7F20 in the exe pushes 22 or 23 (98 calls
  tallied; not a proof that nothing else touches them).
  - Harness (`hand-measure.txt`): "the right hand's body no longer pushes
    people now (layer 23)" when the scripted hands closed, "pushes people
    now (layer 22)" when they opened; "layer 23 no longer collides with
    the character controllers (layer 20)". **Not verified:** an NPC
    actually not pushed - the harness cell has no one to push.
  - **Headset (the tester, 2026-09-29): "NPCs wegschieben passiert
    immernoch, vll einfach durch den player körper? und der scheint mom
    noch einen zu großen radius zu haben."** The log of that run shows the
    right hand switching to layer 23 with its fist and back; no combat was
    logged. Whether the push came from the player's body is not measured.
  - **The player's body (read 2026-09-29, game/PlayerCapsule.h):** each
    actor's character controller has two shapes built from its skeleton's
    bound ("BBX") times its scale (0x00895190): a capsule of the
    character's radius at +0x374 and an 18-point hull at +0x378, which a
    humanoid walks with (+0x36C the slot in use). The radius is at +0x3A0,
    Havok units: **20.2 game units (about 29 cm) for the harness player**
    (measured). The console's SetSize ("player.shrink") changes it through
    0x00894BD0 thiscall(controller, radius): the capsule becomes the shape
    and grows or shrinks to the radius. Shapes are per actor (built, not
    cloned; one shared hull at 0x00BA7A64 only with cinfo +0x85 set).
  - **Built: `[Hands] BodyRadiusScale`** (settings row "Body radius",
    0.3 to 1.5, default 1: the game's own, and OBVR then leaves the radius
    alone). Other than 1, OBVR asks the controller for the game's radius
    times it, the way SetSize does, whenever the controller's target
    (+0x3A8) is another - the engine asks for its own again on entering a
    world. Harness at 0.6: "the player's radius asked for 12.1 units (the
    game's 20.2 x 0.60 ...; slot 0)". Not verified yet: that the capsule
    then stays at 12.1 (a "Measure: the player's body" line at each mark
    was added after the tester started playing), and whether a smaller
    body stops the pushing. How actors push each other in vanilla is not
    read.
  - **Headset (the tester, 2026-09-29): "body radius geht nun gut. der
    wert den ich in der ini habe sollten wir als default sezten"** - 0.5,
    now the default. Harness: "the capsule now 10.1, slot 0" at every mark,
    after a move to the exterior too. **"dennoch kann ich mit den händen
    viel zu einfach npcs wegschieben"**: a keyframed body has no mass
    limit, it moves any controller it meets. **Built: `[Hands]
    PushPeople`, default off** (settings row "Push people"): off, every
    hand and weapon body is on layer 23 and passes through the living
    (harness: filter 00090017 on every body); on, the rules above.
    Ragdolls (the dead, biped layer 8) are pushed either way.

## Proposal: pushing and pulling people on purpose (2026-09-29, not built)

The tester: "wie könnten wir es dennoch erlauben? also das es möglich ist
wenn der spieler es bewusst will. npcs an der hand nehmen und mitziehen wie
in higgs ist auch lustig". With `PushPeople` off the hands pass through the
living; a keyframed body has no mass limit, so a touch alone must not move
anyone. What the Skyrim mods do (their Nexus pages, read 2026-09-29):

- **HIGGS alone** grabs bodies (ragdolls): "Bodies can be grabbed and moved
  around". Living actors are grabbed only with PLANCK, and that grab is
  constraint-driven with a force limit for actors
  (`grabConstraintLinearMaxForceActor`).
- **PLANCK** makes the living physically present. It has **a shove**: "shove
  other actors while the player's hands/weapons are sheathed. If you push
  someone hard enough while sheathed, they will either act as if they were
  just ran into (a large bump reaction), or get staggered ... A shove costs
  40 stamina, and increases the npc's aggression a large amount." It has **a
  grab of the living**: "When you grab someone ... they will try and stay
  next to you ... Once you have grabbed someone, they will walk with you when
  you move." And **a yank**: grab and yank while letting go, and they
  ragdoll and are thrown. Touching or holding someone raises an aggression
  score until they turn hostile.

Proposed for OBVR, in this order; all by intent, never by a mere touch:

1. **The shove.** An open hand, weapons sheathed, driven fast into an actor
   (the hand body's speed at contact over a threshold, towards the actor):
   the engine's own reaction - a bump, or a stagger / knock like the script
   function PushActorAway - plus a fatigue cost and a disposition or
   aggression hit. Needs: the contact test (the hand's span against the
   actors' spheres, as MeleeHits already finds them) and the engine's
   push/stagger call (not read yet).
2. **Taking someone by the hand.** A grip closed on an actor's hand or arm
   (within a few centimetres of the bone): they are held. Each frame their
   controller is steered to keep the held point near the hand, with a speed
   and force limit, so they walk along instead of being dragged through
   walls; let go when the grip opens or they are pulled too far away. Their
   disposition drops the longer it lasts; not in combat (or: in combat it
   is a grapple). Needs: how to steer an actor's character controller from
   outside the AI (its velocity or a move target - not read yet).
3. **The yank.** Held, then a sharp pull while letting go: they fall
   (knocked down, ragdoll) and are thrown a little the way of the pull.
   Needs: the engine's knockdown call.
4. **The dead** can already be pushed (ragdolls, layer 8, `PushPeople` or
   not). Whether the vanilla grab takes a body's limb by the hand grip is
   not checked.

## How HIGGS does it (Skyrim VR, source read)

Source: github.com/adamhynek/higgs, `src/hand.cpp`.

- **Hand body.** `CreateHandCollision` builds a box
  (`bhkBoxShape_ctor(handShape, &halfExtents)`) with a `bhkRigidBodyCinfo`
  of `m_motionType = MOTION_KEYFRAMED` and `m_enableDeactivation = false`.
  It then calls `bhkRigidBody_ctor`, `bhkRigidBody_setActivated(true)` and
  `hkpWorld_AddEntity`. Removal is `hkpWorld_RemoveEntity(world, &ret, body)`.
- **Weapon body.** The weapon's own collision shape is cloned
  (`NiObject_Clone`). The filter is `(playerCollisionGroup << 16) | 56`: the
  player group plus a custom layer.
- **Drive.** Every frame:
  `ApplyHardKeyframeVelocityClamped(targetPos, targetQuat, 1 / deltaTime, body)`.
  The body gets the velocities that reach the tracked pose in one physics
  step.
- **Switching collision off** (while two-handing, for example): bit 14 of
  the filter (`|= 1 << 14`), then `hkpWorld_UpdateCollisionFilterOnEntity`.
- **Physics rate.** HIGGS sets the Havok step to the frame rate, with a
  minimum of 70 Hz and up to 3 substeps (the "havok fix", HIGGS changelog on
  nexusmods.com/skyrimspecialedition/mods/43930).

## Findings in Oblivion.exe

### Havok 3.1.1 core

| What | Address | Signature / layout | Evidence |
|---|---|---|---|
| `hkWorld::addEntity` | `0x008994E0` | thiscall(hkWorld*, hkEntity*, activation), ret 8. Queues the operation while the world is locked (`world+0x88` ≠ 0) | **Read**: Havok timer marker `"LtAddEntity"` at 0x00899547; called with (body, 1) by bhkWorld::AddObject and by the hkWorld constructor |
| `hkWorld::removeEntity` | `0x008996C0` | thiscall(hkWorld*, hkBool* result, hkEntity*), ret 8 | **Read**: marker `"LtRemEntity"` at 0x0089972E. Same argument order as HIGGS's `hkpWorld_RemoveEntity` |
| `hkWorld::updateCollisionFilterOnEntity` | `0x0089B630` | already in `GrabPhysics.h` | **Read**: marker `"LtUpdateFilterOnEntity"` at 0x0089B6B0 |
| `hkWorld::stepDeltaTime` | `0x00898B70` | thiscall(hkWorld*, float dt) | **Derived**: called once per step by bhkWorld::Update with the step length |
| `hkRigidBody::hkRigidBody(const hkRigidBodyCinfo&)` | `0x008A9F50` | thiscall(this, cinfo*), ret 4. Size 0xC4, hkMemory class 0x2A. Vtable 0x00A97A98 | **Read**: both engine creators allocate 0xC4 and call it (0x0089AC72, 0x008A43A0) |
| `hkRigidBodyCinfo::hkRigidBodyCinfo()` | `0x008DF420` | thiscall(this) | **Read**: defaults below |
| `hkRigidBody::setMotionType` | `0x008A9AB0` | thiscall(body, UInt8 type, activation, filterUpdate), ret 0xC. Queued (operation 4) while the world is locked, otherwise `0x008CD4E0` | **Read**: bhkRigidBody's creation calls it as (type, 1, 0) at 0x008A43C1 |
| Motion factory | `0x008A9630` | cdecl(type, pos*, rot*, mass, inertia*, centerOfMass*, maxLinVel, maxAngVel). Case 6 is keyframed (`0x008EA140`), 7 or out of range is fixed (`0x008EA030`) | **Read** (jump table at 0x008A9814) |
| hkMemory | `[0x00BA7D98]`, vtable +0x10 | alloc(size, memClass). The caller then writes the size to word +4 | **Read** (every Havok allocation here) |
| addReference / removeReference | `0x008BC720` / `0x008BC730` | thiscall; refcount word at +6, size word at +4 | **Read** |
| Activate a body | `0x008A6410` | thiscall(hkRigidBody*) | already in `GrabPhysics.h` |

**Motion types**, read from the seven motion vtables (all share
`setLinearVelocity` 0x0089DB90 at +0x54 and `setAngularVelocity` 0x0089DBB0
at +0x58; the type getter is at +0x08):

| Type | Vtable | Class |
|---|---|---|
| 2 | 0x00A979A8 | sphere inertia |
| 3 | 0x00A97A20 | stabilized sphere |
| 4 | 0x00A9AF38 | box inertia |
| 5 | 0x00A9AEC0 | stabilized box |
| **6** | **0x00A9AE10** | **keyframed** |
| 7 | 0x00A9AD90 | fixed |
| (base) | 0x00A96F78 | hkMotion, pure-virtual type getter |

**Derived:** this is Havok's own numbering (1 dynamic … 6 keyframed,
7 fixed). It matches the grab refusing types 6 and 7 (0x0066D5DD).

**hkRigidBodyCinfo layout** (**Read**: the default constructor and the
hkRigidBody constructor's reads):

| Offset | Field | Default |
|---|---|---|
| +0x00 | collision filter info | 0 |
| +0x04 | hkShape* | null |
| +0x08 | byte, copied to body+0x58 (collision response) | 1 |
| +0x0A | word, copied to body+0x8E (contact callback delay) | 0xFFFF |
| +0x10 | position (hkVector4, Havok units) | 0 |
| +0x20 | rotation (quaternion x, y, z, w) | (0, 0, 0, 1) |
| +0x30 | linear velocity | 0 |
| +0x40 | angular velocity | 0 |
| +0x50 | inertia tensor (3 × hkVector4) | identity |
| +0x80 | center of mass | 0 |
| +0x90 | mass | 1 |
| +0x94 / +0x98 | linear / angular damping (to motion +0xC8 / +0xCC) | 0 / 0.05 |
| +0x9C / +0xA0 | friction / restitution (to body +0x5C / +0x60) | 0.5 / 0.4 |
| +0xA4 / +0xA8 | max linear / angular velocity (to motion +0xB4 / +0xB8) | 200 / 200 |
| +0xAC | allowed penetration depth (to body +0x34) | −1 |
| +0xB0 | byte motion type | 1 (dynamic) |
| +0xB1 | byte deactivator type (0x008A9C90: 2 allocates a 0x70-byte deactivator, 0x008E90A0; 1 takes the static one at 0x00B2FD60; to body +0x64). Corrected 2026-09-29: this row said "quality type" | 2 |
| +0xB2 | byte solver deactivation (to motion, 0x0089DB80) | 2 |
| +0xB3 | byte quality type (MotionQuality: 1 fixed, 2 keyframed, 3 debris, 4 moving, 5 critical, 6 bullet, 7 user, 8 character); if 0, body+0x2E becomes 1 fixed / 2 keyframed / 3 other (0x008AA102..0x008AA139) | 0 |
| +0xB4 | byte, copied to body+0x90 | 0 |

**hkRigidBody** (**Read** unless noted): +0x08 hkWorld* (null when not in
a world), +0x0C user data (the bhkRigidBody wrapper, already used by
GrabPhysics), +0x14 collidable, +0x2C owner-type byte (1 = entity,
collidable+0x18), +0x30 filter, +0x50 hkMotion*, +0x92 byte "keyframed or
fixed" (set by the constructor). Motion: +0x10 rotation, +0x40 translation,
+0xD0 linear velocity, +0xE0 angular velocity (known from GrabPhysics and
WorldPush).

**hkWorld:** +0x34 the world's own fixed body (the constructor at 0x0089A230
creates it with motion type 7 and adds it), +0x80 pending operation queue,
+0x88 lock count (non-zero means "queue, don't apply"), +0x2B0 user data =
the owning bhkWorld (**Derived** from 0x008B0020 and 0x0089D730, which reach
the bhkWorld through it).

### Bethesda wrappers (bhk)

| What | Address | Notes | Evidence |
|---|---|---|---|
| `bhkRigidBody` factory | `0x008A41F0` | no arguments; allocates 0x1C bytes (0x00401F00) and constructs (0x008A4150) | **Read** |
| `bhkRigidBody` vtable | `0x00A5605C` | RTTI `.?AVbhkRigidBody@@`, COL 0x00ABF1B8; ends at +0xB0 | **Read** |
| vtbl +0x4C SetObj | `0x0089D730` | sets hkObj (+8), leaves the old world | **Read** |
| vtbl +0x58 GetWorld | `0x0089D940` | returns `[hkObj+8]` (hkWorld*) | **Read** |
| vtbl +0x5C AddToWorld | `0x008A48C0` | thiscall(bhkWorld*), ret 4, bool. With no collision-object property it goes to 0x0089F470 | **Read** |
| vtbl +0x60 RemoveFromWorld | `0x008B0020` | via hkWorld+0x2B0 to `bhkWorld::RemoveEntity` | **Read** |
| vtbl +0x70 CreateHavok | `0x008A4260` | thiscall(bhkCinfo*), ret 4: hkMemory 0xC4, `hkRigidBody` ctor, SetObj, removeReference | **Read** |
| vtbl +0x74 CreateHavokData | `0x008A5980` | thiscall(bool* created): 0xF0-byte, 16-aligned block at wrapper+0x0C, initialised by 0x008A5790 | **Read** |
| vtbl +0xA0 SetTranslationAndRotation | `0x008A2FB0` | already in `GrabPhysics.h` | known |
| `bhkEntity` move to world | `0x0089F470` | thiscall(bhkWorld* or null), ret 4: removes from the old world, then `bhkWorld::AddObject` | **Read** |
| `bhkWorld::AddObject` | `0x0088C210` | thiscall(bhkWorld*, hkWorldObject*), ret 4. Entities (type byte +0x2C == 1) go to `hkWorld::addEntity(obj, 1)`, or to a pending list (bhkWorld+0x28/+0x2C) while bhkWorld+0x1C > 0 | **Read** |
| `bhkWorld::RemoveEntity` | `0x0088B430` | thiscall(bhkWorld*, hkEntity*), ret 4, bool. Deferred to a list (bhkWorld+0x48/+0x4C) while bhkWorld+0x20 > 0, otherwise `hkWorld::removeEntity` | **Read** |
| `bhkWorld::Update` | `0x0088C440` | runs `[0x00BA7914]` steps of `stepDeltaTime([0x00BA790C])` | **Read** |
| `bhkBoxShape` ctor | `0x00564BF0` | thiscall(this, const float halfExtents[4]*), ret 4; allocate 0x14 bytes with 0x00401F00. Convex radius from 0x00B2EFC4 (0.1). **Derived:** the engine halves and scales to Havok units before the call (0x0056567A, 0x005655C9). Vtable 0x00A6593C; its CreateHavok 0x008B8120 builds the hkBoxShape (0x20 bytes, class 0x24) | **Read**: the engine's own runtime use at 0x00565724 |
| `bhkCapsuleShape` ctor | `0x00563BB0` | thiscall(this, data*), ret 4; allocate 0x14. Data (default ctor 0x00564030): +0x04 radius, +0x10 vertex A, +0x20 vertex B (hkVector4, Havok units). Vtable 0x00A65794 | **Read**: 0x00565819 |
| `bhkSphereShape` ctor | `0x00532090` | thiscall(this, float radius, …); vtable 0x00A55D4C | **Read**: 0x0056588F. Signature not fully read |
| Havok critical section | `0x00BA7B00` | enter 0x0043F2E0, leave 0x0043F300 | known (`GrabPhysics.h`) |
| The player's bhkWorld | interior cell's (0x00424180 on cell+0x28), else `[0x00B35C24]` | | known (`PlayerTeleport.h`) |

**The bhk creation block** (the 0xF0 bytes from CreateHavokData, filled by
0x008A5790): +0x00 filter, +0x04 hkShape*, +0x08 byte 1, +0x0C/+0x10 0,
+0x14 0x80000000, +0x18 byte 1, +0x1A word 0xFFFF, **+0x20 an embedded
hkRigidBodyCinfo** (defaults as above, but max linear velocity 10000 and max
angular velocity 31.4159). CreateHavok copies +0x00/+0x04 into the cinfo's
filter and shape (**Read**, 0x008A436E), clamps the damping and velocity
limits, and handles mass 0: a keyframed or fixed type with mass 0 is created
dynamic and then switched with `setMotionType(type, 1, 0)`. **Not settled
(2026-09-29):** a second reading of `test ah,44h; jnp` at 0x008A4328 gives the
opposite - a nonzero mass with a type of 6 or more is created dynamic with
quality 4 (0x008A4337/0x008A433E), a mass of 0 with type 7 gets quality 1
(0x008A4362). The bodies OBVR makes are keyframed as logged, whichever holds.

**Derived:** the shape pointer has to be the `hkShape*`, which is the bhk
shape wrapper's hkObj at +0x08. The hkEntity constructor (0x008A6850) takes
it from the cinfo.

**Body to reference:** the engine goes body → collision object through an
hkEntity property (key 0x00BA7B80, lookup 0x0047F990), then an RTTI check,
then `[obj+8]` for the node (0x0047FA60, 0x00452A60). The collidable
resolver 0x004806E0 returns 0 when the property is missing (**Read**).
**Derived:** a body OBVR creates has no property and no node, so it
resolves to "no reference" wherever the engine asks. The engine's
node-to-Havok push (0x0089EAE0) cannot overwrite it either, because it runs
over nodes' collision objects. This settles the open question in
holding-objects-spec.md.

### Collision filter and layers

- **The rule** `0x008A7F70` cdecl(filterA, filterB) (**Read**):
  1. If layer A ≠ 29 and either filter has bit 14 (0x4000) → no collision.
  2. **If either group (high 16 bits) is 0 → collision**, without looking
     at the layer matrix.
  3. Different non-zero groups → `matrix[layerA] & (1 << layerB)`.
  4. Same group → no collision, unless both are linked (0x8000) or both are
     biped (layer 8); those go to the bone tables.
- **All engine queries go through this rule.** The filter vtable around
  0x00A95CBC has overloads for collidable pairs (0x008A8060), ray casts
  (0x008A8110: ray filter at rayInput+0x24), shape keys (0x008A80E0) and
  one more (0x008A8090); each calls 0x008A7F70 (**Read**).
- **The layer matrix** is at `0x00BA7DB0`: 32 rows × 32 bits, so only
  layers 0–31 have a matrix row. The next table starts at 0x00BA7E30.
  0x008A83C0 fills it with 0xFF (every layer collides with every layer),
  and pick objects overwrite their own row (0x00535A7C: `(1 << own) |
  0xA277F`). **Setter** `0x008A7F20` cdecl(layerA, layerB, bool), symmetric
  (**Read**).
- **Layer names** (table at 0x00B2EB40, **Read**): 0 UNIDENTIFIED,
  1 STATIC, 2 ANIMSTATIC, 3 TRANSPARENT, 4 CLUTTER, 5 WEAPON,
  6 PROJECTILE, 7 SPELL, 8 BIPED, 9 TREES, 10 PROPS, 11 WATER,
  12 TRIGGER, 13 TERRAIN, 14 TRAP, 15 NONCOLLIDABLE, 16 CLOUDTRAP,
  17 GROUND, 18 PORTAL, 19 STAIRS, 20 CHARCONTROLLER, 21 AVOIDBOX,
  **22 and 23 unnamed ("?")**, 24 CAMERAPICK, 25 ITEMPICK,
  26 LINEOFSIGHT, 27 PATHPICK, 28/29 CUSTOMPICK1/2, 30 SPELLEXPLOSION,
  31 DROPPINGPICK, 32 OTHER, 33–56 bone layers (HEAD … WING).
- **Derived: use layer 22.** It is unnamed, nothing in the code refers to
  it, and it has a matrix row. The pick rows (0xA277F) do not include bit 22.
- **Derived: use the player's group** (read from the controller's filter, 0x0065ABE0,
  fallback 9), as HIGGS does. Then the hand does not collide with the
  player's own controller or with the held object (both are in that group
  while held). It also stays out of every ray cast that uses the player's
  group, such as the grab ray at 0x0066DAAC.

### Physics step rate

- `fMaxTime:HAVOK` (setting value at 0x00B097C0, default 1/60 = 0.016667)
  is copied, clamped to ≤ 1, into the runtime value **0x00B2E2E8** when a
  world is created (0x004D4C44, 0x004D52EE) (**Read**).
- The step planner `0x00889810` cdecl(float frameDelta, bool) runs every
  frame from the main loop (0x0040D8C2). It accumulates frame time in
  0x00B2E2E0 (**Read**).
  - `iUpdateType:HAVOK` (0x00B0520C, default 0; copied to 0x00BA7918)
    picks the mode. Mode 0 is fixed steps: count = floor(accumulated /
    fMaxTime), capped at 3 (2 when the flag argument, `[0x00B333B8]`, is
    set). Step length = fMaxTime, and the rest carries over.
  - The step count goes to **0x00BA7914**, the step length to
    **0x00BA790C**.
  - bhkWorld::Update then runs that many steps (the physics step at
    0x0040DE3F in docs/vr-modding/engine-behavior.md).
- **Derived:** at 90 fps the physics runs at a fixed 60 Hz, as measured for
  #38. About one frame in three has no step. A keyframed body driven per
  frame therefore lags the drawn hand by up to one step (16.7 ms) and moves
  in 60 Hz beats. The fix is the same as HIGGS's: set the runtime copy
  0x00B2E2E8 to the headset's frame time. **Not** the INI setting: writing
  a setting persists it to the user's INI (see the iSize incident in
  docs/vr-modding/failed-approaches.md). It must be rewritten after each
  world creation.

## The held object today: already a Havok body

Status: **built (`[Hands] HeldObjectsPush=1`, 2026-09-28), not yet tested
in the headset. The tester (Nadi) will test it; nothing changes until then.**

A held object needs none of the body creation above. It is already a
dynamic Havok body, so Havok computes its contacts with other objects
itself. What OBVR does (holding-objects-spec.md, "Built 2026-09-28: the
held object pushes"; `game::DriveLinearVelocity` /
`DriveAngularVelocity`, `GrabPhysics.h:246-329`):

- Each frame the body gets the linear and angular velocity that bring it
  to the pose shown in the hand: gain 0.8, at most 15 m/s and 40 rad/s.
  The time used is the frame time (1/90 s).
- It stays **dynamic** (finite mass), not keyframed. Derived from that:
  - **Against clutter:** it pushes with real contacts, friction and
    tipping. A light object against a heavy one meets it in between,
    because both have mass.
  - **Against walls and statics:** it is stopped. HIGGS differs here: its
    regular grab keyframes the held object, which then passes through
    walls.
  - **Against the player and the hands:** while held, the body is in the
    player's group and collides with neither the player's capsule nor
    (once built) the hand bodies. Same group, no collision (rule step 4).
- More than 35 units (about half a metre) from the hand, for example held
  back by a wall: the drive stops, the engine's spring pulls again, and the
  engine lets go as in vanilla.

Two possible issues, **derived from the code, not observed**:

1. **Picture and physics at a wall.** In the in-hand mode the visible
   model is written straight to the hand. The body is only driven. A wall
   holding the body back leaves the picture in the hand, inside the wall,
   and the contact happens where the object is not seen.
2. **60 Hz against 90 Hz.** The drive uses the frame time, but Havok steps
   in fixed 1/60 s steps, and about one frame in three has no step (see
   "Physics step rate"). That allows roughly 1.2× overshoot and uneven
   motion: a possible slight jitter while pushing.

Decision: **test first**. Only what the test shows gets fixed.

- If 2 shows, the drive uses the planner's step count and length, as in
  "Driving it every frame" (a small change).
- If 1 bothers the tester, the choice is:
  - (B) keyframe the held object while held, as HIGGS does:
    `setMotionType` 0x008A9AB0 to 6, the old type back on release. It
    then pushes everything and passes through walls, and picture and
    physics always match.
  - (C) let the picture follow the body once it is blocked.

**Test in the headset (Nadi):**

1. Pick up a plate and push a cup off the table with it. Does the cup
   tip and fall?
2. Push the plate slowly against a wall. Is it seen inside the wall? Is
   it let go after about half a metre?
3. Slide the plate slowly across the table against other objects. Does
   what it pushes jitter?
4. Push a light object against a heavy one (a spoon against a crate).
   Does it feel too weak?

**Result (Nadi, 2026-09-29):** it works. **Open bug:** "manche objekte
kollidieren bevor die objekte nah ran kommen, keine ahnung ob das einfach
bei oblivion so ist." Not measured; probably the same question as the
hands' early contact above (the objects' Havok shapes against their
meshes), not yet settled.

## Design

### Creating a body (per hand, and per drawn melee weapon)

All calls run on the main thread, inside the Havok lock (0x00BA7B00), from
the frame hook that runs `WorldPush` today:

1. **Shape.** Hand: `bhkBoxShape` (0x00564BF0) with half extents in Havok
   units (game units × 0.142877). Weapon: `bhkCapsuleShape` (0x00563BB0)
   from the blade segment `WorldPush` already computes (grip → tip) plus a
   radius. Cloning the weapon's NIF collision, as HIGGS does, is a later
   option; the capsule needs no clone and no NIF.
2. **Wrapper.** `bhkRigidBody` factory 0x008A41F0. Take one NiRefObject
   reference (xOBSE `NiRefObject`: refcount at +4. **Unverified** in
   Oblivion.exe, to be read before building).
3. **Creation block.** vtbl +0x74 (CreateHavokData) → fill: +0x00 = +0x20
   = `(group << 16) | 22`; +0x04 = +0x24 = the shape's hkObj; cinfo
   +0x10 position and +0x20 rotation = the tracked pose; +0xB0 = 6
   (keyframed); mass 1 (non-zero, so the direct keyframed path runs);
   +0xB2 solver deactivation off (**Unverified**: the value of "off" in
   3.1.1 is not read; the fallback is activating the body every frame).
4. vtbl +0x70 (CreateHavok) with the block.
5. vtbl +0x5C (AddToWorld) with the player's bhkWorld. It is deferred
   safely while the world steps (bhkWorld+0x1C).

### Driving it every frame

- Once per frame, after the step planner (0x0040D8C2) and before the
  physics step (0x0040DE3F): target = tracked hand (or blade) pose.
  - n = `[0x00BA7914]`, dt = `[0x00BA790C]`.
  - n > 0: v = (target − current) / (n · dt); ω from the quaternion
    difference / (n · dt). This is `game::DriveLinearVelocity` /
    `DriveAngularVelocity` with gain 1 and that time (GrabPhysics).
  - n = 0: no step this frame, so leave the body untouched.
  - Set through motion vtbl +0x54 / +0x58, then activate (0x008A6410).
- **Teleport instead of velocity** when the gap is too large (a recenter,
  a teleport, a loading screen, first creation): vtbl +0xA0
  (SetTranslationAndRotation), velocity 0.
- Current pose: motion +0x40 / +0x10, as the held-object drive reads it.

### World changes and removal

- Every frame compare the body's world (vtbl +0x58 → hkWorld+0x2B0) with
  the player's bhkWorld. If they differ, call 0x0089F470 (move: remove
  from the old, add to the new). With no world (main menu, loading), call
  vtbl +0x60.
- Remove and release when: the setting is turned off, the weapon is
  sheathed (weapon body), a game is loaded, or the plugin shuts down.
- **Switching off without removing** (two-handing, holding, menus): set
  filter bit 14 and call `hkWorld::updateCollisionFilterOnEntity`
  (0x0089B630), as HIGGS does. Layer 22 is not 29, so bit 14 works.

### Settings

- `[Hands] BodyCollision` (default 1 once it is proven; 0 falls back to
  `WorldPush`).
- `HandBoxHalfExtents` and `WeaponRadius` (cm).
- `PhysicsRate` (0 = the game's 60 Hz, otherwise the headset rate).

## Phases and acceptance

1. **Addresses at start-up.** Compare the first bytes of every function
   above with the bytes read here, and log one line.
   - PASS: the line appears, all match.
2. **One static box.** Create a keyframed box in front of the player,
   leave it there, and log its hkWorld* and motion type (must be 6).
   - Harness: drop a plate onto it, and the plate must rest on it.
   - PASS: log plus screenshot.
3. **Driven hand box.** Hand-script harness (synthetic hand).
   - Sweep through clutter on a table: the objects move, and they move
     with a still hand pressing on them.
   - PASS: logged velocity changes on the clutter plus screenshots. The
     crosshair activation behind the hand still works.
4. **Weapon capsule.** The same, with a drawn sword swung through clutter.
   `WorldPush` is off for anything that has a body.
5. **Physics rate.** 0x00B2E2E8 set to 1/90 after each world creation.
   - PASS: the step count at 0x00BA7914 is 1 in every frame at 90 fps
     (log). Frame time does not regress in `tools/analyze-performance.py`.
6. **World change and load.** Interior ↔ exterior door, fast travel,
   loading a save.
   - PASS: no crash; the body is in the player's world afterwards (log).

## Tests (pure logic, FrameLogic pattern)

Every flow gets a unit test in `tests/`:

- **Filter composition:** group and layer → filter. Off (bit 14 set)
  and on. Group 0 falls back to 9.
- **Planner-aware drive:** n = 0 (no change), n = 1, n = 3 (velocity
  divided by 3·dt). A gap above the teleport limit → teleport. Clamping at
  the maximum velocities.
- **Quaternion delta → ω:** identity, a small angle, near 180°, the
  opposite-hemisphere sign flip.
- **Lifecycle decision:** (setting, weapon drawn, world now, world of
  body, loading) → create / move / remove / keep / collision off. Every
  combination.
- **Capsule from blade:** grip/tip in game units → vertices in Havok
  units; a degenerate zero-length blade is refused.
- **Box half extents:** cm → Havok units.

## Open questions and risks

- **Group-0 bodies ignore the layer matrix** (rule step 2). If clutter
  and statics have group 0, layer 22 collides with everything, triggers
  (phantoms, layer 12) included.
  - **Unverified**: the groups of clutter, statics and triggers. First
    step: log the filter of every body `WorldPush` meets.
  - A trigger fired by the hand may do nothing, since the body resolves to
    no reference. Also unverified.
- **Ray casts with group 0** (for example the crosshair pick 0x005806D0
  — its filter has not been read) would hit the hand body and could block
  activation behind the hand. Check it in phase 3. If it happens, read the
  pick's filter; there is no hook yet.
- **Contact listeners.** Any engine listener that reaches for a
  reference without the null check 0x004806E0 has would crash on our
  body. **Unverified**; phase 2 and 3 are the test.
- **Keyframed vs. character proxies** (NPC controllers, layer 20): it is
  not read whether a proxy is blocked by a keyframed body. If NPCs get
  pushed by the hand, clear the matrix bit 22↔20 (0x008A7F20).
- **Lifetime of the bhkShape wrapper** and the NiRefObject release path
  have not been read. The body holds an hkShape reference (the hkEntity
  constructor), but not necessarily the wrapper.
- **Saves:** the body belongs to no form or node, so it is not saved
  (**Derived**). On load the old world is destroyed. Remove first (phase
  6).
- **Solver deactivation "off" value** in 3.1.1 is not read (see Creating
  a body, step 3).
- **Changing the physics rate** affects all physics (ragdolls, clutter,
  and possibly the chase camera stepping from #38). It stays a separate
  setting, off by default until measured.

## Rejected alternatives

- **An invisible reference with a keyframed NIF collision** (an
  activator the engine places). It would spare the constructor search,
  which is now done. It would also add a form, a save-game footprint and
  the engine's node sync fighting the drive.
- **A raw hkRigidBody without the wrapper** (0x008A9F50 + 0x008994E0) is
  possible. The wrapper is preferred because it brings the deferred
  add/remove during a step, the world back-pointer and the user data
  GrabPhysics/WorldPush already read, and it mirrors HIGGS.
- **Keeping `WorldPush`** is the fallback (`BodyCollision=0`), not the
  target.
