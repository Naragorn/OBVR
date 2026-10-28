# Holding objects in the hand

Status: **parts 1, 2 and 4 built (2026-09-26), untested in a headset.**
Parts 3 and 5 are proposals. Addresses are Oblivion.exe 1.2.0.416.

## The problem

A grabbed object floats over, in front of or inside the hand, "like a
magician" (tester, 2026-09-26). Three causes:

1. **The hand does not close.** The first-person finger bones take whatever
   the animation gives them, usually an open hand.
2. **The wrong point goes to the wrong place.** The engine pulls the point
   of the object that the grab's ray touched towards a target. The target
   was the controller's tracked origin, which sits on the tracking ring above
   the fingers, not in the palm.
3. **The object hangs and lags.** The engine's spring pulls one point and
   holds no rotation. The object dangles from the touched point, swings, and
   trails a moving hand. It does not turn with the wrist.

## How the engine holds (read in the disassembly)

- **The spring.**
  - The grab's spring is a `bhkMouseSpringAction` (RTTI vtable 0xA3D524,
    wrapper built by 0x0047DE90 at 0x0066D84C).
  - It is stored at player+0x574; the grabbed reference is at +0x578, the
    mode at +0x57C and the distance at +0x584.
  - Its creation info carries the body, a **pivot in body space**, a target
    in the world, damping 0.5 and a maximum relative force.
- **The pivot is the ray's hit point**, turned into body space
  (0x0066D7D8..0x0066D828: hit minus the body's translation, times the
  transposed rotation).
  - With no ray hit the pivot is the body's own position (0x0066D4BC).
  - The grab's start is aimed at the laser's pick hit (AimAtSource), so the
    pivot is the point the laser touched.
- **Each frame** the update (0x0066D930) writes one thing, the target
  (0x00605DC0 → 0x008B8A10 → hk+0x30). Strength and damping are fixed at
  creation.
  - It releases the grab when the pivot gets too far from the target (a
    compare against 96.0 at 0xA73DE0, scaled by 6.999).
  - **The spring has no rotation target**, so no setting of it can make the
    object turn with the hand.
- **Target arithmetic.** The target is the first-person camera node's world
  translation plus the player's forward times +0x584 (the added constant at
  0x00A2FC68 is 0.0). OBVR swaps the rotation and the distance so that this
  lands on a point it chooses (`camera::GrabHoldTarget`).
- **Scene side.**
  - A reference's scene node is at TESObjectREFR+0x3C (vtbl+0x154, 0x00422DE0,
    returns it), its base form at +0x1C, and the form's type byte at +0x04
    (xOBSE GameObjects.h, GameForms.h).
  - The node's world bound radius is at NiAVObject+0x2C.

## The five parts

### 1. Grip pose while holding — built

While the engine holds something (the mode at +0x57C set, the grab key
down), the fingers of the hand that grabbed close:

- Every node under that hand's bone whose name contains "Finger" is a
  finger link. They are found by name, not from a list, and logged once.
- Each link is turned about its own z by `[Hands] GripCurlDegrees` (default
  45, settings row "Grip curl"), on top of the rotation it had when the hold
  began. The thumb (the "Finger0" links) turns half as far.
- When the hold ends, the links get their old rotations back.

Code: `game::HandGrip` (`HandGripWanted`, `FingerCurlDegrees`,
`CurledAboutZ`, `StepHandGrip`). Tested in FrameLogicTest.

