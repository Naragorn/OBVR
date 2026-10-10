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
    meshes; the harness can compare them with the bound.
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
  hand's shield, later.

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
- `ParryStopsAll` (decision 3: on), `WeaponLetGoMetres` (0.30),
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
  shield.
