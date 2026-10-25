# Weapon collisions with everything (feasibility and design 2026-10-09; walls and bodies built 2026-10-09)

The tester (2026-10-09): "waffen kollisionen mit allem. machbarkeits
analyses. wie würdest du es umsetzen oder designen".

Read here as: the drawn weapon meets whatever is in its way - walls,
furniture, the ground, clutter, the living, the dead, an opponent's weapon,
a shield - and each answers the way it would: a wall stops the blade, a cup
is knocked over, a body is hit, a blade is parried.

Markers as in the other specs: **Read** (seen in the code, the
disassembly or a source), **Derived** (concluded from what was read),
**Unverified** (still to be shown in the game), **Proposed** (a design).
Addresses are Oblivion.exe 1.2.0.416.

## Verdict

| What the blade meets | Today | Proposed | Feasible | Size |
|---|---|---|---|---|
| Walls, floors, furniture, trees, doors (fixed bodies) | passes through | stops at the surface and slides along it; the controller goes on, the drawn weapon stays | yes, every engine piece is in use already | M |
| Clutter (dynamic bodies) | pushed by the keyframed body; fast swings pass thin things | pushed as now; a fast swing that crosses one kicks it | yes | S |
| The living | passes through; a hit when the blade passes the bound sphere | a slow touch stops the blade at the body; a swing hits and then passes through, as in PLANCK | yes, with OBVR's own bone capsules | M-L |
| The dead, ragdolls | pushed (biped layer) | pushed as now | built | - |
| An opponent's weapon | nothing | the blades meet: our blade stops; met during their attack, the blow is parried | likely; the blow's timing is unmeasured | L |
| Shields (theirs and ours) | ours blocks by the raised-hand gesture | a shield stops a blade; ours blocks the blows it meets | likely | M |
| Arrows and spells | nothing | parried by the blade | unknown | L |

The recommendation: **one contact step of OBVR's own for the drawn weapon,
kinematic, between the weight spring and everything that uses the drawn
pose.** Havok keeps pushing what can move (the keyframed body, built);
OBVR decides where the drawn weapon may be. Not a dynamic weapon on a
Havok joint (section 3.1 says why not).

## 1. What OBVR has already (read in the repo)

- **The drawn pose is apart from the controller.** `vr/WeaponWeight.h`
  lags the drawn weapon hand behind the controller by the weapon's weight,
  in the tracking space (`StepWeaponLag`, called in `vr/HandMode.cpp`).
  Everything downstream uses the drawn pose (`weaponHandRotation`,
  `weaponHandOffsetUnits`): the bone pin, the Havok body, the strike. A
  collision only has to decide this pose; nothing else needs to know.
- **The weapon is a keyframed Havok body** (`game/HandBodies`,
  hand-weapon-collision-spec.md): a capsule along the drawn blade
  (`BladeSpanFromNode`). It pushes clutter and ragdolls. Nothing pushes it
  back: a keyframed body has infinite mass, and against a fixed body nothing
  is resolved (Read, hkpMotion.h; the HIGGS author says the same of his
  hands).
- **The world's ray pick** (`game::PickWorldSegment`, bhkWorld vtable
  +0x88) answers the hit point, the surface normal, the fraction along the
  ray and the collidable hit. The collidable leads to its body and its
  filter (`game/WorldPush`), and the filter's low bits are the layer
  (STATIC 1, CLUTTER 4, BIPED 8, TERRAIN 13, CHARCONTROLLER 20 ...).
- **Layer 23** is OBVR's own quiet layer: its matrix row is kept without
  the character controllers (layer 20), checked every step
  (`game/HandBodyLogic.h`). A ray cast as layer 23 with the player's group
  does not stop at a controller's coarse capsule (Derived from the filter
  rule 0x008A7F70: different non-zero groups go through the matrix).
- **The strike by motion** (`game/MeleeHits`) tests the drawn blade against
  each living actor's bound sphere (0.7 of its radius plus 8 units) and
  hands a hit to the engine's hit function 0x005FEBF0.
- **Bones of other actors** are read already: by name through GetNiNode
  (vtable +0x154) and GetObject (+0x58) for leading by the hand
  (`game/Lead`) and for reaching to open (`game/ReachTargets`).
- **The block** is the engine's own, held by a gesture (`IsBlockGesture`,
  `IsWeaponGuard`), and the engine's cone check is rerouted to the gaze
  (`game/BlockCone.h`). How vanilla resolves a blocked hit is read in
  combat-comfort-spec.md, section 1: the target's process action has to be
  Block (6, 0x005E5670); the blocked share comes from 0x005474A0; a blocked
  melee blow without a counterattack makes **the attacker recoil**
  (0x005F4F00 at 0x00600565, action 7).
- **Process actions** are known (`game/GameAddresses.h`): Attack 2,
  AttackFollowThrough 3, Block 6, Recoil 7.
- **Controller vibration** goes through `TriggerHapticVibrationAction`
  (`vr/ControllerActions.h`, used by `vr/MenuHaptics.h`).
- **Engine sounds** play through 0x006AF880 (`PlaySwingSwish`). Oblivion.esm
  carries the sounds a contact needs (Read 2026-10-09, its SOUN records):
  `WPNHitBladeX` 0000C3C4, `WPNHitBluntX` 0000C3C7, `WPNHitHandX` 0000C3CA,
  `WPNBlockBladeX` 0000C3CB, `WPNBlockBluntX` 0000C3CC,
  `WPNBlockShieldLightX` 0000C3D0, `WPNBlockShieldHeavyX` 0000C3CF, and
  material collision sounds such as `CSpecialHeavyStone` 00097C14,
  `CSpecialHeavyWood` 00097C16 and `CSpecialHeavyMetal` 00097C15. Which one
  vanilla plays for what is not read; the names are the evidence.

## 2. How others do it (sources)

- **Blade & Sorcery:** "an invisible object track[s] the position of your
  controllers exactly ... a joint - think like a really strong rubber band -
  that connects this invisible object to the player's in-game hands. This
  allows you to put your controllers through a wall in-game, but your
  in-game hands get stuck on the wall"
  (reddit.com/r/BladeAndSorcery/comments/17jpdkv, quoted in
  physical-combat-spec.md).
- **HIGGS** (Skyrim VR): keyframed hand and weapon bodies; "there is no
  interaction between 'fixed' objects like walls and your hands"
  (reddit.com/r/skyrimvr/comments/100k7jp).
- **PLANCK** (Skyrim VR, nexusmods.com/skyrimspecialedition/mods/66025,
  read 2026-10-09): the weapon collides with the enemy's ragdoll "at any
  speed and will influence their ragdoll". Above a swing speed it counts as
  a hit, and "collision will be disabled between your weapon and that npc
  until the weapon stops colliding with the npc for a very short time
  (hitCooldownTimeStoppedColliding, default 0.22 seconds)", or after 1.2 s
  (`hitCooldownTimeFallback`). So **a hit passes through; a touch does
  not.**