- The axis: each link bends about its own z. This was an assumption at
  first (3ds Max Biped's finger axes). It was measured on 2026-09-28 in the
  game's own fist animation (part 1b): +z closes the hand.
- **Limits.**
  - One pose for every object: a pen and a pumpkin get the same fist
    (part 5).
  - Fingers go through the object where the pose does not fit it.

### 1b. Fingers that follow the controller — built, not yet seen running

With nothing held, each finger of a hand follows the controller's finger
(SteamVR's skeletal summary, the curls the fist gesture reads):

- 0 is the game's open hand, 1 the game's fist, and each link in between is
  blended. Both poses are the game's own local rotations, read with pyffi
  from `_1stperson\idle.kf` (open) and `_1stperson\handtohandidle.kf`
  (fist) on 2026-09-28. The left hand is the right's mirror image, x and y
  negated, as the files have it.
- The measurement also settles part 1's assumption: in the game's fist
  every link is turned about +z from the open hand.
- The order of precedence (`game::FingerPoseFor`):
  1. an object the engine holds for that hand closes it (part 1);
  2. otherwise the tracked curls, when `[Hands] FingerTracking` is on
     (default on, settings row "Finger tracking"), the device gives curls,
     and the hand holds nothing of its own;
  3. otherwise the animation.
- "Holds something of its own" means a node under the hand bone, other than
  a finger link, that carries a child: the drawn weapon's "Weapon" node, the
  "Torch" node. That hand keeps the animation's grip on the handle. The
  first transitions are logged per hand.
- When OBVR lets go, a link gets the animation's rotation back, unless the
  animation has written it since.
- The first tracked frame logs the index finger's first link as the
  animation left it, next to the table's open hand and fist. It is a
  constant in both files, so one of the two should match to about 0.003.
  That is the check that the quaternion-to-matrix convention is the
  engine's.
- Code: `game::HandGrip` (`FingerPoseFor`, `FingerLinkOf`, `RotationOfQuat`,
  `BlendQuat`, `TrackedLinkRotation`, `StepHandFingers`, `HandHoldsItem`).
  Tested in finger_test. Scenario: `tools/hand-scripts/finger-tracking.txt`.
- Seen in the headset (the tester, 2026-09-28): open hand, fist, single
  fingers, the sword hand keeping its grip, the grab. The bow hangs on the
  left hand's "Torch" node (harness log), so the bow hand keeps the
  animation.
- **The thumb, joint by joint** (the tester: on an Index the thumb was
  either straight or folded into the fist, "wie ist das in alyx"). The
  summary has one thumb curl, and on an Index a thumb on any button reads
  as a fist's thumb. Now each thumb joint comes from the full skeleton
  (`vr/ThumbPose.h`): SteamVR's bone rotations with the controller's range
  of motion, measured as a share of the way from SteamVR's open hand to
  its fist (`GetSkeletalReferenceTransforms`), per joint, and the game's
  thumb links (Finger0, 01, 02) blended each by its own joint. Without the
  skeleton the thumb follows the summary curl as before.
  - The harness (`tools/hand-scripts/thumb.txt`, the new `thumb` command)
    shows three distinct thumbs: lifted (a thumbs up), its base turned in
    with the rest straight, and across the fingers.
  - **Not yet known:** what an Index actually reports for a thumb on A, B,
    the stick or the trackpad. The log says it (`OpenVR input: right thumb
    joints ...`, the first 80 changes) for tuning after a headset run.
  - A sideways turn of the thumb that does not lie on the open-to-fist path
    is dropped: the game's hand has only its two poses to blend between.
    So the OK sign can only be approximated.
- **Solved, not in OBVR (the tester, 2026-09-28): the left little finger
  stayed straight.** It works once the hand sits closer to the controller
  ("musste näher an controller ran"). The fist's limit is now a settings
  row, "Fist at" (docs/controls-spec.md 4.3), for a hand whose fingers read
  low. The finding as it was: whatever the hand does on the controller ("der linke kleine
  finger/pinky bleibt immer gestreckt egal wie ich den controller
  berühre"). The tester's own guess is old controllers. Nothing settles it
  yet:
  - The log has no per-finger curls, only the thumb's lines, so it is
    unknown whether SteamVR reports the left pinky's curl as 0 (the
    controller or its driver) or reports it and OBVR drops it.
  - Derived from the code, not measured: the pinky takes the same path as
    the other four fingers (summary index 4, links Finger4/41/42, the
    left-hand mirror is the same for every finger). A fault only in OBVR
    would have to be in that index or those link names.
  - Next: log the five summary curls per hand on change, as the thumb's
    lines do, and compare the left pinky with the right. If SteamVR already
    reads 0, the cause is outside OBVR (the controller's capacitive sensor
    for that finger). Otherwise the fault is in OBVR's mapping.
  - Built for the tester's next run (2026-09-28), both hands, so the right
    is the comparison:
    - `OpenVR input: <hand> curls thumb to little ...` - the five device
      curls each time one moves a quarter step (the first 120 changes),
      with SteamVR's own animation curls beside them;
    - `Hands: the <hand> little finger curled ...` - once per hand, the
      first time its tracked curl reaches 0.8: the angle OBVR writes to
      Finger4, 41 and 42 and whether last frame's write was still there.
  - **Measured (the tester's run, 2026-09-28): the cause is the left
    controller, not OBVR.**
    - In all of the 120 curl lines, the left little finger's device curl
      is 0.00. This includes lines where the left middle finger reads 0.73
      and the ring finger 0.62 (the lines around 590 to 600).
    - The right little finger moves with its ring finger: 0.98/1.00,
      0.67/0.33, 0.12/0.40, 0.23/0.64.
    - SteamVR's own animation shows the same thing: 0.12 to 0.15 for the
      left little finger all run long, so SteamVR's hand keeps it straight
      too.
    - When the left little finger did read high once, 0.88 late in the
      run, OBVR bent it like the right's: 64, 75 and 79 degrees (right at
      0.82: 61, 71 and 74).
    - "kept no" on both hands: the animation writes the links each frame
      and OBVR writes over it after, the same on both sides.
  - What remains is the controller's sensor for that finger. Index owners
    report the same and the fixes they give (not tried here):
    - drumming the fingers along the handle, from the little finger up,
      which the controller uses to learn the hand (Valve's
      troubleshooting page,
      https://help.steampowered.com/en/faqs/view/57B5-E574-406A-4E61, as
      quoted in r/ValveIndex e5nndh);
    - turning the controllers on with the hand resting on them unpressed
      (r/ValveIndex kxwyfp, a user's report, unverified).
  - Not built, and only if the sensor stays dead: letting the left little
    finger follow the ring finger when its own curl never moves. This
    would be a workaround for the hardware, not a fix.
- **Open bug (the tester, 2026-09-28): no thumbs up.** The tester's log:
  with the controller's range of motion, the thumb at its most open still
  reads 0.43 0.00 0.25 from base to tip, so its base never leaves the grip.
  Ideas: SteamVR's range without the controller, a third pose (a thumb
  straight up), another reference skeleton. The details are in
  `docs/next-up.md`.
- **Looking into the hand from below (the tester, 2026-09-28: "man kann
  immer noch von unten in die hand schauen"; 2026-10-01, of the bow hand:
  "ich will hier wenn keine rüstung dran ist genau dieselbe hand wie ohne
  bogen") - sealed 2026-10-01.** The bare hand's lid was its own far wall,
  so what lies inside the hand showed through it: the harness's views into
  the wrist (probe "wrist", 15:37) showed the empty hand's one flat lid and,
  in the bow hand, the bow's grip with its dark open end. Now a bare,
  card-skinned hand (its vertex declaration has blend indices) seals its
  opening with the stencil (`render/BackfacePass.h`, "the sealed opening",
  backface_pass_test): the parity of all its faces finds the opening, its
  visible front faces keep their skin, and the flat lid is drawn there over
  what is inside, at the nearest depth, so what is drawn later inside the
  hand fails against it. Only with eight stencil bits (the harness's buffer:
  D24S8). The probe after (15:48) showed the bow hand's wrist sealed; what
  of the bow still shows lies outside the open scripted hand. Not seen in a
  headset. Trade-offs: something drawn before the hand in front of its
  opening (a wall the wrist is pushed into) is drawn over there; something
  drawn after it in front of the opening fails there (the arrow passing
  just in front of the bow hand's wrist). The first try took "a stream in a
  dynamic buffer" for skinned and sealed the bow (its string's morph is
  written into one) instead of the hands.

### 2. The touched point in the palm — built

- The target handed to the engine is now **the palm of the pinned hand
  bone**: the bone's origin (the wrist) moved 7 cm along its own x, which on
  a Bip01 hand runs along the fingers (`game::PalmPoint`,
  `kPalmAlongMetres`).
- `[Hands] HeldObjectMetres` (settings row "Held object distance", now
  meaning "along the fingers from the palm") moves it.
- The spring pulls the touched point there, so an apple lies in the hand and
  a sword hangs at the place that was taken.
- Without a pinned hand bone (PinHands off), the controller's origin moved
  along the laser is used as before.
- **Limits.**
  - The palm is a point on the bone's axis. The hollow of the hand is a
    little to the palm side, which is not modelled. HeldObjectMetres only
    moves it along the fingers.
  - The spring still lags and holds no rotation (part 3).

### 3. Rotation with the wrist — proposal

- The engine's spring cannot hold an orientation. Options:
  - **(a)** Per frame, set the rigid body's angular velocity towards the
    wanted orientation, a "rotation spring" of OBVR's own. This needs the
    hkRigidBody (spring cinfo +4, reachable from the spring at player+0x574)
    and Havok's setAngularVelocity. The entry point is not yet found.
  - **(b)** Keyframe the body while it is held: switch its motion to
    keyframed and drive position and rotation directly. Collisions then push
    others but not the held object. It needs the motion-type switch; the
    engine refuses to grab motion types 6 and 7 at the start, so those are
    the keyframed and fixed kinds.
  - **(c)** Visual only, for small objects: part 4 does this.
- I recommend (c) now, and (a) later for large objects, so a held plank
  follows the wrist and still collides.

### 4. Small objects fixed in the hand — built

- For small things, the object's scene node is placed every frame in the
  draw pass (after physics has placed it, before it is drawn), rigidly on
  the hand:
  - It turns with the wrist the way it was turned against the hand when
    the hold began.
  - The touched point stays in the palm.
- The physics body keeps following the spring to the same palm, so letting
  go leaves it roughly where it was seen, and a throw keeps the spring's
  speed.
- "Small" (`game::IsSmallHeldObject`) means both:
  - The form type is one of apparatus, book (scrolls are books),
    ingredient, misc item, ammunition, soul gem, key, potion or sigil stone
    (xOBSE GameForms.h: 0x13, 0x15, 0x19, 0x1B, 0x22, 0x26, 0x27, 0x28,
    0x2A).
  - The scene bound's radius is at most 21 units (0.3 m).
- Weapons, armour, lights and anything larger stay on the spring.
- The setting is `[Hands] AttachSmallObjects` (default on, settings row
  "Small objects in the hand"). The first holds are logged with the type,
  the radius and the decision.
- Code: `game::HeldObject` (`CaptureAttachment`, `AttachedPose`,
  `StepHeldObject`). Tested in FrameLogicTest.
- **Limits.**
  - Letting go shows the physics body's own orientation from the next
    frame, so there can be a small turn at the moment of release.
  - A held object does not collide visually: it can pass through a table
    while the physics body is stopped by it. If the spring breaks (too far,
    0xA73DE0), the engine drops the object.
  - The gesture "an item brought to the body goes into the inventory" is
    built (controls-spec 4.6). The 25 cm distance floor was removed for it on
    2026-09-26.

### 5. Grips by size — proposal

- Choose the hand pose by the object's bound size and shape:
  - a pinch (thumb and index) under about 4 cm;
  - a round grip for potions and fruit;
  - a fist for handles;
  - two hands for anything over about 60 cm, where the second hand's grip
    near the object also counts as holding it.
- This needs per-finger curls (part 1 bends all links alike) and the
  object's bound, which part 4 already reads.
- Two-handed carrying would also want the target between the two palms, so
  a crate is carried in front of the chest.

## Built on 2026-09-26, after the first test

### Through the player's body

A held object was stopped short of the body: the player's capsule collides
with it.

- The body's filter is at hkRigidBody+0x30. The layer is in the low 6 bits,
  0x4000 means "no collision", and the system group is in the high 16 bits.
- The engine's rule (0x008A7F70) lets two bodies of the same group pass
  through each other, unless both are linked or both are bipeds.
- While held, the body therefore takes the player's group through the
  engine's setter 0x0089F4D0, which also refreshes the world. The group is
  read from the controller's filter (0x0065ABE0); the fallback is 9.
- On release its own group goes back.
- It keeps colliding with the world.
- Always on, no setting: gestures will build on it (an object brought to the
  body goes into the inventory). Code: `game::GrabPhysics`.

### Throwing

Vanilla's release leaves the spring's damped speed, a soft drop.

- Once the engine has let go, OBVR does what the engine's Telekinesis
  throw (0x006A7830) does: it activates the body (0x008A6410), then sets
  its linear velocity (hkMotion vtable +0x54, in
  Havok units = game units × 0.142877).
- The velocity: the palm's speed first, SteamVR's since the fourth test;
  taken out in the fifth test and back since the sixth, with
  `[Hands] ThrowStrength` (0 leaves Havok's own release).
- A body that went away with its hold (picked into the inventory, say) is
  not touched: the check is the ref still having a scene node and the body
  its vtable and wrapper.

### Two modes: in the hand (default) and levitated

The tester's call after the second test: the levitating grab goes behind a
flag, and the default holds every object in the hand like a sword or a
torch.

- **In the hand** (`[Hands] LevitateObjects=0`, the default):
  - Every grabbed object's scene node is placed on the hand each frame, as
    part 4 does for small ones. It turns with the wrist, and the touched
    point stays in the palm.
  - The spring still pulls the physics body to the palm.
  - On release the body is first put where the object was seen, through
    bhkRigidBody vtable +0xA0 (SetTranslationAndRotation, 0x008A2FB0), inside
    the Havok critical section at 0x00BA7B00, as the engine's own
    node-to-Havok push does (0x0089EAE0).
  - Then it is activated and given the hand's speed.
  - The rotation goes over as a quaternion (x, y, z, w;
    `game::QuaternionFromRotation`, tested against the rotation it came from).
- **Levitated** (`LevitateObjects=1`): the behaviour of parts 1–4 above.
  Only small things are fixed (`AttachSmallObjects`); the rest floats on the
  spring.

### Third test (2026-09-26): the node's own update, the sword grip, items by distance

- **It did not turn with the wrist.** The object's scene node was written
  and then updated through its own update pass. For a Havok-driven object
  that pass sets the node back to its rigid body's pose, so what was seen
  was the spring-pulled body: in the palm, but hanging.
  - The node's world transform is now written directly.
  - Only its children are updated from it (`game::UpdateChildTransforms`).
- **"A": held like the sword.** In the in-hand mode, whatever way an object
  was picked up:
  - its own up (local z) runs along the blade, i.e. the grabbing
    controller's forward, the axis a swung weapon strikes along;
  - its x runs across the fingers;
  - its middle (the scene bound's centre) sits where the weapon's grip is,
    the right hand's `Weapon` node, or the palm for the left hand
    (`game::SwordGripRotation`, `CaptureSwordGrip`).
- **Items by distance, not by a ray.** A hand brought to an item without
  pointing at it found nothing, because the pick is one ray a frame.
  - The loaded items of the player's cell are measured against both hands,
    to the surface of each scene bound (`game::FindNearestItem`).
  - The pick is aimed from the nearer hand at the nearest item within
    reach, so the tooltip, the marker and the grab follow as before.
  - This replaces the "hand that has been moving" rule of the second test.
  - **Limit:** only the player's own cell is searched. In the open world an
    item just across a cell border is not found.
- **Throw strength** default 0.6 (1.0 was "zu stark"); see the fourth test.

### Fourth test (2026-09-26): the marked side in the grip, the throw's speed

- **The marked side in the grip.** The hand sat in the object's middle. The
  point the marker showed (the pick's hit when the grip closed) now goes
  into the grip, so the side that was reached for is the side held. The
  middle is used when there is no such point.
- **Objects shot away on short, quick moves.** The speed came from five
  frames of palm positions, and a short jerk made a spike. It now follows
  the SteamVR Interaction System's throwable (Throwable.cs, ReleaseStyle
  AdvancedEstimation and scaleReleaseVelocityCurve,
  github.com/ValveSoftware/steamvr_unity_plugin):
  - **SteamVR's own controller velocity.** TrackedDevicePose vVelocity and
    vAngularVelocity, filtered in the tracking, is carried to the held point
    as v + ω × r. The Normal VR studio found SteamVR's values more consistent
    than any velocity they measured themselves (normalvr.com/blog/throwing-
    throw-down).
  - **The fastest of the last 8 frames**, so a hand already slowing as the
    grip opens still throws.
  - **Eased in:** 10 % of the speed at rest, rising smoothly to all of it at
    3 m/s, so a drop, a toss and a throw stay apart. A short flick at 1 m/s
    gives about a third.
  - **Throw strength** back to 1.0 by default.
- How Half-Life: Alyx itself computes a throw: I could not verify this; no
  published source was found.

### Fifth test (2026-09-26): kept as it lay, the throw back to Havok

- **Held as it lay, not like the sword.** The marked side sat in the grip,
  but the sword turn ("A") set every object upright along the blade. The
  tester wants the object to keep the turn it had in the world. The in-hand
  mode now captures the object against the hand when the grip closes
  (`CaptureAttachment`), as the levitated mode's small things always did:
  closing the grip moves and turns nothing, and the wrist turns it from
  there. The marked side (or the middle) stays in the grip, the right hand's
  `Weapon` node or the left palm (`game::AttachesInHand`).
  `SwordGripRotation` and `CaptureSwordGrip` are gone.
- **The throw back to Havok.** Short flicks of the wrist still shot objects
  away with SteamVR's velocity (ω × r adds the most exactly on a flick). At
  the tester's call OBVR sets no speed of its own any more: the body is put
  where it was seen and woken, and keeps the speed the spring gave it.
  `[Hands] ThrowStrength` and the velocity plumbing are gone; a1ce300 has
  them if a throw of OBVR's own is wanted again. Vanilla's release was
  called a soft drop after the second test, so throws may be weak again.
  Not verified: that SetTranslationAndRotation (bhkRigidBody vtable +0xA0)
  leaves the body's velocity as it was.
- **No ring or tooltip on NPCs.** The ring and the tooltip showed on
  whatever the pick found near a hand, so an enemy close in a fight got the
  grab ring. They now show only on items a hand can take
  (`game::ReachMarkerWanted`, the same item types as the distance search).
  A dead body can still be grabbed with the grip, but shows no ring.

### Sixth test (2026-09-26): the flick is not the throw, the ring to the hand

- **Havok alone threw neither well.** With no speed of OBVR's own, a real
  throw was only a soft drop, and a short flick of the wrist still shot the
  object away. So the flick never came from OBVR's velocity: it happened
  with the palm's speed, with SteamVR's and with none.
- **The likeliest cause: letting go inside the player's capsule.** While
  held, the body is in the player's group and passes through the capsule.
  At the release the old group went back at once. A flick lets go near the
  chest, inside the capsule, and Havok pushes an overlapping body out. A
  throw with the arm lets go outside it. Not yet confirmed.
  - The old group now waits until the object's bound is clear of the
    capsule on the horizontal (`game::InsidePlayerCapsule`). The radius is
    the one the grab update uses: the controller (0x0065A2C0) asked for its
    shape's radius (0x008913C0), times 6.999, plus 5 units.
  - An object taken again while it waits keeps its first group.
  - The log says at each release whether it was inside, how far from the
    player's axis, when its group went back, and the object's speed over
    the 12 frames after the release ("flight of ...").
- **OBVR's throw is back** (a1ce300: SteamVR's velocity, the peak of 8
  frames, eased), with `[Hands] ThrowStrength` (0 leaves Havok's own
  release).
- **The ring moves to the hand.** The pick is aimed from the hand at a point
  between the item's middle and the vertex of its geometry nearest the
  hand: the middle at the marker's distance, the near side from 10 cm in
  (`game::NearSideWeight`, `NearSideAimPoint`, `kNearSideMetres`). The pick's
  hit is where the ring sits and what the grip takes.
  - The vertices are read from NiGeometry's data (+0xB4, xOBSE NiObjects.h).
    The data's own layout (count +0x08, bound +0x10, vertices +0x20) is not
    in xOBSE; it is checked, not trusted: every vertex must lie in the
    data's bound, else the middle is kept and the log says so once
    (`game::NearestVertexOf`, `VertexInBound`).
  - **Fixed 2026-09-29: the near side never worked.** Measured on a placed
    cup: the bound is at **+0x0C** (+0x1C holds a pointer), the vertices at
    **+0x1C**, and +0x20 holds the normals (vectors of length 1). And the
    array's end was checked at `vertices + count * 12 - 1`, never 4-aligned,
    which `LooksLikeObjectAddress` always refuses. So no geometry was ever
    read and the pick always aimed at the middle. Now the near side is used
    as described above - how the ring and the grab feel from 10 cm in has
    not been seen in the headset since.

### Seventh test (2026-09-26): confirmed, the ring from a metre, the float

- **Confirmed: the flick was the capsule.** The flight lines of the sixth
  test's log show every object let go inside the capsule leaving with the
  speed OBVR gave it (167 given, 163 to 152 over the next frames; 338 given,
  374 to 325) or, set down, falling under gravity (20, 25, 31 ... units/s),
  and no push-out. The one large first sample of each line is the placement
  at the release, not a speed. The tester: no more flicks.
- **Throw strength 1.6 by default**, the tester's value.
- **The ring from a metre.** `[Hands] ReachMarkerMetres` defaults to 1.0 (up
  to 2 in the menu). The ring starts in the item's middle and moves to the
  side nearest the hand as it comes closer, all the way from 10 cm in, and
  back to the middle as the hand leaves (sixth test's `NearSideWeight`, now
  over the longer distance). Beyond `GrabReachMetres` an item counts only
  while the hand's laser points at it within 35 degrees
  (`game::ReachingFor`), so an item on a table does not take the pick from
  a door being pointed at.
- **Floating to the hand** instead of appearing in it: the held point
  glides from where it lay to the grip, eased, at about 2 m/s, between 0.1
  and 0.35 s (`game::FloatSeconds`, `FloatWeight`, `FloatPoint`). The object
  keeps its turn and follows the wrist on the way.

### Eighth test (2026-09-26): the pull, the ring earlier

- **The pull.** `[Hands] PullReachMetres` (default 1.0, 0 is off, up to 2):
  a closed grip takes an item that far from the hand while the hand points
  at it (beyond the grab's reach, `game::ReachingFor`), and the item floats
  in. Only items a hand can take are pulled; a body or anything else the
  grab could move still needs `GrabReachMetres` (`game::GripTakes`). The
  float now lasts up to 0.6 s, half a second from a metre.
  Not verified: that the engine's grab takes an object that far; the log
  says "did NOT take" when it refuses.
- **The ring moves earlier.** `[Hands] ReachNearSideMetres` (default 0.5)
  is where the ring sits all the way on the near side, and the move from
  the middle is eased out, fastest at first: five centimetres in from a
  metre it has moved a fifth of the way.

### Ninth test (2026-09-26): the float's speed, the ring evenly

- **Faster float, adjustable.** `[Hands] PullSpeedMetres` (default 4, 0.5
  to 10 m/s; 2 before): a metre in a quarter second. The float lasts
  0.05 to 0.6 s (`game::FloatSeconds`).
- **The ring evenly with the hand.** The eased-out move jumped too soon, the
  smooth one moved too late: the weight is now linear from the marker's
  distance to `ReachNearSideMetres`, whose default is 5 cm - very near the
  hand, very near the edge; half way, half way (`game::NearSideWeight`).
- The headset settings menu built at most 96 rows; with the new setting
  there are more, so the last would have had no row. It builds up to
  `ui::SettingsMenu::kRowCapacity` (160), and SettingsMenuTest checks that
  every setting fits.

### Tenth test (2026-09-26): the open hand, not only the pointer

- Beyond the grab's reach an item counted only while the laser pointed at
  it, but the usual reach is the open hand brought to a thing. The palm now
  counts too, in a wider cone of 50 degrees (`game::ReachingForWithHand`,
  `kPalmConeCos`); the laser's 35 degrees stay.
- The palm's direction is the controller's sideways axis towards the other
  hand: -x for the right, +x for the left. That is read off how an Index
  controller is held, not measured; if the back of the hand finds items and
  the palm does not, the sign is wrong.
- The death log line "the body is drawn ... ahead" was written every frame:
  the restore after each render cleared its once-flag. The restore no longer
  does.

### Eleventh test (2026-10-06): the pick held steady

The tester: pointing at or reaching for a thing, with the name under it,
the ring and the outline, "ist ziemlich jittery. es sollte sich natürlich
anfühlen." Three things shook, all in `game/PickHold.h` now
(pick_hold_test, harness `pick-hold`):

- **The choice.** The item was chosen afresh every frame (`PickRank`), so two
  items ranking alike swapped with every tremor of the hand, and the hand
  the pick came from swapped too when both reached for one thing. Now the
  pick holds its item and its hand (`StepPickHold`): a challenger has to be
  clearly better - a better class, or within the class a key better by
  2 degrees (the laser's miss) or 7 cm (a distance) - and stay so for
  0.15 s; a touched item takes over at once. An item no hand reaches for any
  more is kept 0.15 s before it is let go, so a hand at the edge of the
  reach does not blink.
- **The distance.** Within the grab's reach the distance to an item is to
  its mesh (`NearestVertexOf`), not to its bound sphere: a sword's sphere is
  a metre across, and two swords lying together put a hand inside both at
  once (the first harness run could not tell them apart). So "touched" is
  the blade a hand is on, and a hand hovering 40 cm over a sword's middle
  is no longer in reach of it - it has to come to the blade, or point.
- **What is shown.** The ring, the info row and the mark follow the engine's
  pick target only once it has stayed the same for 0.12 s
  (`StepRefSettle`): the ray aimed at the held item can cross the other
  thing lying against it for a few frames (seen in the first run: the mark
  went to the other sword and back during one sweep). And their points ease
  towards the target with an 80 ms time constant (`StepAnchor`), snapping
  only when the thing changes - the ray's hit no longer walks the ring over
  the surface with every tremor, and the aim at the item's near side no
  longer jumps from vertex to vertex. The info row hangs under a small
  thing's own middle (a bound within 56 cm), under the hit only for a large
  one (`vr::TargetHangPoint`).

Harness `pick-hold` (two Iron Longswords dropped together, the right hand
lowered to them and swept 15 cm across and back in 1.5 cm steps, then a
tremor of a centimetre): 2 "Pick: on" lines, both while the hand came down,
none during the sweep or the tremor; the outline moved once. Not seen in
the headset. The log says "Pick: on <ref> (class, key, hand) - was <ref>"
at each change, forty lines at most.

**The tester's first headset round (2026-10-07):** steadier, with three
things left, built the same day:

- **The nearest to the hand wins.** On a laden plate the hand brought to
  one thing took another: the laser from that same hand, falling on the
  thing behind within 6 degrees, ranked above "within the grab's reach".
  The classes are reordered (`PickRank`): touched, then within the grab's
  reach by distance, then the laser on it, the cone, the palm. So a hand
  near things takes the nearest; the laser decides only beyond the reach,
  which keeps the earlier tester's case (three things at arm's length,
  pointing at the far one). The hold's distance margin is 2 cm now (7
  before), so on a plate the nearer thing can still take over; the 0.15 s
  do the steadying.
- **The old name on a change.** The ring and the row waited 0.12 s for
  the engine's target to settle even when the pick itself had changed on
  purpose, so for those frames the new name stood under the old thing. The
  item the pick holds now bypasses the settle (`StepRefSettle`'s `trusted`),
  and the settle for anything else is 0.08 s. Whether the flicker was only
  that: not verified - the HUDInfoMenu's own text is the engine's.
- **The outline with "the setting off".** The tester's INI has no
  `TargetOutline` line, so the default applied, and that was on (decided
  2026-10-05) while the "Target glow" row showed off. Both default off now
  (`[Hands] TargetOutline=0`, `TargetGlow=0`); the rows under Hands switch
  them on. Whether a mark ever stayed on with the row switched off: not
  seen (a mark switched off is stopped the same frame).
- **The name while holding (the tester, 2026-10-07: "muss sichtbar sein am
  objekt auch noch während des festhaltens").** While the grab holds
  something, the pick ran along the hand's laser, so the game's info text
  named whatever the laser crossed and the row hung there. Now the pick is
  aimed from the holding hand at the held thing's bound (the `heldRay`
  branch before `nearRay` in the camera pass), so the text stays its name,
  under it. Whether the engine's crosshair pick returns the grabbed
  reference at all is not verified: the harness `stow` run of the day could
  not hold anything (the headset was awake, the scripted hand 1.6 m off),
  and the "HandScript: items" line now prints "the crosshair on <ref>" next
  to "held" for the next run with the headset asleep.
- **The open palm near a thing beats the laser on a far one (the tester's
  second round, 2026-10-07).** A new class between "within the grab's
  reach" and "the laser on it": the palm turned to a thing within 60 cm
  (`kPickPalmNear`, `kPickPalmNearUnits`), by distance. Farther off the laser
  decides as before. The classes are named now (`game::PickClass`).
- **The name while holding, second try.** The pick aimed at the held thing
  did not bring its name (the tester: "objekte die ich vor mir halte zeigen
  keinen namen an"); why the engine's pick does not name a grabbed
  reference is not known (no read of the grabbed-reference field in the
  pick routine's range 0x580000-0x582800; the harness could not look while
  the headset was awake). So OBVR paints the name itself: the base form's
  TESFullName in the menu font on a dark strip (`ui/HeldNamePainter.h`,
  held_name_test), a canvas overlay 36 cm wide hung under the held thing's
  bound and eased like the info row (`UpdateHeldName`), hidden when nothing
  is held. **Removed the same day** (the tester: the vanilla font or
  nothing). The engine route is still open: the field at HUDInfoMenu+0x54
  is written at 0x005A9E3F (`mov [esi+54h], eax` after a float-to-int
  call, inside a long HUDInfoMenu update around 0x005A9D90), which does not
  look like a plain "set the crosshair reference" entry; the function that
  builds the text from a reference is not found yet. The pick aimed at the
  held thing (the `heldRay` branch) stays, since it costs nothing.
- **Owned things in the hand (not built, the tester's decision).** The idea
  of a toggle making the grab of an owned thing no crime until it is
  stowed, eaten, carried out of the cell or 15 m away is left to Put it in
  its Place (above), which changes exactly the grab's crime; OBVR's stow
  stays a crime like activate.

### Open wish (Nadi, Windows, 2026-10-24): the name with the ring

"Namenstexte sollen früher schon kommen, am besten gleich mit dem Aufhebekreis." The ring
shows from a metre (the seventh test); the name is the engine's HUDInfoMenu text for its own
target, which comes when the engine's pick has settled on the thing (StepRefSettle, 0.08 s)
and within the engine's activation reach [R for the settle; the reach part unmeasured]. Wanted:
the name at the moment the ring shows. First step: log the frames between the ring and the
name for a thing 1 m and 0.5 m away; if it is the engine's reach, the name would need
OBVR's own text on the ring (the hand HUD's overlay) rather than the HUDInfoMenu's.

### Up to the mouth and the body

Held objects stopped about 25 cm from the head (2026-09-26). Eating by
bringing food to the mouth is a planned gesture.

- The grab update keeps the spring's target out of a cylinder round the
  player: the controller's radius × 6.999 + 5 units (0x0066DF10..0x0066DF44).
- `game::AllowGrabNearBody` turns its branch (`jne` at 0x0066DF44) into a
  `jmp` while the hand mode runs, with the same x87 stack, and puts it back
  otherwise.
- **Not verified:** the near clip plane is 10 units (14 cm) from the eye, so
  an object right at the mouth may be cut away in the picture.

- **Built 2026-10-25: food and potions let go at the mouth are eaten and drunk** (Nadi,
  Windows, 2026-10-24: "Essen und Tränke an den Mund ist Essen"; 2026-10-25: "auch potions aus
  der spielwelt trinkt man wenn man sie zum mund führt"). In vr::StepStow beside the chest's
  stow (stow_test): an ingredient - Oblivion's food, the apple and the bread are ingredients -
  or a potion held with the drawn hand within `MouthReachMetres` (0.17 since 2026-10-26, Nadi: "Mundfläche zum essen etwas vergrößern"; 0.13 before) of the mouth, a point
  `MouthDownMetres` (0.09) below and `MouthForwardMetres` (0.05) ahead of the eyes in the
  head's own frame (it pitches with the head; vr::MouthInTracking), and let go there or within
  the stow's grace after leaving it, is taken as activating it takes it (its owner, the crime)
  and then used as the inventory uses it: game::EatItem, Actor::EquipItem on the base form,
  which eats an ingredient (0x005E44C0) and drinks a potion (0x005E08C0) with the game's sound
  [R in the binary, 0x005FAEA0]. A poison - a potion whose effects are all hostile, the game's
  own test 0x00414EB0 on its effect list at +0x30 - is not drunk: from the inventory the game
  would put it on the weapon; let go at the mouth it drops. The release decides, as at the
  chest and as HIGGS does it in Skyrim VR ("consume ... by dropping them at the mouth"), so
  nothing is eaten by passing the face; a short buzz says the hand is at the mouth. Where the
  mouth's sphere meets the chest's, the mouth wins for something edible. `[Hands] EatAtMouth`
  (default 1); it needs StowAtBody, being the stow's release. **The sound (Nadi, 2026-10-25:
  "beim essen muss der ess sound kommen der beim aktivieren auch kommt"):** the take is quiet
  (the pickup's "ITMIngredientUp" / "ITMPotionUp" silenced as a pulled weapon's take is), and
  the use plays the game's own eat sound, the one the inventory's use plays - EquipItem's
  0x005E99C0 with up 1 and use 1, whose picker 0x005E96E0 names "NPCHumanChew" for an
  ingredient and "NPCHumanSwallow" for a potion [read]; the log line names it. **The mouth
  follows the T-pose (Nadi, 2026-10-25: "mundposition kann ja wieder abhängig der t pose im
  onboarding berechnet werden"):** the measurement scales MouthDownMetres and
  MouthForwardMetres with the eye height as it scales the stow spot (vr/BodyMeasure.h,
  body_measure_test), 0.8 to 1.25 times. That moves the mouth by a centimetre or two; the
  bigger unknown is where the drawn hand sits with a bottle at the lips, which the headset's
  "Eat: ... the drawn hand X m from it" lines will say. Harness eat-at-mouth.txt: the
  apple eaten (no apple left for a second drop), the Restore Health potion drunk (health 40
  of 50 to 50 a second on), the Weak Poison of Illness refused and dropped, a loaf let go at
  the chest stowed, not eaten. The mouth's place and size are guessed from a face's build;
  the headset's to settle. Open: a stack of ingredients lying as one reference opens the
  game's quantity menu on the take, and the eat then finds none yet in the pack (not tried).
- **Found on the way and fixed: a taken reference was still followed by the grab physics.**
  A thing let go inside the player's capsule waits there for its old collision group
  (game/GrabPhysics.cpp, Wait); taken into the inventory the same frame, its reference and
  body were deleted under that wait, and the eat scenario went down with the game two frames
  after the potion was drunk (2026-10-25 13:18; the run before had survived the same lines).
  game::ForgetTakenRef now drops the reference from the wait, the flight report, the last
  pose and the hold before every take - the chest's stow, the mouth, a pulled weapon's take.
  The chest's stow had the same hole since the wait was built.

### Feature idea (Nadi, 2026-10-25): the helmet - an overlay, taken off and put on by hand

Nadi, 2026-10-25: "Helmet overlays and / or taking off helmet physically?" Nothing built.

What is there now [R in the code]: the first-person body collapses `Bip01 Head` for the draw
(`[Body] HideHead`, default 1; game/PlayerBody.cpp), and a helmet is a rigid shape that goes
with that bone (dismemberment-spec), so the player never sees their own helmet in the headset
(derived, not looked at in the headset). A helmet goes on and off only through the inventory.

- **(a) Taken off and put on by hand** - medium. The off or free hand gripped at the head
  takes the worn helmet off: the game unequips it, and it lies in the hand as a held world
  object, as a released weapon does. Let go at the head, it is equipped again, the way
  `EatAtMouth` decides at the mouth on the release (vr::StepStow, a sphere round a point in
  the head's own frame, scaled by the T-pose like the mouth). Let go elsewhere, it falls,
  and at the chest it is stowed. Reused: the stow's release zones, Actor::EquipItem as the
  eat path calls it, the drop path that puts a released weapon into the world, the
  hold. New: the grip at the head as a take, and the helmet's reference made at the hand
  and held in the same frame - the ForgetTakenRef lesson (above) applies. Risk: whether the
  dropped helmet's reference comes up in the frame it is asked for (not read), and the grip at
  the head against the grips already there (the yield gesture, the body's own holds). Test:
  harness (helmet worn, hand to the head, grip, pulled away: unequipped and held; back to the
  head and let go: worn again; let go at the chest: in the pack), then the headset for the
  head's sphere size.
- **(b) The overlay** - small for one generic rim, medium with a shape per helmet. A worn
  helmet shows as a dark rim or visor at the edges of both eyes' views, drawn on a quad in
  front of the eyes as OBVR's other overlays are. Its outline cannot be read from the helmet's
  record: the biped slots say "head", not open or closed (guess: a closed helm would need a
  list or a guess from its mesh). Risk: comfort - a frame narrows the view and some feel shut
  in - and only the headset can judge that. It would be a switch, default off. Test: harness
  pictures with and without a helmet, then the headset.

**Best pick: (a) first.** It fits the hands-on direction (eating at the mouth, stowing at
the chest, B&S) and is mostly the release zones and equip calls already built, and its
risks can be tested in the harness. (b) is cheap to try once (a) is in, and only the headset
can say whether it should stay. The evidence for the order is the reuse in the code. Nothing
measured says what players want more; the cheapest way to settle that is to ask the
testers in the next beta's notes.

### Feature wish (Nadi, 2026-10-28): from the inventory into the hand (nothing built)

Nadi, 2026-10-28: "im inventar ein item mit pointer highlighten und dann mit linker oder rechter
hand grab am controller nur greifen kein 100% griff -> das inventar schließt und ich halte das
objekt in der hand wo das greifen am controller war. wenn objekt dann halte ichs als objekt, wenn
waffe oder schild ist die dann ausgerüstet, wenn fackel dann fackel in der hand. ab da kann ich wie
in obvr interagieren wie ich will."

**Wanted:**
1. In the inventory, the laser pointer is on an item, so it is highlighted.
2. The left or the right hand closes its hold. This is the grip by its pull (vr/GripHold.h,
   `[Hands] GripCloseAt` 0.8), not the squeeze that makes the binding's click.
3. The inventory closes.
4. That hand holds the item:
   - a plain item (food, clutter, a potion, an ingredient, a book): held as a held object;
   - a weapon or a shield: equipped and in the hand;
   - a torch: equipped and burning in the hand.
5. From then on, everything OBVR does with held things applies: let go = fall or throw, the stow
   at the chest, the mouth, a holster place, the eat.

**The pieces that exist [R]:**
- The pointer and the item under it: the inventory follows the laser cursor (`[Hands]
  LaserCursorDirect`). The drop button already acts on "the item under the cursor" through
  vanilla's Shift and left click (vr/HandInput.h `DropPressState`). Which item form sits under the
  cursor is not read by OBVR anywhere: the drop leaves that to the game.
- The hold's edge: `vr::StepGripHold`, the same signal the holsters and the weapon grab use.
- A weapon into the hand at once: the grip take (game/WeaponGrab.h, `EquipWeaponForm`,
  `[Hands] TakeDrawsAtOnce`, drawn within 3 frames, free-weapons-spec 5.5).
- A shield and a torch equipped by hand: the holster takes (`game::EquipItemForm`,
  `[Hands] HolsterShieldTorch`, free-weapons-spec, 2026-10-24); a torch burns once equipped
  (TorchRelease).
- A plain item into the hand: vanilla's drop puts a reference at a place (RemoveItem with the drop
  flag and a position, as `game::DropEquippedWeapon` does for the weapon). The hand's grab then
  holds a reference (game::GrabPhysics, HeldObject, the pull to the hand).

**What is new:**
- Reading the form of the highlighted item from the inventory menu. The tile under the cursor is
  known (`ActiveMenuId`, the cursor's tile search 0x00581390); the step from the tile to its
  inventory entry is not read. This is the main engine unknown.
- The trigger in the menu: the hold closing while the inventory is up, for either hand, without
  the hold closing being taken as anything else there.
- Closing the inventory from OBVR, then the take in the world the next frame (the menu closed
  first, so the drop and the grab run in the world as they do today).
- For a plain item: drop it at the hand's place and hand the new reference to that hand's grab at
  once, without the pull's flight.

**Deviations, to decide [R/D]:**
- **The weapon in the left hand.** The engine shows a one-hander only in the weapon hand and a
  shield only in the other hand (free-weapons-spec section 1: the engine's single weapon slot).
  Today "the other hand only ever carries" (game/WeaponGrab.h). So a sword grabbed with the
  shield hand cannot be equipped there. Options:
  - (a) the sword goes to the weapon hand anyway: cheap, but it does not appear where the grab was;
  - (b) that hand holds it as a plain held object, unequipped, as a sword lying in the world held
    off its grip is held today: cheap, and the other hand can take it from there;
  - (c) a true off-hand weapon: large (free-weapons-spec 5.3, "later").
  Recommended: (b). It keeps "it is in the hand where I grabbed" and reuses what exists.
  **Decided by Nadi, 2026-10-28: (b).** An item the grabbing hand cannot be equipped with is held
  there as an object, unequipped. The same goes for a shield or a torch in the weapon hand. The
  weapon hand's grip on the held weapon's handle then equips it, as a weapon lying in the world is
  taken today.
- **A shield grabbed with the weapon hand:** the same choice. (b) holds it as an object.
- **A torch grabbed with the weapon hand:** the engine burns a torch only in the other hand. The
  same choice applies: (b) gives an unlit torch held as an object. Unverified whether a dropped
  torch reference can burn in the right hand.
- **A two-hander or a bow:** equipped as they are, both hands. Which hand counts as holding it
  follows the grip take's rules.
- Quest items cannot be dropped in vanilla. (b) has no reference for them. Proposed: equip one
  that can be equipped, refuse a plain one with the game's message.

**Cost: medium.** Most pieces exist (the take, the equips, the drop, the grab, the hold's edge).
New are the read of the highlighted entry (engine RE, the risk), the menu's closing with the take
after it, and the plain item's drop straight into the grab. Tests: the unit flows of the choice by
kind and hand, and a harness scenario per kind (sword, shield, torch, a potion, a two-hander; each
grabbed with each hand). The feel of the hold at 0.8 in the menu is for the headset.

**Step 1 built 2026-10-28 [R/M]: the highlighted entry is read.** game/InventoryGrab.h
(`InventoryItemUnderCursor`), pure part game/InventoryGrabLogic.h, inventory_grab_test. The
tile under the cursor itself (InterfaceManager +0x98) carries the item's index in its user11
trait (0x0FB9; xOBSE GameTiles.h and Commands_Menu.cpp GetActiveMenuElement). The index goes to
TESObjectREFR::GetInventoryItem 0x004D88F0 (thiscall index, bGetWares = 0), which returns an
EntryData with the form at +8. Only the tile itself counts: the list's frame one level up has a
user11 of its own (0 off the list), which named the first item while the pointer was elsewhere
(the harness's first run). A hand script's mark in the inventory logs "Inventory: under the
pointer index n - <name> (form, type)" or "no item". inventory-pointed-item.txt PASS 2026-10-28
08:47 (Linux, headless): "Iron Longsword" (00000C0C, type 33) on the first row at two angles, no
item off the list. Open: an index past the first row was not reached in the run (the angles
missed the second row). Whether the index stays right after the list is sorted or scrolled is
unchecked (xOBSE reads it the same way).

**Step 2 and 3 built 2026-10-28 [R/M]: the hold in the inventory takes the item.**
`[Hands] GrabFromInventory=1` (default on). In the inventory, a hand's hold closing (the pull,
vr/GripHold.h; a hold already closed when the inventory opened counts only after it opened) reads
the item under the pointer and chooses the take by kind and hand (`ChooseInventoryTake`,
inventory_grab_test):
- a weapon in the weapon hand, or a bow in the other hand: the inventory closes
  (game::CloseMenus 0x00579770), and in the first world frame the weapon goes to the take's
  steps of the world grab (`BeginWeaponTake`, equipped and in the hand);
- a shield or a torch in the other hand: closed the same way, then EquipItem with its sound;
- anything else, and decision (b) (a sword in the other hand, a shield or a torch in the weapon
  hand): held as an object, step 4. Not built yet, so it logs "would be held ... not built yet"
  and the inventory stays open.

The item kinds are read off the form [R from xOBSE, measured for the weapon only]:
- TESObjectWEAP's type at +0x90;
- TESObjectARMO's slot mask, a word at +0x68 (TESBipedModelForm at +0x64, the mask at +4, the
  shield bit 13);
- TESObjectLIGH's flags at +0x7C (0x02 carried).

The ARMO type 0x14 and the shield bit are not yet measured in the game.

Logged: "Inventory grab: the weapon|other hand closed on <name> (form, type, kind) - take n", then
"the inventory closed - the weapon to the take's steps" or "equipped in the other hand".

inventory-grab-weapon.txt PASS 2026-10-28 09:17 (Linux, headless, Nadi's Windows mod list):
- the left hand on the Iron Longsword: "would be held in the other hand as an object - not built
  yet", and the inventory stayed;
- the right hand on it: the inventory closed, "Weapon grab: equipped - drawing";
- the grip opened after: the sword dropped where the hand was (WeaponHold, as for a weapon taken
  in the world).

**Seen [M]:** straight into the hand did not hold in either run ("out, but not on the hand -
drawn by the click"). The draw's animation brought it, so about half a second later than a
weapon taken in the world (not timed). The cause is not looked into: the Iron Longsword may have
been the equipped one already.

**Open:** the shield and the torch in the game, a bow, the step 4 hold, a grip held through the
inventory's opening in the headset.

**Order:**
1. Read the highlighted entry (a log line at a mark in the inventory: name and form ID). Done.
2. The trigger and the closing.
3. The weapon, the shield and the torch in their own hands.
4. The plain item.
5. The deviations as decided.

## Hands, weapon and held objects that push the world — research (2026-09-27)

Asked for: the weapon swung through the room, the hands, and what they hold
should knock into the objects there. Nothing is built yet; this is what the
route rests on.

Read:
- Oblivion is built with Havok 3.1.1 (xOBSE `obse/obse/HavokBase.h`, line 4:
  "oblivion is built with havok 311"). Oblivion.exe carries the class names
  `hkKeyframedRigidMotion`, `hkFixedRigidMotion`, `hkBoxShape`,
  `hkCapsuleShape`, `hkSphereShape` and `bhkMouseSpringAction`.
- xOBSE's physics commands move actors only (their character proxy); none
  pushes a clutter body (`obse/obse/Commands_Physics.cpp`).
- HIGGS (Skyrim VR, github.com/adamhynek/higgs) gives each hand, and the
  weapon, its own keyframed rigid body: `Hand::CreateHandCollision`
  (src/hand.cpp 559-607) builds a box body with the keyframed motion type in
  a collision group of its own and adds it to the world; the weapon's body
  clones the weapon's own collision shape (hand.cpp 755-790). Every frame
  `applyHardKeyFrame` gives the bodies the velocity that reaches the
  controller's pose by the next step (hand.cpp 690-712, physics.cpp 865). A
  keyframed body has infinite mass: what it meets is pushed, it is not.
  A held object is switched to keyframed and driven the same way (hand.cpp
  1677), its motion type restored on release (hand.cpp 3069).
- OBVR already has the pieces for the held object: the body, its collision
  group, the Havok lock, `hkMotion::setLinearVelocity` (GrabPhysics.h).

Derived: the engine has what the HIGGS route needs - keyframed motion
exists in the build. The held object is the nearest step (a body that exists,
driven by velocity instead of the spring); the weapon and the hands need new
bodies made at run time.

Not found (I could not verify these): the addresses of `setMotionType`,
`hkWorld::addEntity`/`removeEntity`, the shape and rigid-body constructors of
3.1.1, whether `hkKeyFrameUtility` is linked in (without it the velocities
are computed by OBVR), a free collision layer, and whether the engine's own
node-to-Havok sync overwrites a keyframed body.
### Built 2026-09-28: the held object pushes

The first step of the route above, for the object in the hand (the body
exists; nothing has to be made). Each frame, with `[Hands] HeldObjectsPush=1`
(default), the held body is given the linear and angular velocity that
bring it to the pose shown in the hand by the next step (gain 0.8, at most
15 m/s and 40 rad/s) - `game::DriveLinearVelocity`, `DriveAngularVelocity`,
tested. The spring stays; the drive takes over once the body is within
35 units (about half a metre) of the hand, so an object pulled from afar
still comes on the spring, and one stopped by a wall is let go by the
engine as before. Keyframed and fixed bodies (motion types 6, 7) are not
driven; nor is an object being stowed.

Read: the body's pose in its motion (rotation columns at +0x10/+0x20/+0x30,
translation at +0x40, from the grab's pivot arithmetic at 0x0066D7DD);
setAngularVelocity at motion vtable +0x58, derived from the pair
0x004D6AF0/0x004D6B30 (+0x54 and +0x58, same shape) and the impulse at +0x5C
(Telekinesis, 0x006A7964). Not seen in the game yet: whether the motion's
pose is where these offsets say, and whether +0x58 turns the body - the log
lines "driving ... the body at most N units from it" and a held object that
turns with the wrist will say.

Not yet: the weapon and the hands. They need bodies of their own, made at
run time; the constructors and hkWorld::addEntity are still to be found.
## Other mods: Put it in its Place - Enhanced Grabbing (2026-09-29, open)

The tester: "wie interagieren wir mit [nexusmods.com/oblivion/mods/19847]
was das vr grabbing angeht? wäre ja ein match made in heaven".

- **What it is** (its Nexus page, read 2026-09-29): shadeMe's OBSE script
  mod, version 0.5 "FiNAL" (2010). It overhauls the Grab key: grabbing an
  owned object is no crime any more, and NPCs watch, judge and react
  instead; thrown things make noise NPCs investigate; owners put thrown
  things back; a toggled grab (tap to hold, tap to let go) with an
  "Auto-Flinger"; slapping, snatching from actors, locking objects in
  place, equip on activate while grabbed; it works with telekinesis. An
  INI switches the parts.
- **How OBVR grabs** (read in OBVR): with the vanilla grab. The grip holds
  the game's Grab key (Z, `game::HandControls`), the engine's own grab
  (player +0x574, the spring) holds the object, OBVR only moves where it is
  pulled and gives the throw its speed. **Derived:** whatever the mod reads
  of the vanilla grab - the Grab control held, the grabbed object moving -
  OBVR produces too, so most of it should see VR grabs as grabs.
- **Where it could clash (derived, not verified):**
  - its toggled grab - a tap of Z starts, a second tap ends - against a
    grip that holds Z for as long as the hand is closed; it would want to
    be switched off in its INI;
  - its Auto-Flinger and OBVR's throw both setting the object's speed;
  - which object it thinks is grabbed: if it asks the crosshair
    (`GetCrosshairRef`) when Z goes down, it gets OBVR's pick, which runs
    from the hand while a grip closes (`SetWorldPickHandRay`) - likely the
    same object, not checked;
  - crime: OBVR's stow takes an owned object as activate does (a crime);
    the mod changes only the grab's crime.
- **Not verified at all:** the mod is not installed here and its scripts
  have not been read (they are in its ESP; the download needs a Nexus
  login). To settle it: install it into a test profile, read the scripts
  (TES4Edit), then a hand-script scenario that grabs an owned object in
  front of its owner.

## Order

Built first: 1, 2 and 4 (the tester's choice). Next: tune 1 and 2 in the
headset (curl axis and sign, palm offset), then 3(a) for large objects, then
5.

## Test in the headset

1. **Pick up a potion.**
   - The fingers close around it.
   - It sits in the palm and turns with the wrist.
   - The log says "Hands: holding ... small: fixed in the palm".
2. **Pick up a larger object** (a plate, a basket). It is on the spring,
   with the touched point at the palm and the fingers closed.
3. **Fingers bend the wrong way?** Make Grip curl negative. If they bend
   sideways, the axis is wrong; report that.
4. **Object in the wrist or too far out?** Move Held object distance in
   2 cm steps.
5. **Let go and throw.** It flies with the hand's speed. Watch for a jolt
   at the release.
