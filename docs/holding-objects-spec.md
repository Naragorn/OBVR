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