- **Pseudo Physical Weapon Collision and Parry**
  (nexusmods.com/skyrimspecialedition/mods/100781,
  github.com/ijwzac/WeaponCollisionVR, read 2026-10-09):
  - geometric, not Havok: "it detects collision every frame instead of only
    the hit frame";
  - when the weapons meet while the enemy attacks (VR players need not
    attack), the "enemy's current attack will always be nullified" (with the
    player's stamina above 10 %);
  - the enemy recoils when the controller swings gently, recoils and
    staggers when it swings fast; both lose stamina;
  - "This mod delays melee hit events on the player for 9 frames ...
    because in Skyrim enemies have some cheating attack animations, where
    they hit you a few frames before their weapons reach you."

## 3. The design

### 3.1 One contact step, kinematic

Each frame, for the drawn melee weapon:

1. The controller gives the wanted pose.
2. The weight spring gives the drawn pose it would take (built).
3. **The contact step** sweeps the blade from last frame's drawn pose to
   that pose and stops it at the first contact that stops a blade (3.3).
4. That pose is the drawn pose for everything: the bones, the Havok body,
   the strike, the two-hand grip.
5. The spring is told where the blade stopped (3.4), so the weight goes on
   from there.

Why not a dynamic weapon on a Havok joint (B&S's way, physical-combat-spec
F2 way 2):

- A living person in Oblivion is only a character controller in the
  Havok world; their bones are not in it while they are animated (Read,
  0x008A493C..0x008A4962). The controller's proxy is a phantom, so a dynamic
  blade would likely not be stopped by them (Derived, not verified in 3.1).
- Fast swings already pass thin things between 60 Hz steps (tips measured
  at 5 to 40 units a step, a few at 81 to 155). A dynamic blade would pass
  walls the same way. A sweep of OBVR's own sees the whole path.
- The picture and the physics drift apart for a dynamic body (the held
  object's open issue).
- Opponents' weapons and shields are not in the Havok world either
  (Derived from the same rule: they hang on bones).

### 3.2 The sweep

- **What is swept:** five points on the drawn blade (the guard, a quarter,
  the middle, three quarters, the tip), each from where it was last frame
  to where it would be now, plus one ray along the blade at the new pose
  (guard to tip), which finds a blade that would end up inside something.
  The blade is `BladeSpanFromNode`'s span, the one the Havok body uses.
- **What is asked:**
  1. the Havok world: one ray pick per point (`PickWorldSegment`, cast as
     layer 23, so not stopped by controllers). The hit's layer says what it
     is;
  2. OBVR's own shapes, by arithmetic: capsules on the bones of the actors
     near the player (4 C), their drawn weapons as capsules (4 E), their
     shields as boxes (4 F).
- **The result:** the earliest contact - its fraction of the frame, the
  point, the normal, its kind (fixed, movable, living, dead, weapon,
  shield), the reference, the bone, and the blade's speed there.
- The pose at the contact: position blended, rotation slerped by that
  fraction (`SlerpTowards`). Over one frame at 90 Hz a turning point's path
  is close to its straight line (Derived; the error grows with the turn per
  frame).

### 3.3 What each kind does

| Kind | Found by | The blade | The engine | Feel |
|---|---|---|---|---|
| Fixed: layers STATIC, ANIMSTATIC, TERRAIN, TREES, PROPS, GROUND, STAIRS | the ray pick | stops, slides along | nothing | vibration and a sound by speed |
| Movable: CLUTTER, WEAPON on the ground | the ray pick | goes on | the body gets the blade's velocity at the point (WorldPush's kick), so a fast pass knocks it away | a light vibration |
| Living, slow (no swing) | bone capsules | stops on the body, slides | nothing | a light vibration |
| Living, a swing | bone capsules | goes through; that actor is passed for the rest of the swing (PLANCK's rule: until the blade has left them for 0.22 s, at most 1.2 s) | the hit, as now (0x005FEBF0), from the contact | hit vibration, the engine's hit sound |
| Dead, ragdoll: BIPED | the ray pick | goes on | the Havok body pushes it (built) | a light vibration |
| Their weapon | their blade's capsule | stops | during their attack: parried (4 E) | the block sound, a strong vibration |
| A shield | the shield's box | stops | theirs: our blow lands on the shield, not the body; ours: blocks (4 F) | the shield block sound |
| Triggers, water, NONCOLLIDABLE, CHARCONTROLLER, the player's own group | - | ignored | - | - |

### 3.4 Stop, slide, let go

- **Stop:** the blade is held at the contact, a blade's radius out along
  the normal.
- **Slide:** what is left of the frame's motion, without its part into the
  surface, is swept once more. A blade pressed against a wall and moved
  sideways scrapes along it.
- **Started inside:** when the along-blade ray hits at the drawn pose
  already (a teleport next to a wall, a door swung into the blade), nothing
  is clamped until the blade is free. A blade is never trapped.
- **Let go:** when the controller is further than a cap from the drawn
  hand (proposed 0.30 m or 45 degrees), the weapon goes back to the hand and
  passes through, B&S's "too far" case. Decision 1 below.
- **Back to the spring:** the stopped pose becomes the spring's state, with
  the velocity into the surface taken out (an inelastic contact). The weight
  pulls the blade on from where it stands; let go, it swings back to the
  hand with its weight. For a weapon with no lag (a dagger, or the slider
  at its lowest) the step keeps last frame's drawn pose itself.
- **Two hands on the handle:** the left hand already holds the handle where
  the drawn weapon is (`CameraHook.cpp`, the weight's grip), so it stops
  with it.
- **The view is never moved.** Only the drawn hand and weapon stop.

### 3.5 The feel

- Vibration on every contact, its strength from the blade's speed into the
  surface; a scrape repeats it softly while sliding.
- A sound from the list in section 1 by kind and weapon type, only above a
  speed, at most a few a second. Sound by surface material (stone, wood,
  metal): the shapes carry a Havok material in the NIF, but where it is at
  runtime is not read.
- A hit-stop: on a landed hit the drawn blade is held back for 2-3 frames,
  never the view. Optional (decision 4).

## 4. Each target in detail

### A. Walls, floors, furniture (M)

- Everything needed is in use: the ray pick with the normal, the drawn
  pose, the weight spring's state. New: the sweep, the stop and slide, the
  kind from the hit's layer, the handover to the spring.
- **Unknown:** how far architecture's collision stands out of its mesh.
  Clutter's stands 1 to 5 units out (measured, hand-weapon-collision-spec);
  if walls are similar, the blade stops a few cm short. To measure in the
  harness: the tip's distance to the drawn wall at a stop.
- **Unknown:** whether a ray pick stops at trigger volumes. If it does, the
  wall behind one is not seen; then cast again from just past it.
- **Unknown:** the cost of a ray pick. Today the hand rays cast 4 a pusher
  a frame without a measured cost; this adds about 6. The profiler measures
  it before the step goes on by default.

### B. Clutter (S)

- Pushing is built. The open bug "the sword's tip sometimes passes
  through" (measured: most real swings travel far enough between physics
  steps to pass a cup) is fixed by the sweep: a CLUTTER hit on the swept
  path gets the blade's velocity at that point - WorldPush's kick, already
  written for the rays.
- Clutter never stops the blade. A heavy thing slowing it is not proposed.

### C. The living (M-L)

- **Bone capsules** (physical-combat-spec F2 way 1): head, the three spine
  bones, pelvis, upper and lower arms, thighs and calves, each a capsule
  from one bone to the next with a radius per part.
  - The radii are proposed constants for a human (a head 10 units, the
    torso 14, limbs 5 to 7), times the actor's scale. Not measured on the
    meshes; the harness can compare them with the bound. To be replaced by
    capsules fitted to each actor's mesh: mesh-fitted-capsules-spec.md
    (2026-10-24, open).
  - Creatures: the "Bip01" names are not known for every creature
    skeleton; without them, the bound's upright column the shove uses
    (`HandAtBody`).
  - Node pointers are cached per actor with a fingerprint and read anew
    after a load (the skin-swap lesson).
- **Touch and hit:** a slow touch stops the blade on the body; a swing (the
  strike's own swing state) hits at the contact and then passes, PLANCK's
  rule. The contact replaces the bound-sphere test in `MeleeHits`, so a hit
  lands where the blade is seen, and the bone hit is known (for a later
  damage by body part - not proposed now).
- Shove and lead are untouched; they use the hands, not the weapon.

### D. The dead (built)

Their bones are in the world on BIPED (layer 8). The keyframed body pushes
them; the sweep lets the blade go on.

### E. An opponent's weapon: parry (L)

- **Their blade:** the "Weapon" node of their skeleton, along its own
  axis, to its bound's far end - `BladeSpanFromNode` on their node.
  Unverified: that the third-person skeleton carries the node and that it
  is up to date when OBVR reads it (actors in high process animate each
  frame; whether the transform is this frame's or last frame's is not
  known - a frame late would still fall inside a parry window).
- **The test:** our blade's sweep against their blade's sweep (two segments
  that both move), a few sub-steps per frame.
- **Physically:** our blade stops at theirs, as at a wall. Theirs is played
  by an animation and is not moved.
- **The parry:** they are in the Attack action (2) with the player as their
  target, and their blow has not landed yet. The blow is then resolved as
  blocked by the engine's own path:
  - The hit function asks whether the target is blocking (0x005E5670) and
    whether the attacker is in front (0x006131D0, already rerouted). For a
    parried blow, both answer yes. The call site of 0x005E5670 in
    0x005FEBF0 is not read yet; it is the same kind of hook as BlockCone.
  - Then vanilla runs: the block reaction, Block's experience, the fatigue
    and wear, and **the attacker recoils** (0x005F4F00, read). UESP's
    Oblivion:Block lists recoil only for hand-to-hand blocks; the
    disassembly shows it after every blocked melee blow without a
    counterattack, and the CS wiki agrees ("An actor is recoiling when their
    strike is blocked"). The code was read, so it is trusted over UESP.
  - **How much it stops:** vanilla's blocked share is at most 75 %, and with
    a weapon half the skill's (combat-comfort-spec: a Block 50 player stops
    25 %). For a parry that feels poor. Proposed: a parry stops the whole
    blow (the share answered as 1 for that blow, at 0x005474A0's call) and
    costs fatigue - as the Skyrim parry mod does. Decision 3.
  - **A fast parry:** a stagger of the attacker as well (0x005F4FD0, read,
    used by the shove), as the Skyrim mod does by controller speed.
- **Unmeasured, and the largest risk:** when Oblivion resolves an NPC's
  blow against where its blade is. The Skyrim mod delays the player's hits
  by 9 frames because Skyrim's blows land before the blade arrives. If
  Oblivion's do too, a parry would come too late; then the blow has to be
  held back a few frames (a hook on the hit against the player), which is
  more risk. First step: log, at each blow on the player, how far the
  attacker's blade tip is from the player's body.
- The gesture block stays as it is. A parry is the blades meeting, without
  holding the gesture.

### F. Shields (M)

- **Theirs:** the shield hangs on "Bip01 L ForearmTwist" (Read,
  `camera/FrameLogic.h`); its box from its node's bound. Our blade stops at
  it. When they are blocking, the engine decides as now; when they are not,
  a blow that meets the shield never reaches the body, so it is no hit -
  physically right, and a change from today, where the bound sphere is hit.
- **Ours:** the raised shield blocks the blows it meets: the same parry
  path as E, with the shield's factor. This is proposal A of
  combat-comfort-spec ("a block counts only when the blades meet") built
  on contacts instead of an angle.
- Our shield against walls: the same contact step can run for the left
  hand's shield, later. **Built 2026-10-11 (section 14), and the free hand
  with it.**

### G. Arrows and spells (L, unknown)

A projectile's position and velocity per frame, and a way to deflect or
end it, are not read. Not proposed now.

### H. Not proposed

- The hands at walls (B&S's hands). The same step can serve them later;
  the weapon first.
- Wear on wall hits, breaking things, sparks (Oblivion has no destructibles;
  a spark effect was not looked for), water splashes.
- Whether a wall hit's sound alerts sneaking targets is not known.

## 5. Settings (proposed)

Each part behind its own switch, as the tester asked for ReachOpens ("alles
hinter einem feature flag"):

- `[Hands] WeaponStopsAtWalls` (A, and B's kick with it), default 1.
- `WeaponStopsAtBodies` (C), default 1.
- `WeaponParries` (E, F), default 1 once measured.
- `ParryStopsAll` (decision 3: on), `WeaponLetGoMetres` (0.30; 0.60 since 2026-10-10, the tester: "bei 0,3 ist generell zu früh"),
  `WeaponLetGoDegrees` (45), `ContactHaptics` (strength, 0 off).

## 6. Phases and acceptance

1. **Measure (S).** The sweep runs and only logs: per swing what it met
   (kind, layer, reference, speed), the pick's cost, and at each blow on the
   player the attacker's blade tip against the body. Harness: a sword drawn
   into the harness cell's wall; a placed NPC in combat (the lead and throw
   scenarios place people already).
   - PASS: "Contact:" lines for the wall; the cost per frame in the profile.
2. **Walls (M).** Stop, slide, let go, the spring handover.
   - Harness: the scripted hand pushed into the wall - the drawn tip stays
     outside it while the controller is 20 cm inside; past the cap it lets
     go; pulled back, it follows.
3. **Clutter kick (S).** Harness: a scripted fast sweep over a cup knocks it
   down every time (today it sometimes passes).
4. **Bodies (M-L).** Harness: a placed beggar; the blade lowered slowly onto
   the shoulder stops there, no hit; a swing hits once and passes.
5. **Parry (L).** Only after the blow timing of phase 1 is known. Harness:
   the blade held across in front of the body against an attacking NPC; the
   blow logged as blocked and the attacker recoiling.
6. **Shields (M).**
7. **The headset** judges each.

## 7. Tests (pure, the FrameLogic pattern)

- The earliest contact of several rays; none; a hit at the first point.
- Stop and slide: the slide's direction, a second contact in the slide, a
  corner.
- Started inside: no clamp until free.
- Let go: by distance, by angle, back again.
- The spring handover: the velocity into the surface removed, the rest
  kept.
- The kind table: every layer to its kind, the ignored ones.
- Capsule against a swept segment; two moving segments; a parallel and a
  crossing pair.
- The hit pass-through ledger: touch, hit, left for 0.22 s, the 1.2 s
  fallback.
- The parry window: their action, their target, the blow landed or not.
- Each switch off.

## 8. Decisions (the tester, 2026-10-09)

"1) Variante A 2) Langsam an einen NPC gehalten liegt die Klinge auf 3) Gar
keiner (dein Vorschlag) 4) ja . fang an"

1. **Past the cap:** the weapon lets go and passes through, and a blade
   that went through strikes nothing until it is free again (added to A
   when it was explained).
2. **The living, slowly:** the blade rests on them; a swing hits and passes.
3. **A parry:** stops the whole blow ("gar keiner": no damage comes
   through) and costs fatigue - `ParryStopsAll` on.
4. **A hit-stop** on a landed hit: yes.

## 9. Built: phases 1 to 3, walls (2026-10-09)

- `game/BladeContactLogic.h` (pure, `blade_contact_test`), the Havok side
  in `game/BladeContact.cpp` (layer 23 picks, `ReadPickBody`,
  `KickBodyByBlade` in WorldPush), wired as `StepWeaponContact` in
  CameraHook before the strike; settings `[Hands] WeaponStopsAtWalls`,
  `WeaponLetGoMetres`, `WeaponLetGoDegrees`. controls-spec 4.32 has the
  details.
- Differences from the design above:
  - Each point's ray reaches the margin (1.5 units) past its end, so a
    point never ends nearer a surface than that - a ray that began on a
    face did not see it, and the next frame's ray went into the wall (unit
    test, a wrist turned into a wall in 5-degree frames).
  - A pose across something along its length is never taken: the slide's
    end, then the stop, then where the blade was; across something even
    there, it lets go (a door swung over the blade).
  - Nothing is fed back into the weight spring: it runs on behind the
    wall, and the blade follows it again as soon as the way is free. The
    spring's momentum into the wall is therefore not taken out - a heavy
    weapon let go of swings on as it would have. Not judged in the headset.
  - A camera jump (over 40 units or 15 degrees in a frame) sweeps nothing;
    the blade is taken up where it is wanted, through if inside something.
- **Harness** (`blade-wall.txt`, PASS 2026-10-09): the floor met at -256,
  a fixed static (layer 1, motion 7); the tip held at -254 with the hand's
  at -261 and -272; let go at 24.2 units; free again raised. 2.0 rays and
  0.004 ms a frame over 600 frames, no ray met anything but rigid bodies.
- **Answered from section 4 A:** a ray pick met only rigid bodies in the
  harness cell (no trigger seen); the cost is small. How far walls'
  collision stands out of their meshes is still not measured (the floor was
  met 2.5 units above the player's reference point, which says nothing of
  the drawn floor).
- **Not exercised yet:** a wall and the slide along it, the kick of a cup
  by a fast swing, the knock and the pulse, a two-hander. Headset open.

## 10. Built: the living and the hit-stop (2026-10-09)

- `game/BladeBodies.cpp` (the capsules on the bones), the living in
  `SweepBlade` and `StepBladeContact` (`BladeLiving`, `BladePassLedger`,
  `HitStopShare`); settings `WeaponStopsAtBodies`, `WeaponHitStop`.
  controls-spec 4.32 has the details.
- Differences from section 4 C:
  - The contact does **not** replace the bound-sphere test of the strike
    yet: a swing passes the capsules and the strike by motion hits as
    before. The blade only rests on people when it is not swinging. Taking
    the hit from the capsules is the next step, once the capsules' fit is
    seen in the headset.
  - Pressed into someone past the cap, the blade lets go into them (they are
    passed) rather than "through": it still strikes.
  - The hit-stop starts when a swing goes into a capsule, not when the
    engine takes the hit (a light swing's hit is held to the swing's end).
- **Not measured:** the capsules' radii against the meshes; creatures'
  skeletons (only a beggar's was read: 18 of 18 bones); the cost with many
  people near (the walk runs every frame a blade is drawn).
- **Harness:** `blade-body.txt` PASS 2026-10-09 (a lying, frozen beggar: the
  blade rests on them, pressed on goes into them at 21.2 units, a fast
  swing goes in with the strike's hit and the hit-stop). The runs before it
  found a limb crossed between two swept points passing the whole person;
  a pose across someone not passed is now refused (`PersonAcross`).
  `blade-wall.txt` PASS again on that build.

## 11. Built: the parry (2026-10-09)

- `game/ParryLogic.h` (pure, `parry_test`), `game/Parry.cpp` (the two
  reroutes), the cone's answer in `game/BlockCone.cpp`, their blades from
  `game/BladeBodies.cpp`; settings `WeaponParries`, `ParryStopsAll`
  (decision 3: on), `ParryFatigue` (10). controls-spec 4.32 has the details.
- **Read 2026-10-09 (dumpbin on the hit handler 0x005FEBF0):** the target
  is `esi`, the attacker `edi` (`mov edi,ecx` at 0x005FEC3B: the handler is
  a thiscall on the attacker); the block check `call 005E5670` at
  0x005FF7DF is thiscall(target) and answers in `al`; the cone's second
  argument is `[esp+20h]`, the arrow (`ebp`) or else the attacker; the
  share `call 005474A0` at 0x005FF8C7 takes five dwords - the target's
  Block (actor value 0x0F, read through vtable +0x284), its Luck (7), the
  float from 0x005F4880, and two flags - and answers in st0. A
  hand-to-hand block against an armed attacker skips the share (0x005FF885).
- Differences from section 4 E:
  - The blades meeting do not stop the player's blade yet; the engine's
    recoil throws the attacker's back.
  - Their blades are looked at four steps a frame, both moving
    (`BladesMeet`), within 4 units; the window is 0.8 s from the meeting,
    taken by their first blow in it.
  - A block forced for one attacker's parry is answered "not in front" for
    any other attacker's blow in it, so theirs lands.
- **Unmeasured until the harness or the headset says:** when a blow is
  resolved against where the attacker's blade is - every blow at the player
  logs the attackers' blades' distance to the eyes and to the player's
  blade ("Parry: a blow at the player ...").
- **Shields** (section 4 F, ours): the worn shield's ball parries as the
  blade does (`ShieldBall`, `BladeMeetsBall`). Their shields stopping the
  player's blade is not built.

**Open bug (the tester's run, 2026-10-09): the strike hits where the blade
never touched.** A power swing struck an NPC by the strike by motion's
sphere (the blade 52 units from the bound's centre; 0.7 of the radius 90
plus 8 = 71 allowed) and no capsule was entered - no "a swing went into
someone", no hit-stop. The sphere is far wider than the body the capsules
draw. Taking the hit from the capsules (section 4 C) would make the hit,
the hit-stop and the blade's rest agree; not built yet.

**Measured 2026-10-09 (`blade-parry.txt`, PASS - the hooks ran on 24 blows
at the player): Oblivion's blows land with the attacker's blade far from
the player.** A melee bandit attacking a player held still: at every blow
its blade's tip was 131 to 191 units (1.9 to 2.7 m) from the player's eyes,
its torso 140 to 165 units away - the reach fCombatDistance 128 (logged)
times the weapon's 1.0, from body to body. The player's blade circling 30 cm
out came no nearer than 110 units to theirs; no parry happened. A player's
blade reaches about 110 to 120 units from the eyes (a hand 0.6-0.7 m out and
a 67-unit sword), so a parry by touch needs the arm stretched out to meet
their swing at its far end, before their hit key - rarely, if at all. This
is the same thing the Skyrim parry mod works around ("enemies have some
cheating attack animations"), and worse: here the blade never comes near.
Ways on (not decided, see the tester):
- **A. Bring the attackers in:** their blows land from within
  fCombatDistance x reach; a smaller runtime value (never written to an INI)
  would bring their swings within the player's reach. It is the player's
  vanilla reach too, and other combat settings may decide how close they
  stand - unread.
- **B. A wider parry:** their blade within 40-60 units of the player's,
  not 4: a parry that asks for the blade put towards their swing rather
  than on it.
- **C. The guard on the line:** their attack parried when the player's
  blade lies across the line from their weapon hand to the player's chest
  during their swing - the blade held in the way.

**Built: option A (the tester, 2026-10-09: "A, mach erst den Harness-Test").**
- Tried first by the console (`setgs fCombatDistance 85`, a hand script of
  its own): the bandit struck from about 1.3 m (its torso 80-110 units off),
  its blade's tip 24-55 units from the eyes at each blow, and the circling
  blade parried ten blows - "blocked 1.00 of it (the engine's own share
  0.03, Block 10)", Block rising to 11.
- Then built (`game/CombatReach.h`, `combat_reach_test`): `[Hands]
  CombatReach` (85; 0 the game's own) holds fCombatDistance (0x00B36F20,
  checked by the name kept after it) while Full VR runs and gives the game's
  own back when it stops; a value set by anyone else meanwhile is taken as
  the game's own. A game setting changed at runtime is not kept in the
  savegame (cs.uesp.net/wiki/Con_SetGameSetting) and is no INI setting.
  `blade-parry.txt` with it: PASS, parries with "blocked 1.00".
- It is the player's reach as well: the strike by motion's test takes
  reach x fCombatDistance as its blade's length, so the player's blows now
  reach 85 units rather than 128 - nearer the 67 the sword is drawn.
- Not measured: creatures (their bites reach by the same setting), and how
  NPCs fighting each other look at 85.

## 12. The slow approach (built 2026-10-09)

Fighters run up as in vanilla and walk once within `[Hands]
SlowApproachMetres` (5; 10 at first) of the player - the movement-flag setters of the
high process rerouted (game/SlowApproach.h; controls-spec 4.33), harness
PASS. With `CombatReach` 85 (section 11) they then walk the last metres
and stand 1.2 m off to strike, which the tester felt as "die wollen einen
wrestlen" - the reach is the knob, 85 being what lets a parry meet their
blade.

The same setters hold a fighter still while the off hand holds them
(physical-combat-spec section 8), and the parry's forced block stops the
blows of one held by the head, the neck or the weapon arm.

## 13. Open (the tester, 2026-10-10, for later): the blade against shields and the other blades

The tester: "notiere für später waffen kollisionen auch mit schild. also
eigene waffe kollidiert mit eigenem schwert und gegnerischen
schwert/schild." Nothing built; what each part means and what exists:

- **The player's blade against the player's own shield.** Read as the own
  *shield* ("eigenem schwert" in the note - Oblivion gives the player one
  weapon, so there is no second sword of one's own to meet; to confirm
  with the tester). Today the drawn blade passes through the shield on the
  left arm: the contact step (section 9) casts against the Havok world, and
  the player's own shield is in no layer the blade's rays meet (section 4,
  "Opponents' weapons and shields are not in the Havok world either" - nor
  is the player's). The shield's ball exists already (`ShieldBall` on
  "Bip01 L ForearmTwist", the parry's); a blade capsule against that ball,
  as a stopper in `SweepBlade` with the left arm's pose, is the way.
- **The player's blade against an opponent's blade.** Today blades meeting
  during their attack are a parry (section 11): the blow is blocked, but
  the player's blade passes through theirs. Physically stopping on it
  means their blade (`BladeFoe`: `a`..`b`, the frame before's too) as a
  stopper in the same sweep - a segment-segment cast (`SegmentSegmentDistance`
  exists), with the slide and the let-go of section 9. Their blade moves,
  so a rest on it is a rest on a moving thing; the parry's 4-unit reach
  would stay the trigger for the block.
- **The player's blade against an opponent's shield.** Section 4 F: their
  shield's ball or box from their "Bip01 L ForearmTwist" node's bound
  (`CollectBladeBodies` reads their bones already), our blade stopping at
  it, and a blow that meets it no hit. Still open since the spec was
  written.
- Order proposed: their shield first (it also fixes a strike landing
  through a shield, a real fairness bug), then their blade, then the own
  shield. (Still open after section 14: that section is the hands and the
  own shield against the WORLD, not the blade against the own shield.)

## 14. Built: the free hand and the worn shield at walls and on people (2026-10-11)

The tester, 2026-10-11: "Gib nun einhand Waffe und Schild oder freier Hand
Kollisionen. Same für alle anderen Waffen mit der freien Hand." Read as:
with a one-hander, the shield on the other arm - or that hand, bare - meets
the world as the weapon does; with every other weapon, the free hand does.
Sections 4 F ("our shield against walls") and 4 H ("the hands at walls")
of the design, built on the same step. controls-spec 4.32 has the details;
here what differs from the design and what was measured.

- **One step, three shapes** (`game/HandContactLogic.h`, `StepHandContacts`
  in CameraHook.cpp): `StepBladeContact` is run for each hand the weapon is
  not drawn in, with the hand's span (the wrist to the fingertips, as its
  Havok body has it) or - with a shield on the off hand's arm - the shield's
  disc as two spokes (`BladeContactFrame::span2`, a second span the whole
  step sweeps alongside the first) and the shape's own margin
  (`marginUnits`: a hand 2.5, the disc half its thickness, 1.5 to 4). The
  settings `[Hands] HandsStopAtWalls` and `HandsStopAtBodies`; the let-go
  caps are the weapon's.
- **Who is free** (`FreeHandShape`): the off hand with a one-hander, a
  two-hander off its handle, a staff; both hands with nothing drawn or the
  fists up. Not: the weapon hand with a swung weapon drawn (the blade's),
  either hand with a bow drawn, a hand on a handle, round a held object,
  leading someone, holding a fighter. The staff and the torch themselves
  are not swept (only the hand is) - open, like their blades and shields.
- **Through people** at the shove's speed (1.3 m/s) or in a fist's swing;
  below it the hand rests on them (decision 2 carried over). The shove, the
  slap and the grab are untouched: they run on the same hand pose after
  the step - a hand resting on someone is within the grab's 6 units.
- **The shield's disc from its vertices**, not the bound's ball: the ball
  (the parry's `ShieldBall`) would hold a shield facing a wall a hand's
  length short of it. The vertices under "Bip01 L ForearmTwist" are read
  into the hand's frame; the principal axes of their scatter give the plane
  (the normal is the least spread) and a second pass the reach along each
  spoke. Measured on the Arena Iron Shield: 695 vertices, spokes 20.9 units
  (the bound 21.0 round), 13.1 units thick over boss and handle - so the
  margin is capped at 4 units rather than 7.5. A model that cannot be read
  falls back to the ball, swept as one point, and says so.
- **Not written back into anything but the hand's pose**: the shove, the
  grab, the fist's strike, the push, the Havok hand body and the pin take
  the hand where it stopped; the pick, the laser and the reach, which run
  earlier in the frame, take the controller's own pose (as for the weapon).
  A fist that went through a wall strikes nothing until it is free.
- **Harness** (docs/hand-script-harness.md): `hand-wall.txt` PASS
  2026-10-11 (both hands met the floor, held 3 units above it, let go at
  31.8 and 33.2 units past the 21-unit cap, free again; 2-3 rays and 0.011
  ms a frame), `shield-wall.txt` PASS 2026-10-11 (sword and shield: the
  blade's step had the right hand, the shield's rim met the floor, pressed
  on 12 units, let go at 22.7, free again).
- **Open**: the torch and the staff as shapes; the headset's judgement of
  the feel.

## 15. Built: the stoppers - the shield at the blade, the blade at the hand, the foes' blades and shields (2026-10-11)

The tester, after playing 287bcd1: "schild kollidieren nicht mit schwert.
hand nicht mit waffe." Section 13's three parts and section 4 F's "theirs",
built on one mechanism: `BladeLiving::stoppers` (capsules) and `::discs`
(game/BladeContactLogic.h) are shapes of OBVR's own the sweep holds the
blade or a hand at whatever its speed - the living are passed by a swing,
these never - and that are passed only once pressed past the cap, through
the same ledger by an id of their own, so the blade keeps striking (a shield
arm swung across the blade does not disarm a swing). controls-spec 4.32 has
the details. What was measured and what differs from the design:

- **A disc, not a ball**, for the own shield and the foes' (the ball of the
  parry would hold a blade a hand's length short of a shield it faces): the
  read disc (section 14) carried by the left hand's pose - for a foe by
  their twist bone's own transform, their model read the same way
  (`ReadShieldDisc` on "Bip01 L ForearmTwist", once per bound change or 90
  frames). Round at the longer spoke: a kite shield's short sides stop a
  blade a little early rather than let it through.
- **The foes' blades** are the parry's own list (`BladeFoes`), 2.5 units
  round, so a parry and a stop happen on the same contact: the parry's
  4-unit reach is met while the blade is held 2.5 + 1.5 units off theirs.
  Their blade moves; the blade rests on a moving thing and is carried a
  little - not judged in the headset.
- **The foes' shields** stop the blade whether or not they are blocking; a
  blow that meets the shield never reaches the body (4 F's "physically
  right, and a change from today"). The strike by motion's sphere test is
  still the hit test: a swing held at a shield has its hand go on, and the
  sphere test is on the DRAWN blade, which is held - so the blow does not
  land through the shield unless the hand goes past the cap.
- **The sweep's order** (found by the unit test, 2026-10-11): each swept
  point meets the stoppers and the discs first, then the living - a person
  behind a stopper the point is held at (their blade, your shield) is not
  reached, so a swing stopped at a foe's blade does not pass (hit) the body
  behind it. Before, the living were taken in the same pass whatever held
  the point short of them.
- **Open**: the torch and the staff as shapes (they pass through); the
  headset's judgement.

## 16. Open (the tester, 2026-10-14 evening): collisions he misses

After the evening's play, noted for later ("nichts coden!"), each against
what the code claims:

- **"kollisionen mit gegnern fehlen beim schlagen."** Section 10 built the
  living as capsules the blade meets, with the hit-stop (`WeaponStopsAtBodies`,
  default on; a swing passes through, a slow blade rests on them). What the
  tester misses when he strikes is not read yet: whether the hit-stop is too
  short to feel (`kHitStopFrames`), whether a fast swing passing through is
  what he means by "no collision", or whether the capsules miss the body
  parts he hits. His sessions' logs carry "Hands: struck" lines for every
  blow that landed and no contact lines to compare against; to be looked at
  with him in the headset, with the blade's step's trace on.
  **Built 2026-10-15 (section 17):** the living hold a swing, as Blade &
  Sorcery's bodies stop a blade.
- **"kollisionen mit linker hand fehlt."** Section 14 built the free hand
  against walls and people (`HandsStopAtWalls`, `HandsStopAtBodies`, a hand
  held there slowly rests on a person, a fast hand goes through). The left
  hand striking goes through by design; what he expects of it - the
  punch's stop, the slap's - is the same question as the left fist that does
  not hit (physical-combat-spec section 8, open bug).
  **Built 2026-10-15 (section 17):** the living hold a fast hand too, the
  blow still landing.
- **"kollision mit eigenem körper fehlt."** Nothing of OBVR's own stops the
  hands or the blade at the player's own body: the IK body's capsules
  (BodyIk's bones, player-body-spec) are not among the stoppers of section
  15, and the Havok world has the player's character controller only as a
  capsule the hands' layer no longer collides with (section 9, layer 23).
  A proposal: the own body's trunk and legs as capsules on its bones, as the
  foes' are, as stoppers for the free hand and the blade - held, never
  passed - so a hand cannot be put into one's own chest. Unmeasured.

## 17. Built: the living hold a swing and a punch, as Blade & Sorcery's bodies do (2026-10-15)

The tester, 2026-10-15: "ok wir bauen: Linke Hand + Gegner beim Schlagen.
mach es wie in blade and sorcery. analysier erstmal wie die es da machen be4
du startest." Section 16's first two items.

### 17.1 How Blade & Sorcery does it (read 2026-10-15)

Read on the official SDK wiki (kospy.github.io/BasSDK: the pages Damager,
Damagers (JSON), Collider Groups, Items - Best Practices, Item, Ragdoll,
Ragdoll Part, Ragdoll Hand) and in the developer's replies on the Steam forum
(The Baron, 2019). DeepWiki has no index of KospY/BasSDK; Context7
(/kospy/bassdk) carries the JSON overview only; the game's code (ThunderRoad)
is closed.

- **Everything is a rigid body in PhysX.** An item is a Rigidbody with the
  JSON's mass and drag and primitive colliders - "Even vanilla weapons in
  Blade and Sorcery use capsule colliders for all bladed weapons" (Items -
  Best Practices). A creature is a Ragdoll of Ragdoll Parts: each "requires
  a Rigidbody and CollisionHandler on the same gameObject" with a Collider
  Group of simple shapes, a mass (`ragdolledMass`, `handledMass`) and a
  CharacterJoint to its parent part; the hands are parts too (Ragdoll Hand:
  a touch collider, a palm collider, a simplified collider, finger
  colliders).
- **The hand follows the controller through spring-damper drives with a
  force cap.** The Ragdoll's `playerArmPositionSpring`,
  `playerArmPositionDamper`, `playerArmMaxPositionForce` and the rotation
  triple ("the spring strength applied to the ragdoll's arms when they are
  controlled by a player", "the maximum position force for player-controlled
  arms"); the options book exposes them - a player: "There is for positional
  and rotation, each a damper and a spring. The spring is a spring between
  your physical hand and your VR hand"; the developer: "people can change
  the physics spring and damper settings to make the 'weight'", and "the
  weapons are not 1:1 with real life in order to solve the impact issue". So
  a hand or a held weapon that meets a body is stopped by the collision
  while the controller goes on: the capped force cannot push it through,
  and the visible hand stays with the weapon.
- **The blow is the collision itself.** A Damager (blunt, slash or pierce,
  one per collider group) reads the impact: `velocityDamageCurve` turns the
  impact velocity into damage, impacts under `minSelfVelocity` are ignored,
  the effect scales between `intensityMinVelocity` and
  `intensityMaxVelocity`, `addForce` pushes the part hit (with ragdoll
  multipliers), `hitDelayByCollider` keeps one collider from counting twice.
  A slash sinks its edge in to `penetrationDepth` ("from center to the edge
  of the blade") along `penetrationLength` (the blade's) and may slide out
  (`penetrationAllowSlide`); a pierce goes in to its depth; blunt has
  neither. A body is passed only by dismemberment
  (`dismembermentMinVelocity`, parts with `sliceAllowed`) - or entered by
  penetration, the stab of physical-combat-spec section 9.
- **When the controller runs away from a blocked weapon** the player
  chooses: the "stuck object behavior" `block` or `drop` (the developer,
  2019: "set the stuck object behavior to 'block' and not 'drop'") - the
  hand stays with the weapon, or lets go of it.
- **Not read, so not claimed:** the drives' default values; whether the
  hands collide with the player's own ragdoll (the Ragdoll has
  `allowSelfDamage` for its own parts; the pages say nothing of the player's
  hands against the player's body); the code of the collision handling. I
  could not verify these.

### 17.2 What is the same here, and what is not

- **The capsules on the bones are the ragdoll parts' colliders, the sweep
  is the collision, the hold is the body stopping the rigid body.** The
  living become what the stoppers (section 15) already were: held at any
  speed. The hand is not physical here, so the force cap and the
  `block`/`drop` choice have one stand-in, decision 1 of section 8: pressed
  on past `WeaponLetGoMetres` (0.6 m) the blade or the hand goes INTO the
  person - they are passed, the blade still strikes (not "through" as at a
  wall), and the stab's path stays open.
- **The blow stays the engine's.** The strike by motion (the bound's sphere,
  0.7 of its radius plus 8 units) for the weapon; the shove, the slap and
  the punch by the controller's speed for the hand (read from SteamVR's
  velocity, so a hand held at a body still reads its speed). B&S's velocity
  curves and `addForce` have no counterpart; the engine's own damage and
  stagger apply.
- **No sinking of the edge** (B&S's slash depth): the blade is held its
  half-width (1.5 units) short of the capsule's surface, the hand 2.5. **No
  dismemberment, no penetration** (physical-combat-spec section 9, open).
- **Off** (`SwingsStopAtBodies=0`, `PunchesStopAtBodies=0`) everything is
  as before: a swing or a fast hand goes into them and passes them, the
  hit-stop slowing the swing.

### 17.3 Built

- `BladeLiving::hold`, `BladeContactFrame::bodiesHold`
  (game/BladeContactLogic.h): with the flag the living are taken like the
  stoppers - `SweepBlade` does not pass them for a swing, `PersonAcross`
  refuses a pose across them while swinging too. Everything else of the
  step is unchanged: the margin, the slide, the let-go past the cap INTO
  them, the ledger (passed until out of them for 0.22 s), someone who walks
  into the blade or whom the blade starts inside is passed.
- `[Hands] SwingsStopAtBodies=1` ("Swings stop at people") for the weapon's
  step, `PunchesStopAtBodies=1` ("Punches stop at people") for the hands';
  `WeaponStopsAtBodies` and `HandsStopAtBodies` stay the switches for
  meeting people at all.
- CameraHook: a swing held by someone is felt by its speed
  (`BladeTouchAmplitude`, as a stopper is; a blade laid on someone slowly
  stays the light pulse) and logged "Contact: a swing is held by <actor> at
  N m/s (n near) - pressed on N units past them the blade goes in"; a fast
  hand "Contact: a fast <hand> is held by <actor> ..."; the "rests on" line
  follows with the point.
- `[Debug] ContactTrace=N` (default 0): N lines of "Contact: trace - ..."
  for the weapon's and the hands' step, frame by frame while people are
  near and the shape swings, is held or goes in - the tip from and to,
  where it was drawn, held by whom (or which body), entered, event, gap,
  through, passed. Written for the harness runs below; it found both
  scenarios' faults.
- Tests: `blade_contact_test`, "The step: the living hold a swing" - 16
  flows: the default off; a swing held, not passed, no hit-stop, the tip the
  margin short; swung on within the cap; pressed past the cap into them,
  staying in unheld, cleared after a third of a second, held again; the slow
  rest unchanged; someone walked into it passed; a thin arm between two
  points held (without the hold the swing goes on); their blade before their
  body; the cross-piece (span2); a wall behind them not reached.
- **Harness** (docs/hand-script-harness.md): `blade-held.txt` PASS
  2026-10-15 - a beggar 70 units ahead, frozen, a longsword drawn and
  brought slowly level to 45 cm out; four slashes across them at 4.5 m/s:
  the first two held by them (at the guard's end at the slash's start, then
  at the tip at 4.1 m/s), each strike landed ("the blade met", "struck"),
  the hand going on took the blade into them at 42.4 and 43.3 units
  ("pressed on into"); the strikes' stagger moved the beggar 19 units back
  each time, so the last two slashes reached him no more (the strike needs
  the bound only, the hold the capsule). `fist-held.txt` PASS 2026-10-15 - a
  beggar 75 units ahead, a longsword drawn, the left fist with the grip
  squeezed punched 85 cm at 4.5 m/s: held by them 0.2 units from the
  controller at 4.3 m/s while the punch landed hard (3.5 m/s, "the off
  hand's punch, a weapon drawn") and knocked them down; the fingertips
  trail the controller by 4 units, so a punch 72 cm long fell 1 unit short
  of their chest in the run before. `left-punch.txt` PASS again (5.7 m/s,
  hard; the moving bandit's capsules not touched).
- **Found on the way:** a hand's jump to the holster and back is a swing of
  its own (28 m/s) - with the beggar in reach it was held by him before the
  scenario began; a jump of 0.37 m to a slash's start is one too and knocked
  him back before the first slash. The scenarios now move there slowly.
- **The cap at people, of its own (the tester, 2026-10-15: "bau die eigene
  grenze für personen, default 0 wie B&S block"):** `[Hands] BodyLetGoMetres`
  ("Lets go at people (m)"), default 0 - a person holds the weapon or the
  hand for good, however far or how turned the controller goes, until it
  comes back and the shape comes free; set, the shape goes into them past
  that distance (passed, still striking), as walls let go at
  `WeaponLetGoMetres`. `BladeContactSettings::bodyLetGoUnits`; the let-go
  check asks who holds: a wall or a stopper keeps the weapon's cap (and its
  angle cap), a person theirs. Unit flows: held 86 units on, turned 69
  degrees, free when the hand comes back, resting slowly for good, the wall
  and the stopper still letting go; a four-aligned test id, since an odd id
  reads as a foe's stopper.
- **The knockback of the player's hits (the tester, 2026-10-15:
  "problematisch wird auch ob wir dann gegner wegschieben mit schlägen. das
  wäre ein fehler", then "bau mal alle vier optionen mit toggle"):** what
  moved the beggar 19 units a blow was the engine's own hit knockback
  through the Havok character proxy (PlayerStagger.h), not the hold. `[Hands]
  EnemyKnockback` ("Hits push enemies (%)", default 0: none, as a B&S body
  stays where it stands; 100 the game's own; between, the push's vector
  scaled) and `EnemyKnockbackPowerOnly` ("Only power attacks push", default
  0). game/HitKnockbackLogic.h (`HitKnockbackShare`, hit_knockback_test: 17
  flows), PlayerStagger.cpp: the proxy call 0x0060008A now runs through a
  stub that hands the attacker (the handler's `this`, edi) and the power flag
  (the handler's first argument, [esp+0x1E4] of a frame esp never leaves)
  along; a hit to push nobody answers no proxy, one to push less leaves its
  share for the push call 0x006000AF, rerouted to scale the distance. A
  strike by motion hands its own power flag over (`SetMotionHit`) and the
  handler's reading is checked against it once ("Comfort: the hit handler's
  power flag read ... agrees"). Hits on the player stay NoPlayerStagger's,
  other fighters' hits the game's own. **Run 2026-10-15 afternoon:** the
  first motion hit's line read "the hit handler's power flag read 0 for a
  light strike by motion - the stack reading agrees" in every scenario; at
  0 % the knockback was skipped at every hit (the beggar stood at the same
  spot through four strikes, 26.0 to 22.2 health), at 100 % kept (19 units
  back a hit, as before).
- **Open:** the headset's judgement of the feel; the edge's sinking; a push
  on the person held (B&S's `addForce`, now that the engine's is off by
  default - a stumble of their own); the harness runs below.
- **Harness** (docs/hand-script-harness.md), all PASS 2026-10-15 afternoon:
  `blade-held.txt` with the defaults (held for good, the knockback off) -
  the beggar stood at the same spot through four slashes, every strike
  landed, "held until the hand comes back", nothing pressed into them; at
  slash 3 no new hold was logged because the blade was still held from
  slash 2, stuck on their far side while the hand came back round - the
  hold for good as designed. `blade-held-cap.txt` (BodyLetGoMetres 0.30,
  EnemyKnockback 100) - the old way: pressed on 21.0, 21.4 and 24.0 units
  the blade went into them, each hit moved them 19 units (a first run
  failed on the script's own contradictory expectations, inherited from
  blade-held). `fist-held.txt` - the punch held 2.8 units from the
  controller at 4.3 m/s for good, landed hard at 6.7 m/s (the knockdown is
  the shove's own, untouched by EnemyKnockback).

## 18. The target: Blade & Sorcery's combat, rebuilt (the tester, 2026-10-15)

"neues zielbild: nachbau vom B&S combat in OBVR." Section 17.1 has what B&S
does; this is where OBVR stands against it, element by element, to steer
what comes next. physical-combat-spec section 10 carries the same list for
the body and the hands.

| B&S element | OBVR | Status |
|---|---|---|
| A body stops the blade and the fist, the controller goes on | the living hold swings and fast hands (17), held for good by default (`BodyLetGoMetres` 0), the shallow band (19) | built, headset open |
| The hand follows the controller through a capped spring; a stuck weapon: block or drop | the held pose written back, the hand drawn where it stopped; no spring, no drop | built, as a hold |
| The collision is the blow, damage by impact velocity | the strike by motion by touch of the capsules (19), light or power by the swing's speed; the shove by the hand's speed | built; no velocity curve |
| Impulse on the part hit, the body stays | the engine's knockback off by default (`EnemyKnockback` 0), scalable; no impulse on the part | built as off; a stumble open |
| A slash sinks its edge in and slides out | held the half-width short of the capsule; the slide along it | partly |
| Penetration: a thrust goes in and sticks | nothing | open (physical-combat-spec 9) |
| Dismemberment | nothing yet; the killing blow cuts by bone pointers (dismemberment-spec.md) | designed 2026-10-25 |
| Hit locations, damage by part | nothing; the part is known (`BodyPart`) | open (physical-combat-spec 9) |
| Blades and shields meet: a parry stops the blow | the parry (11), the torch's too (22), the stoppers (15), the block by angle and reach | built |
| The hands and the weapon against the own body | nothing (16) | open |
| Weapon weight and inertia | `WeaponWeight`, the swing-through spring | built |
| Grabbing a fighter, throwing, the ragdoll hold | the grapple, the ragdoll grab (physical-combat-spec 8) | built, the weight open |

## 19. Built: the tester's headset notes of 2026-10-15 - through the body, the fist, the grab

The tester, after a session on the morning's build (his OBVR.log of 11:31):
"1. ja geht aber manchmal auch direkt durch den körper... 2, same 3. nein
linker pinky trackt nicht daher kam keine faust. griff: gegner sackt in sich
zusammen 4. 100 oder 0 nix passiert, ist whl ok so" - then "Ja 1 + 2 + 3 + 4
knockback sollte schon gehen wenn das an ist."

### 19.1 Read in his log

- 20 "the blade met" lines, 12 with a hold within eight lines, 8 without -
  the blade 29 to 79 units from the fighter's bound centre at those. No
  "went into someone", no "pressed on into": the hold never passed anyone
  by its own rules. The fighter's bound radius read 87 to 116, so the
  strike's ball (0.7 of it plus 8) landed hits with the blade up to 1.1 m
  from his centre - swings through the air beside him took health.
- The left hand's only finger line: index 0.50, middle 0.00, ring 0.01,
  little 0.00, SteamVR's own animation 0.13 to 0.18 for the three. Three of
  four fingers never read, and the fist's rule leaves fingers out only
  when at least three have read - so all four counted and no fist came;
  all seven of his left blows were the open hand's.
- The grab: the known picture of section 8 of physical-combat-spec - the
  take knocked the fighter into his ragdoll, the pull's budget cut every
  change, the body 25 to 27 units behind the hand, lost after 0.3 s.
- The knockback: at 100 % he had "Only power attacks push" on, so his
  light hits were skipped there too ("skipped (Hands.EnemyKnockback 100 %,
  power only 1)"); his one power attack pushed, unlogged. The comparison
  was confounded, not the mechanism.

### 19.2 Built

- **The fist by the grip's force** (`[Hands] FistGripForce=0.6`, "Fist by
  grip (force)"; `game::HandBodyFist` with the grip force and the setting,
  `kFistGripRelease` 0.15 of hysteresis; HandMode carries each grip's
  force, an Index's rAxis[2].x). A grip squeezed to the force is a fist
  whatever the fingers say, and stays one until the force drops 0.15 below;
  the fingers' rule is unchanged beside it. Logged a few times per hand
  when the grip alone made the fist ("Hands: the left hand's fist by the
  grip's force ..."). hand_body_test: ten flows. Not provable in the
  harness (the synthetic controller has no grip force): the tester's next
  session.
- **The hit by touch** (`[Hands] MotionHitsByTouch=1`, "Hits need the
  touch"): a swing or a punch hits only where the blade or the fist comes
  within `touchUnits` (4; a fist 6.5) of one of the actor's capsules - the
  list the contact steps collected this frame (`MotionStrike::capsules`,
  the weapon's or the hands'), `game::CapsulesTouched` on the drawn blade's
  OWN span (the weapon's geometry at the drawn hand, as the contact step
  held it - a longsword's axis is 25 degrees off the hand's pointing axis,
  so the strike's blade along that axis missed a capsule the held weapon
  touched) or, for a fist, `kFistTouchUnits` (12) of itself. An actor with
  no capsule in the list is not hit (logged a few times); no list at all
  (the contact settings off) or the setting off: the bound's ball as
  before. Log "Hands: the blade touched <actor> - the hit by touch, within
  N units of a capsule (the drawn blade's own span)".
- **The shallow band** (`kBladeShallowUnits` 3, at most half a capsule's
  radius; `ShallowBand`, `CapsuleDepthAt`): a shape only just inside a
  person is held at them, not passed - in the sweep (a point starting in a
  capsule within the band is held where it is, the normal out of the
  capsule), in `PersonAcross` (across only past the band) and in the
  lying-in pass (passed only past the band). Found by the walked-into log
  on the harness's frozen beggar: his idle moved the torso's capsule over
  the blade held 1.5 units short of it, and the blade passed him. The
  thin-limb protection stays: a blade through a 2-unit arm's axis is 2
  deep, past the arm's band of 1. blade_contact_test "the shallow band".
- **The walked-into log**: "Contact: <actor> walked into the blade where it
  was (n near) - passed until ..." whenever the ledger grows without a swing
  going in, a let-go, a start inside or a blade through a wall; the hands
  the same. The stoppers' ids are left out.
- **The grab** (physical-combat-spec 8, ways B, C and D): the loss as a
  throw - a hand that outruns the body at the shove's speed throws it (the
  ragdoll: with the hand's velocity times GrabThrowStrength; standing: as
  the grip opening would, thrown or knocked down), a slow one loses it as
  before; `kGrabLiftMassAtHundred` 300 to 1200 (a Strength-25 hand's budget
  5518 to 22073, lifting 75 to 300 of a 530-mass ragdoll); `kGrappleReachUnits`
  6 to 14 (his misses were at 7.4 to 13.7). grapple_test: the flows and
  the constants.
- **The knockback kept whole** logged ("Comfort: the knockback of <actor>
  by the player's light hit kept whole (Hands.EnemyKnockback 100 %)") so a
  100 % run reads in the log.
- **Diagnosis**: the live INI carries `[Debug] ContactTrace=4000` for his
  next session.

### 19.3 Harness (2026-10-15 afternoon, Linux)

- `left-fist.txt` PASS - the beggar 55 units ahead now (the fist has to
  touch): three laps, three hits by touch ("the fist touched ... within 4.0
  units of a capsule"), light 26.0 to 25.1 and two power strikes to 20.4,
  the beggar standing where he was (the knockback off).
- `blade-held.txt` PASS - four slashes, four hits by touch on the drawn
  span (the blade 28 units from the centre at each, against 35 to 56 by the
  ball before), four holds, the knockback skipped at each, the beggar
  standing where he was; a first run with the touch on the hand's axis hit
  nothing - the weapon's 25-degree offset.
- `fist-held.txt` PASS - the punch held 2.8 units from the controller at 4.6
  m/s, landed hard.
- `grapple-ragdoll.txt` PASS - two takes, the torso at -2.5 units and the
  right arm at 10.1 (past the old reach of 6); the pull's budget 22073
  lifting 300; the torso 25 to 38 units from the hand over 0.8 s (the loss
  at 40 - close), the arm 3.5 to 18; both let go by the jump back as throws
  at 22.7 m/s; no loss.
- `left-punch.txt` PASS - four punches at the moving bandit, 3.4 to 6.7 m/s,
  all hard. `blade-held-cap.txt` from the morning not run again on this
  build.

### 19.4 Open

- The headset's judgement of all of it; the first "fist by the grip's
  force" lines with his force values (the 0.6 is a guess, the row tunes it).
- The torso's lag in the ragdoll hold (25 to 38 units of the 40 that lose
  it): way A of physical-combat-spec 8 - no loss while held, the body
  dragged - or a lower GrabWeight.
- The capsules' radii against the meshes (10 units of torso), still
  unmeasured.
- **Open bug (artemis, beta 6, 2026-10-22): the hold at people for good keeps a
  hand the player walks or turns away from - the body IK's arm stretches to it
  ("sticky hands ... like a piece of spaghetti").** The release asks only
  whether the sweep from the held hand to the controller still crosses the
  person, never where the controller is, so a controller carried round them by
  the stick keeps the hold. The cause, the relief (`BodyLetGoMetres=0.3`), the
  options and the harness plan are in controls-spec.md, the beta report of
  2026-10-22; the pick is option A: the hand's hold ends when the controller is
  outside the person, the blade's hold for good kept. **Built 2026-10-23,
  section 20.**

## 20. Built: a person holds a hand only while the controller is inside them (2026-10-23)

The Quest 3 tester's beta 6 report (2026-10-22): "'Sticky' hands. If you
approach a character and touch their body, your hand sometimes (actually,
quite often) gets 'stuck' to them. When you try to move away, your arm
stretches out like a piece of spaghetti!" - his picture a guard standing idle
and the sleeve drawn across the picture to the hand at his belt.

- **The cause, read:** section 17's hold at people for good (`BodyLetGoMetres`
  0) let go only when the sweep from the held hand back to the controller no
  longer met the person. The release never asked where the controller was.
  Walking or turning away on the stick carries the controller past them or
  round them, the line from the held hand still crosses them, the hand slides
  along their surface and stays, and the body IK draws the arm out to it. The
  harness showed it (`hand-held-walk.txt`, 2026-10-23 10:53, the build before):
  the hand rested on a frozen beggar, pressed 10 cm in; the player walked
  forward-right past him and the hand stayed held with the gap to the
  controller at 9.7, 41.3, 42.8 and 44.3 units, the drawn tip fixed on his
  chest, until the pass ledger took him ("walked into the left hand where it
  was") and the hand went on passed.
- **Built:** `BladeContactFrame::holdWhileInside` (BladeContactLogic.h): a
  person holds the shape only while the wanted shape - the controller's - is
  inside them, one of its swept points within the margin of one of their
  capsules (`ShapeInsidePerson`). Outside, the hold ends where the controller
  is: a new event, `BladeContactEvent::CarriedOff`, nothing passed, so the hand
  meets them again the moment it comes back; pushed clear through them it comes
  out on their far side, as Blade & Sorcery's hand does when forced. The hands'
  step sets the flag (CameraHook, the hand and the worn shield alike); the
  weapon's step does not, so the blade keeps section 17's hold for good. A cap
  at people set (`BodyLetGoMetres` above 0) works as before inside them. Logged
  "Contact: the <hand> came free of <actor> - the controller outside them, N
  units from where it was held".
- **Tests:** blade_contact_test, "a person holds the hand only while the
  controller is inside them" - 16 flows: the default off; nobody near; inside
  within the margin, outside past it; pressed in and held; pressed on with the
  controller inside; carried round them (the controller outside, the line still
  across them): free at the controller, not passed, not through; following the
  controller on; brought back: held again; pushed clear through: free on the far
  side; without the flag the same carry keeps the hold (the blade); the slow
  rest carried off; a set cap inside them unchanged. Written with the fix, green
  on the first run.
- **Harness:** `hand-held-walk.txt` PASS 2026-10-23 10:55 (headless, the build
  14761da3cf90500e): the rest on his chest at 0.65 m/s, pressed in at 1.50 m/s,
  then on the walk "came free of ... - the controller outside them, 16.5 units
  from where it was held" at the first frame the hand's span (15 units, pressed
  10 in) cleared his torso's capsule, the drawn tip at the controller from
  there; no "pressed on into". Headset: open (the feel of the release while a
  hand rests on someone and the player steps aside).
- **Relief for the testers on beta 6**, which has none of this:
  `[Hands] BodyLetGoMetres=0.3` ("Lets go at people (m)") frees a walk away past
  30 cm, by going into them.

## 21. Open wishes (Nadi, Windows, 2026-10-24)

- **A strike to the groin, with its animation** ("Crotch Schlag, Anim"). Hit locations are
  open (physical-combat-spec 9; section 18's table): the part the blade or the fist met is
  known (the capsules on their bones), the engine takes no location (0x005FEBF0) [R]. What
  animation a struck foe could play for it is unread [G: the stagger, or a knockdown; vanilla
  has no groin reaction]. First step: the capsule part named "Pelvis" on a hit logged, and the
  idle and stagger animations a foe can be given.
- **Foes recoil from a pommel strike and a half-sword thrust** (free-weapons-spec 5.6): the
  recoil is the stagger the shove and the knockback already give (`EnemyKnockback`, off by
  default; ShoveActor) [R]; to decide is when a hit without the power flag recoils.

## 22. Built: the torch parries (2026-10-25)

Nadi, 2026-10-25: "mach das man mit torch auch blocken kann". Read as: the torch in the off hand
blocks a blow it is held into, as the blade and the shield do - the parry of section 11, by touch.

- **Not the raised hand.** The left hand raised blocks only with a shield (vr/HandInput.h,
  LeftHandBlocks; a tester, 2026-09-29: "linke hand heben wenn kein schild equiped, darf nicht als
  block zählen"), and a torch is held up to light the way. A raised torch as the Block action would
  hold the player in a block in every dark cave. The parry needs no pose: their blade meets the
  torch while they swing. The pose block for the torch is an option, not built: LeftHandBlocks with
  the torch and a `BlockWith::Torch` in the angle rule (game/BlockAngleLogic.h), the shaft as the
  blade's segment. Cost small.
- **The shaft** (game/TorchShaftLogic.h, `torch_shaft_test`; game/ShieldShape.cpp, ReadTorchShaft):
  read from the model under the left hand's "Torch" node, in the hand's frame, the way the shield's
  disc is - the line of the largest spread, its reach along that line from the vertices within 5
  units of it. The engine hangs the fire on the torch's FlameNode1 (Fire\FireTorchLarge.nif, read
  2026-10-25: a billboard quad 32 by 64 units), and its corners would have stretched the shaft half
  a metre past the head. Lights\Torch02.NIF (four LIGH records of Oblivion.esm) runs 48 units along
  its y axis, from 14 below the grip to 34 above, 5 thick. A shaft 16 to 120 units long, its spread
  across under a quarter of the spread along. Read again for another node or torch and every 90
  frames; carried into the world by the left hand's pose each frame (CameraHook StepTorchShaft).
- **The parry** (game/ParryLogic.h, WhatMeetsTheirBlade, `parry_test`): the blade first, then the
  shield's ball, then the torch's shaft, each from where it was to where it is, within the parry's 4
  units. A shield and a torch are never worn together. The torch parries with a weapon drawn or
  without (`[Hands] WeaponParries`; the foes are gathered round the torch when no blade is drawn).
  It is felt in the off hand, heard as WPNBlockBluntX (wood), and logged "Parry: their blade met the
  torch".
- **Bare fists** (read 2026-10-25, the hit handler's hand-to-hand branch, OBNG knowledge
  blocking.md): with neither a weapon nor a shield equipped the block is hand to hand, and against
  an armed attacker the handler loads 0 as the share at 0x005FF890 without asking 0x005474A0. A
  torch is neither. A parried blow at a torch-and-fists player would have landed whole, as would a
  blow from someone the off hand holds while the weapon slot is empty (derived; section 11 and
  physical-combat-spec 8 were measured with a sword). The compare at 0x005FF87E now goes through an
  18-byte stub (BuildHandToHandStub): a stopped blow goes on to the share, where it is stopped
  whole; any other blow is compared as before.
- **Harness 2026-10-25, Linux, PASS both** (a melee bandit, tgm, CombatReach 85, the torch circled
  round the player at chest height as blade-parry.txt circles the blade):
  - `torch-parry.txt`, bare fists (the state lines read "slot 0", nothing equipped): the shaft read
    at once, 368 vertices, 47.9 units along the hand's y axis (-20.6 to 26.8); "the hand-to-hand
    compare (005FF87E) goes through a stub"; 24 blows, one met the torch, "blocked 1.00 of it (the
    engine's own share 0.01, Block 10)". That line comes only through the stub: without it the
    handler skips the share for a hand-to-hand block against a weapon.
  - `torch-parry-sword.txt`, the Steel Longsword drawn and held back out of the swings: 24 blows,
    four met the torch, each "blocked 1.00 of it". The blows log their distance to the torch since
    this run ("N from the torch"): 0 to 60 units at the blow, a quarter of them within 12.
  - The parries per blow are the scripted circle's, not a player's: a hand that puts the torch
    into the swing meets it far more often. Headset open: the feel, and whether 4 units of reach
    are enough for a torch held into a cut.
