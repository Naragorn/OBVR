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

- **Assumption:** a Bip01 finger bends about its own z (3ds Max Biped's
  finger axes), not confirmed on Oblivion's first-person skeleton. If the
  fingers bend backwards or sideways, a negative curl flips the direction.
  If the axis itself is wrong, the curl needs another axis, which is not
  built yet.
- **Limits.**
  - One pose for every object: a pen and a pumpkin get the same fist
    (part 5).
  - Fingers go through the object where the pose does not fit it.

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
  - A future gesture, "a potion brought to the body goes into the
    inventory", fits here. The 25 cm distance floor was removed for it on
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

- Once the engine has let go, OBVR does what the engine's Telekinesis throw
  (0x006A7830) does: it activates the body (0x008A6410), then sets its
  linear velocity (hkMotion vtable +0x54, in Havok units = game units ×
  0.142877).
- The velocity is the palm's speed over the last frames, times
  `[Hands] ThrowStrength` (default 1.0; 0 keeps the soft drop).
- A hand slower than 0.5 m/s is a set-down, not a throw.
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
  - Then it is activated and given the palm's speed.
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
5. **Let go and throw.** It flies. Watch for a jolt at the release.
