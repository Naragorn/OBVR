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
