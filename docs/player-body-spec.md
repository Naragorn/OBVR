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

"Body IK: the third-person skeleton - 27 of 27 bones found", "the right
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

## Round 2 (2026-10-12): the view down the body, the body steady

The tester's first headset run of the IK: "wenn ich runter schaue sehe ich
in die körpermitte hinein statt am körper herunter. view muss wohl weiter
nach vorne um am körper herab zu sehen statt von oben durch die wirbelsäule
zu gucken. außerdem ruckelt und zuckelt der körper und alles auf einmal.
keine flüssige erfahrung!" - and, a little later, "die ellenbogen wechseln
zu schnell beim drehen des handgelenks. die arme wirken auch noch irgendwie
alien haft." The asked-for references, the Max Payne 2 VR mod (evilSheep;
betotron's: "Max's body follows with a delay") and the Titanfall 2 VR mod
(CircuitLord, "Fullbody IK"), publish no source and no write-up of their IK
(their repositories and pages read 2026-10-12), so nothing could be taken
from them; the one open arm IK of that kind, VRArmIK (dabeschte, MIT, the
Unity code of Parger's thesis), was read for how it treats the hand's
rotation: as a small correction to a position-based elbow, eased over
0.08 s (`rotateElbowWithHandDelay`), with a note that its forward term
makes "horrible jumps" at other weights.

### What was wrong, measured

`Debug.BodyTrace` (new: one "Body trace:" line a frame, read by
`tools/body-trace.ps1` as frame-to-frame differences between a hand
script's marks; harness `body-ik-smooth.txt`) on the build of 835f15a:

- **The body moved with the animation.** The body is stood so the head
  bone's eyes land on the headset, and the head bone's offset from the
  root was taken as the animation left it each frame - so every pose moved
  the whole body under the still headset: the walk bobbed it 3.2 units a
  step and leaned it 6 back as it began; the sneak (a crouch in the room
  asks for it) dropped the animation's head 40 units in one frame, and the
  body jumped 44 units with it. A real walk bobs the eyes with the body; a
  headset does not bob, so the body may not either.
- **The view into the chest.** The eyes were put in front of the head bone
  along the head's YAW alone; a look down took the headset down and
  forward and the body followed it, so the camera looked into the chest
  from above instead of down the front of a body that stays put.
- **The elbow flipped.** The hand's hint for the elbow (the forearm
  straight behind the wrist along the knuckles) was made unit even when
  the knuckles pointed nearly along the arm - a sliver across the line, and
  a small turn of the wrist swung the sliver through zero: the elbow to the
  other side. The torso keep-out searched in whole steps of 10 degrees,
  alternating sides: an elbow at the chest popped between steps.
- **The elbow at arm's length twitched.** The cosine rule's slope runs to
  infinity at the straight arm (68 degrees of elbow a unit in the last
  tenth of a percent, 16 at 2 % short): the head's own sway under the
  shoulder and the controller's tremor moved the elbow by degrees.
- With the inputs standing still the body did not move from one frame to
  the next (0.01 units, 0.01 degrees), and every bone write survived the
  draw ("held 16/16 fp 12/12"): the engine does not rewrite the skeleton
  between the write and the draw. So the stutter was not a race.

### What was built

- **The neck's pivot** (`BodyRootInput::eyesFollowHead`, `headRot`): the
  eyes are put in front of the head bone the way the HEAD faces, pitch and
  roll included - the headset's whole rotation. A look down swings the eyes
  down and forward about the head bone and the body stays where it is; the
  camera orbits the neck, as a head nods on its neck. The head bone is the
  pivot: it sits at the skull's base, 8 cm behind and 8.5 cm below the eyes,
  where the Oculus neck model puts the pivot (8.05 cm, 7.5 cm).
- **The trunk held steady** (`StepTrunk`, BodyIkLogic.h "The trunk"): the
  chain from the root to the head - `Bip01 NonAccum`, the pelvis, the three
  spine bones, the neck, `Neck1`, the head - is held at one pose instead of
  the animation's: the standing idle's, taken once the player has stood
  idle for half a second (not sneaking, no action, no step asked of the
  legs - `PlayerStandsIdle`; and the head's offset reads as a standing one,
  `HeadOffsetStands`), once per skeleton. From then on no animation moves
  the body under the headset. The legs keep the animation; each foot is
  solved back to where the animation put it on the ground
  (`FootSolveWanted`: sunk by more than 3 units, floating by more than a
  quarter, or off sideways) - a body let down for a crouch bends its
  knees, a trunk held higher than the walk's lowered pelvis straightens
  them. Under "Turns with your head" (IKTorso), which already owns the
  trunk's yaw.
- **The elbow**: the hand's hint weighs what it has across the arm's line
  (in forearm lengths) instead of being made unit - a hand pointing along
  the arm says nothing about the elbow, and the body's habit (down, a
  little out and back) decides; the pole is eased from last frame's over
  50 ms (`EasedDirection`, `EaseShare`, after VRArmIK's 80 ms); the torso
  keep-out keeps the elbow 2 units clear of the box, finds the crossing to
  a sixty-fourth of a step, and keeps the way round it went last frame
  while the other is not 30 degrees shorter (`ElbowClear`,
  `kElbowSideHoldRadians`). The two rolls (the upper arm's, the hand's
  against the forearm) are continued the short way from last frame's
  (`UnwrapAngle`, `HalfOfRoll`), so a roll past a half turn no longer flips
  a twist bone round by 150 degrees.
- **The reach softened** (`SoftReach`, `kLimbSoftShare` 0.08): from 92 % of
  the arm's length the distance solved with saturates towards the length,
  the bones grown to land the hand (the zone's own 3 % allowed whatever
  ArmStretch says), so the elbow's slope never exceeds 8 degrees a unit
  and at the full length the elbow keeps 28 degrees of bend. The legs are
  solved exactly (their targets are the animation's own feet).
- Log: "Body IK: the trunk's pose taken from the standing idle after n s -
  the head x y z from the root"; the mark line says "the trunk held at the
  standing pose" or "the animation's"; `Debug.BodyTrace=N` writes the trace.
- The harness's synthetic head now nods and turns about a neck 8 cm behind
  and 7.5 cm below its eyes (`ScriptHeadPosition`, the same Oculus numbers),
  as a real headset moves, its hands placed from the level eyes; before, a
  head pitched in place and the body under it rose with the eye offset.

### Measured after (`body-ik-smooth.txt`, 2026-10-12, four runs)

Frame to frame, the body against the headset: with the hands held still
0.014 units (as before), walking 0.017 (was 0.30 - the bob), the walk's
first steps 0.017 (was 1.45 - the lean), the frame the sneak animation
began 0.03 (was 44); the trunk held at every frame after the first half
second; the feet solved down 3.4-4.1 units while walking (the trunk held
above the walk's lowered pelvis) and up 22-31 for the crouch; the elbow's
direction within 0.3 degrees a frame with a hand slid an inch at a time
across the chest, and never "moved out of the torso" in a step. The eye
dumps show the arms hanging, reaching and crossed, the body looked down at
from in front of its chest. `body-ik.txt` and `body-ik-shield.txt` PASS
again (the wrist 0.00 off its hand, the shield carried, the far hand short
by 20.6 grown x1.15, the turn -35.7 then -1.8).

### Found on the way, not built

- **The game's sneak lowers the view 30 cm on top of the headset's own
  crouch** (the harness trace: the camera 21.7 units lower and 12.7
  further back the frame the sneak began, after the 31.5 the crouch had
  taken it down): the engine's first-person camera sits lower while
  sneaking, and OBVR adds the headset's room offset to it. A crouch in the
  room that asks for the sneak (controls-spec 4.27) therefore drops the
  view twice, in one frame. The fix would hold the engine's camera at its
  standing height above the feet while Full VR sneaks (the headset already
  measures the crouch), as a setting - whether a stick-toggled sneak
  should still lower the view is the tester's call.
- **The camera moves with the pitch on its own** (the trace, runs 3 and 4):
  with the headset's own motion taken out (the script's head pitched in
  place, run 3) the camera stood 2.5 units higher at 60 degrees down than
  level, and in run 4 that share arrived one frame after the headset's
  (0.4 units at a 10 degree step). The engine's first-person camera, or
  OBVR's own arc corrections - not read. The body follows the camera, as
  it must (the camera is what is seen), so it rises those 2.5 units with
  a look down - a slow slide, not a jerk; the view itself shifts against
  the world by the same. A camera item, for Full VR: the headset's motion
  is the truth, the engine's share should be taken out.

### Open

- Not seen in a headset since round 2.
- The trunk's pose is the unarmed idle's, taken once: a save loaded while
  sneaking or fighting keeps the animation's trunk until the player has
  stood idle for half a second ("the animation's" at the mark).
- Leaning forward bends no spine: the body still slides under the head.
- Robes that carry the hands in the upper body cannot hide the body's
  hands; they sit inside the first-person ones.
- Left-handed play keeps the body's own arms.
- Menus, dialogue and third person leave the body to the engine (the body
  goes in a menu that keeps the hands).
- Swimming lays the animation's body flat; the held trunk stands upright
  in it.
