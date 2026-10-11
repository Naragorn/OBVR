# The player's body in VR

## What exists

- `game/PlayerBody` shows the third-person skeleton in first person,
  Enhanced Camera's way (docs/vr-modding/ecosystem-and-prior-art.md,
  "Enhanced Camera 1.4b"): the skeleton (`Bip01`) moved each frame so
  `Bip01 Head` stands under the headset, the head collapsed for the draw.
- `[Body] Visible` is on by default since the body IK (2026-10-11). It was
  off after the first two headset runs of 2026-09-07, which had the view
  lurch with every look; that cause was found elsewhere the same day
  (`BlockVerticalLook=0` under `AimAtSource` doubling the pitch, fixed in
  `2b5ec02`; docs/vr-modding/open-questions-and-known-issues.md). The body
  has not been in a headset since - the IK below is its first headset run.

## Built 2026-10-11: the body IK

The tester: "mach als nächstes das ik vom body. es gibt bereits eine stting
für was ähnliches. das als grundlage verwendn" - the setting is `[Body]
Visible`, the body under the headset. The note of 2026-10-09 ("dann notiere
IK für den player body") asked for the picture VRIK gives Skyrim VR: the
arms reaching the controllers, the torso turned towards the headset, the
legs under it. All three are built, each on its own switch
(`[Body] IK`, `IKArms`, `IKTorso`, `IKLegs`; settings rows "Follows you
(IK)", "Arms reach your hands", "Turns with your head", "Knees bend when
you crouch"). The decisions are pure (`game/BodyIkLogic.h`,
`body_ik_test`), the engine side is `game/BodyIk.cpp`.

### The skeleton, measured

Debug.BodyTreeProbe (harness `body-probe.txt`) logs the third-person tree
and each bone's local transform ("Body bones:"). Every Bip01 bone runs
along its own +x: the clavicle 8.48 units to the upper arm, the upper arm
21.85 to the forearm, the forearm 14.78 to the hand, the thigh 33.67 to the
calf, the calf 27.33 to the foot. The forearm's local rotation under the
upper arm, and the calf's under the thigh, are pure turns about z (the idle
pose: -32.5 and -18 degrees for the forearms): the elbow and the knee are
hinges about the upper bone's z, folding towards its -y. The clavicles hang
from `Bip01 Neck`; the twist bones (`UpperArmTwist` 12.44 along the upper
arm, `ForearmTwist` at the forearm's origin) are children of their bone.
The body's hands carry "Weapon" (right) and "Torch" (left); a shield hangs
on `Bip01 L ForearmTwist`; the hands' model is the node "Hand  (<form>)".

### The arms

- **The target** is the first-person hand bone as the pins left it (at the
  controller, after the contact step, the two-hand grip and the bow) - so
  everything that places a hand places the body's arm too.
- **Two bones** (`SolveLimb`): the elbow on the circle the upper arm and
  forearm allow, the forearm's local rotation a turn about z alone (the
  rig's hinge, checked against the probe's -32.5 degrees in the test). A
  hand further than the arm reaches grows both bones up to `ArmStretch`
  (1.15: the body's shoulder-to-wrist is 36.6 units, 52 cm, a grown man's
  about 55-60); past that the arm points straight at it and ends short.
- **The clavicle** turns towards a far or high hand (`TurnClavicle`), at
  most 40 degrees: from two thirds of the reach to half the angle at full
  reach, up to 0.6 with the hand four fifths of an arm above the shoulder.
- **The elbow** (`ElbowPole`): nothing tracks it, so it is guessed from the
  body's habit (down, out, back) and from the hand (the forearm running on
  behind the wrist along the hand's axis), averaged across the
  shoulder-wrist line; VRIK and Parger et al. (VRST 2018, "Human Upper-Body
  Inverse Kinematics for Increased Embodiment in Consumer-Grade VR") guess
  it from the same two things - the numbers are this code's own. And never
  in the torso (`ElbowClearOfTorso`): an elbow inside the torso's box
  (between its back and a little in front of the shoulders' plane, inwards
  of a shoulder, between the pelvis and the shoulder caps) is moved round
  the circle to the nearest place outside. Found by the harness: a right
  hand reaching across to the left shoulder put the upper arm through the
  neck; a swivel clamp after Parger et al. was tried first and took a
  natural elbow-back for one inwards.
- **The twists** take half the change of roll: the upper arm's twist bone
  half its roll, the forearm's half the hand's (pronation), so the skin
  does not wring at one end.
- **The wrist seam**: the first-person arm is laid onto the body's - its
  clavicle, upper arm, forearm and their twists at the body's own world
  poses, the hand at its pin. The bare hand's mesh is skinned to the forearm
  and its twist as well (BonePin.h), its wrist edge the body's "Arms" edge
  (ArmStump.h), so with the same bones at the same places the cuff meets
  the body's sleeve or skin; read back in the harness, the two arms agree
  within 0.01 units and 1.3 degrees.
- **What is hidden** while an arm follows: the body's hand model, what
  hangs on its hand bones (weapon, torch, bow) and on its forearm twists
  (the shield) - the first-person hands and what they hold are the ones
  seen. The bare wrist's taper, the forearm stump, the glove elbows and the
  flat wrist lid are off: the arm is there. (The lid seals the hand's
  opening with the depth test off and was drawn over the arm, a dark
  octagon on each wrist in the first harness run.)
- **The shield** is carried with the hand (`CarriedLocal`): it is held by
  its grip, and the contact step reads its disc in the hand's frame
  (HandContactLogic.h). Carried by the IK's forearm instead it left the
  hand as the elbow moved and the shield-wall harness never held it at the
  floor; locking the wrist (the forearm staying where the pins put it, the
  upper arm coming to it) was tried and left the forearm 19-46 degrees off
  the body's arm. The cost: the shield sits 9-19 units off where the IK's
  forearm would carry it, on its straps rather than on the arm - seen in
  the harness's eye dumps as natural for a buckler held out.
- **Left-handed play** (the hands on each other's controllers) keeps the
  body's own arms for now: the body's right arm would reach across the
  chest to the right hand's bone on the left controller.

### The torso

- **The yaw** follows the head (`StepTorsoYaw`, kept in the headset's
  tracking space so a snap turn or the walk direction turning the player
  leaves it): the head may turn `TorsoFollowDegrees` (40) either way
  before it drags the torso along, and a held turn brings it round over
  about `TorsoFollowSeconds` (1.0). A jump no head makes (a recenter) starts
  it afresh. The body is turned about the vertical at the skeleton's top
  (`Bip01`), and the eyes are then put in front of the head bone the way
  the HEAD faces (`BodyRootInput::eyesFollowHeadYaw`), not the body.
- **The chest's own twist** is taken out: the animation turns the shoulders
  (an idle's 8 degrees, an attack's swing), so the line between the
  shoulder joints is turned back to the torso's yaw over the three spine
  bones, at most 75 degrees.
- **The eyes** are `EyeForward` 5.6 units (8 cm) in front of the head bone,
  the Oculus SDK neck model's 0.0805 m ("an average of the male and female
  averages from ANSUR-88", rifty-business.blogspot.com, 2014-08-04, the
  SDK's `OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL`). Enhanced Camera's 14 was made
  for a monitor camera and put the shoulders 12 cm behind the player's.

### The legs

The body is let down to put its eyes at the headset; crouching, that is
more than the legs' own height and the feet went into the floor. The feet
are lifted back by what it was let down, less 3 units (calibration, not a
crouch: `FeetLiftUnits`), each leg solved as two bones with the knee
forward and a little out (`KneePole`), the foot keeping its angle.

### The booking

Every bone written is booked with what it held before (the animation's
pose) and what was written. At the start of the next frame a bone that
still holds the write - the engine left it - is put back first, so nothing
is computed from last frame's answer, and a body given back to the engine
(third person, a menu, the switch off) is given back as the animation had
it. Measured: of the 16 body bones and 12 first-person arm bones written,
the engine rewrote all of them every frame ("0 still held the writes").

### Logged

"Body IK: the third-person skeleton - 22 of 22 bones found", "the right
arm follows its hand - ...", "the body turns with the head - ...", "the body
let down n units ... the feet lifted back ...", "the body's "Weapon" (on
"Bip01 R Hand") hidden ..."; at a hand script's mark one line for the body
(its yaw off the head, the chest's twist taken out, let down, the feet's
lift, nodes hidden) and one per arm (shoulder, elbow, hand, the hand's axis
and where the elbow was sent, the clavicle, the rolls, how far the
first-person arm ended from the body's, what was carried); "Body: n frames
shown since the last mark - x ms a frame". "Camera: at the mark - the eyes
at ... look along ..." gives the camera's own pitch, for reading the
pictures.

### Tests and harness

- `body_ik_test`: the limb reached, too far (grown, short), too near, with
  nothing to go by, on the measured rig; the elbow for a sword held out, a
  raised hand, across the chest (the harness's own numbers), hanging; the
  torso keep-out and the elbow moved round; the clavicle's share and turn;
  the twist split; the torso after the head (dragged, held, a glance, a
  jump, no lag, no time, across the half turn, no number); the chest's yaw
  and its correction; the knees' lift; the arm's gate; the shield carried.
  `body_placement_test`: the eyes along the head's yaw.
- Harness: `body-probe.txt` (the tree), `body-ik.txt` (hands in front,
  raised, across, 1.2 m out to the side, a 45 cm crouch, a 70 degree turn),
  `body-ik-shield.txt` (a shield held, raised, close, lowered onto the
  floor and pressed on), `head-pitch-probe.txt` (the game window against
  the camera) - results in docs/hand-script-harness.md.

### Open

- Not seen in a headset.
- Leaning forward bends no spine: the body still slides under the head.
- Robes that carry the hands in the upper body cannot hide the body's
  hands; they sit inside the first-person ones.
- Left-handed play keeps the body's own arms.
- Menus, dialogue and third person leave the body to the engine (the body
  goes in a menu that keeps the hands).
