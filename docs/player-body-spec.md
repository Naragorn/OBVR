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
- Robes that carry the hands in the upper body (the Black Hand Robe, the
  Shrouded Armor: slots upper body, lower body, hands, feet in one mesh)
  showed the body's hands inside the first-person ones with other fingers
  (a Quest 3 tester, 2026-10-17: "A lot of fingers in some suits"). Fixed
  2026-10-18: their hand geometry is a NiTriShape named plainly "Hand"
  under the node "UpperBody  (<form>)", no "Hand  (<form>)" node, and the
  hides passed it by; BodyNodeHiddenOnArm (BodyIkLogic.h) hides a "Hand"
  under any slot model's node. Harness robe-hands.txt; headset open.
- Left-handed play keeps the body's own arms.
- Menus, dialogue and third person leave the body to the engine (the body
  goes in a menu that keeps the hands).
- Swimming lays the animation's body flat; the held trunk stands upright
  in it.

## Round 3 (2026-10-12): the open collar, the wrist

The tester, after round 2: "IK: nach unten schauen: ich sehe immernoch in
den körper rein. arme wirken super, nur wenn ich die hände voll drehe
sodass die senkrecht nach aussen schauen (also linke hand daumen runter
handfläche nach links, rechts vice versa) dann verkleinert sich das
handgelenk zu sehr. ruckeln nun weg!"

### Looking down: the open collar

- **What was seen.** Round 2's own eye dump at 65 degrees down
  (`body-ik-smooth`, 2026-10-12) shows it: at the bottom of the picture the
  collar of the body as an open ring, and its inside. The head bone is
  collapsed for the draw (Enhanced Camera's way, `PlayerBody.cpp`), the
  head's mesh with it, and the neck went with the head - so nothing closes
  the body at the neck. The Bip01 chain measured in the body probe: the head
  bone 6.3 units above the neck bone, the clavicles at the neck bone's
  height. A real chin hides that place; nothing here does.
- **Built.** The body steps back under a bowed head, along the way the face
  looks (`BodyLookDownShift`, BodyPlacement.h; `kLookDownBackUnits` 20):
  nothing above 20 degrees down, then the square of the way from 20 to 90
  degrees times 20 units - 8.3 units at 65 degrees, 17.2 at 85, 20 straight
  down. The way back is the headset's up axis's level part, so it is
  defined looking straight down, where the forward's level part runs out.
  Always on, no setting: it was `[Body] LookDownBack` for a day, and the
  tester took it out ("kann als einstellung weg. muss nämlich immer an
  sein"). Logged once "Body: stepped back n units under a head bowed d
  degrees - the open collar out of view".
- **Measured** (eye dumps, `body-look-wrist` and a one-off sweep with the
  then-setting at 16): 8 units hid the ring at 65 degrees but not at 85; 16
  hid it at 85, and at 65 put the whole torso out of view - so the shift
  grows with the look, not linearly. With the curve: at 45 degrees the floor
  and the hands; at 65 the feet, the legs and the belt below; at 85 the
  belly, the belt and the legs from above; no ring at any of them.
- **The cost.** The shoulders are that far behind the headset looking
  down, so the arms reach forward to the hands - unseen in the dumps, to
  feel in the headset.

### The wrist turned: the roll spread

- **What was wrong.** The hand's roll on the forearm (the controller's hand
  against the solved forearm, less the animation's own) went half to the
  forearm's twist bone, held to 75 degrees, the whole to the hand, none to
  the forearm. Read from the game's meshes (pyffi on
  `meshes\characters\_male\upperbody.nif` "Arms" and `hand.nif`, the right
  arm in its bind pose): the twist bone sits at the elbow and weights the
  forearm's skin as a gradient up to the wrist (0.07 by the elbow, 0.74 near
  the wrist); the wrist's ring is blended forearm 0.17, twist 0.41, hand
  0.41. At a roll of 180 degrees that ring, its weights turned 0, 90 and 180
  degrees, kept 0.48 of its width (linear blend skinning), and past 150 the
  twist bone stood while the hand went on.
- **Built** (`ForearmRollsFor`, BodyIkLogic.h): the roll spread evenly - the
  forearm bone a third, its twist bone two thirds, the hand the whole, up to
  270 degrees either way (the memory held a little past it,
  `HoldForeRollMemory`). At 180 degrees the elbow keeps 0.87 of its width,
  the forearm 0.87, the wrist's ring 0.74 (derived from the weights).
- **Measured** (`body-look-wrist`, the controllers turned 30 degrees a tenth
  of a second to 180 and on to 210): the roll followed the whole way, no
  flip (the right hand 61.5 -> -190.7 -> -205.9, the left -102.8 -> -173.9
  -> -160.2); the eye dumps show both wrists full, the forearms as they
  should be.
- **Tried and left out: the elbow swung with the roll.** VRArmIK
  (github.com/dabeschte/VRArmIK, MIT, `rotateElbowWithHandRight`) turns the
  elbow about the shoulder-wrist line by the roll past an offset. Built the
  same way first (the roll past 60 degrees, three quarters of it, at most
  90), it failed in the harness twice: the roll at rest is not 0 - the
  right hand 61-82 degrees, the left -103 to -113, the controllers held
  pointing ahead - so the left elbow swung 36 degrees with the hands at
  rest; and a hand turned half round in one step swung both elbows across
  the chest, since which way round a half turn went is not known from one
  frame. Kept for later, with the rest roll taken out first.

### Open bug (the tester, 2026-10-14): the wrists at the game's start

"die sind auch verdreht und verkleinert beim speilstart. erst nach paar
sekunden sehen die normal aus" - next to the open wrist shrink when the
controllers turn outward (Task #110). The harness did not see it:
`body-wrist-start.txt`, the hands held still at the chest from the start,
gave the same rolls from the first mark on (the right hand 67.6 to 68.1
degrees on its forearm, the left -107.2 to -107.5, the first-person arm 0.00
units off) - but its hands are the script's from the first frame, and the
tester's are the controllers'. The tester's own log of that afternoon shows
the first world frames in third person ("the player was in third person -
put back into first") and the hand bones measured thousands of units off the
grip in the first frame. Not decided from that. Since 2026-10-14 the IK logs
both arms every 0.25 s for the first 8 s after a load ("Body IK: 1.25 s after
the load - ..."; the session's start and the next three times the body is
taken up, `StepStartTrace`, body_ik_test): the hand's roll on its forearm,
the forearm's and the twist bone's, and how far the first-person arm sits
off - a start of the tester's shows what changes in those seconds.

The tester's first start with the trace (2026-10-14, 20:13): the first-person
arm lay on the body's within 0.00 units and 1.3 degrees at every step; the
rolls on the forearms swung with the controllers - the right hand 49, 12, 36,
0, 64, -4, -96, -15, 46, 40 degrees over the first 3.3 s and 55 to 5 over the
rest, the left -11 to -92 and back - large jumps within a quarter second, as
of controllers just picked up. Nothing in the numbers names a shrink; the roll
spread (ForearmRollsFor) narrows the wrist's ring only towards a half turn.
His second start (20:17) the same: the arm within 0.7 degrees throughout, the
right hand's roll -72, -68, -48, 23, 62, -35, -90, -14, 31 over the first
2.1 s. What the tester saw and for how long, with the hands held still, is
still to be matched against these lines.

### The view measured: Debug.BodyView

The tester asked whether the harness measures the view down the body, not
into it (2026-10-12). Until then it did not: "Body: stepped back" said the
step back ran, and whether the collar showed was read off the eye dumps by
eye. Now `Debug.BodyView` (`game/BodyView.h`, `BodyViewLogic.h`) casts each
eye's rays at the body at every mark of a hand script - the body's shapes
skinned on the CPU as the draw skins them, the camera's frustum and near
plane - and says how much of each view is the trunk from outside and how
much its inside, with a picture of the rays (docs/hand-script-harness.md,
"What the pictures cannot show"). `body-look-wrist` expects "down the body"
at 65 and 85 degrees and rejects "into the body" at every mark;
`body-look-into` is its control, the eyes put inside the chest
(`EyeForward=20`, `EyeUp=-17`), where the check has to say "into the body".

- **Checked against the game** (2026-10-12, the headset's display off, so
  the monitor's view was the camera's and no step back ran): the rays'
  outline of the body at 85 degrees down matched the game window's shot,
  and that view came out "into the body" on 3.1 % of it - UpperBody 2.6 %,
  the collapsed face 0.4 % - the collar round 2 showed; at 65 degrees the
  inside began 5.4 degrees below the view's bottom edge.
- **The body read** (the harness's character, a body replacer's meshes:
  UpperBody:0 463 vertices, UpperBody:1 with a NiStencilProperty, Arms,
  LowerBody:0 and :1, Hand, Foot, FaceGenFace, FaceGenHair): nine shapes,
  every vertex's weights adding up to one, every shape wound outward by its
  normals (FaceGenHair 1292 of 1302 triangles, UpperBody:1 715 of 718, the
  rest all), 10 570 triangles, about 4 ms a mark.

### The chest in view (the tester, 2026-10-12 evening - built the same evening, headset open)

In the headset the tester confirmed the look down: "ja schaue nicht mehr in
den körper hinein! das ist schonmal gut" - and asked for more: "hatte mir
aber irgendwie erhofft auch die brust des player chars schauen zuu können
und nicht nur auf den unterkörper wenn ich nach unten schaue. ist das
möglich?" The first answer was a cap over the collar ring, and he asked for
it ("ja bau die kappe"). Measured before building, the cap turned out to be
useless; what is built instead is below.

**What is actually seen, measured** (`upperbody.nif` and `skeleton.nif` read
with pyffi, the view check of BodyView, the harness's dumps of the day):

- The eyes under a bowed head are not above the collar - they are on the
  chest. The head bone stands 2.5 units above the collar ring's centre; the
  eyes sit 5.6 units in front of it and 6 above (`EyeForward`, `EyeUp`)
  and swing with the head. Bowed 65 degrees they are 7.8 units in front of
  the head bone and 2.5 below it - at the ring's own height, 4 units in
  front of the neck; bowed 85 they are 6.5 in front and 5 below, where the
  chest's front is 6.6 (the chest's front runs from 4.9 units before the
  head bone 5 below it to 10.5 at the pectorals 17 below, and 9-10 on down
  to the belly). So the eyes stand on the skin, and below them the chest
  bulges 3-4 units further forward than the eyes.
- The camera's near plane is 10 units (14 cm): nothing nearer the eyes is
  drawn. The chest's wall right under the eyes is nearer, so it is cut
  away, and through the cut the inside shows - the floor through a
  one-sided body, a shirt's inside where a shirt is two-sided (the harness
  character's UpperBody:1 carries a NiStencilProperty). That is the "ring"
  the dumps of round 3 showed: BodyView, run without the headset, found the
  trunk's inside on 3.1 % of the view at 85 degrees, in the middle of the
  body's green, exactly where the cut wall is.
- The collar ring itself is behind and above the eyes at every bow (at 85
  degrees 6.5 units behind, 2.6 above) and never in view. A cap over it
  would never be seen; the step back of round 3 did not hide a ring but the
  cut.

**Built: a nearer near plane, and the step back scaled to it.**

- `[Body] NearPlane` (default 4 units, 0 for the game's own) is written into
  the game's camera every frame beside the field of view
  (`game::SetFrustumNear`, game_camera_test; CameraHook.cpp, the frustum
  block). The log says "Camera: the near plane is 4.0 units (the game's own
  10.0 ...)" the first frames and whenever it changes. With it the chest's
  wall is cut only within 4 units of the eyes.
- The step back follows the near plane: twice it at most, looking straight
  down (`LookDownBackUnitsFor`, body_placement_test) - 20 at the game's 10,
  as measured in round 3; 8 at 4, so 6.9 at 85 degrees and 3.3 at 65. Worked
  through on the measured profile with the headset's 44 degrees under the
  look: at 85 degrees the cut wall lies below the view's bottom edge from 5
  units of step back on, and the chest is in view from 9.5 units below the
  head bone - the collarbones - instead of from 22 (the belly) as with the
  20; at 65 degrees from 8.5 units below instead of 15. The "Body: stepped
  back" line names the near plane it was computed for.
- The cost, stated in the INI: depth precision far away. A 24-bit depth
  buffer tells surfaces apart by about z^2/(n * 2^24) units at depth z: at
  10 the game's own, at 4 the far surfaces fight about 2.5 times sooner (6
  units apart at 300 m instead of 2.4). Whether that shows on the distant
  land is the headset's to say; NearPlane=0 puts everything back as it was,
  the 20 units of step back included.
- Not changed: the eye offsets (bowed, `EyeUp` runs the way the face looks
  and acts as a step back, `EyeForward` raises the body towards the eyes),
  the neck pivot. Not built: the cap (useless, above), the hand's stencil
  lid for the trunk (four openings; a ray in through the neck and out
  through the waist crosses no wall).

**Seen in the headset** the same evening (2026-10-12, the build of
fd9ec72): "jo nun geht IK wie erwartet! GJ" - the look down as expected,
the chest in view. Not said: whether the distant land flickers for the
nearer plane; not run in the harness (the tester was playing).
body-look-wrist runs with the default near plane now and its view check
has to say "down the body" at 65 and 85 degrees still - with more of the
chest in the green. A crouch with the head bowed not tried.

### Open

- The chest in view: built with the near plane at 4 (above) and confirmed
  in the headset ("geht IK wie erwartet"); whether the distant land
  flickers for the nearer plane is not reported on yet.
- Why the rest roll differs between the arms (61-82 against -103 to -113,
  not mirrored): the hands' grip on the controllers, or the solve's frame -
  not looked into. It changes nothing seen now that the roll is spread.
- A crouch with the head bowed - the body let down and stepped back at
  once - not tried in the harness.
- Seen in a headset 2026-10-12 evening: the look down no longer looks into
  the body, and with the near plane at 4 the IK "wie erwartet" (the
  tester). The wrists turned: not reported on.
- **Open bug (the tester, 2026-10-13 afternoon): "die armgelenke
  verkleinern sich noch zu sehr beim drehen der controller nach aussen"** -
  the wrists (and the elbows?) still pinch too much when the controllers
  are turned outwards (supination: the palms up and out). The roll spread
  above (`ForearmRollsFor`: the forearm a third, its twist bone two thirds,
  up to 270 degrees) was measured for the turn inwards (`body-look-wrist`
  turns to 180 and 210 one way); the outward turn's rest roll is not 0 (61
  to 82 degrees on the right, see above), so an outward turn may run the
  twist past where the weights hold the ring open. To do: a harness sweep
  turning outwards with the eye dumps and Debug.BodyView's wrist width,
  then either a lower share on the hand for that side or the elbow swung
  with the roll (VRArmIK's way, tried and left out above). Not built.

## Round 4 (2026-10-16): the skeleton read for sense before the body is placed

Status: **built 2026-10-16 (the guard and the "Body check:" line); the cause
in the reporting user's game open.**

A report on 0.3.0-beta1 (a Quest 3 over Virtual Desktop, Windows, Full VR):
"the screen is constantly being obscured by some blue artifacts ... when I
open my inventory or the menu, the flickering disappears completely and the
game looks normal. Also the sound in game stutters." His OBVR.log.prev:

- Before the load the scene's bound is a worldspace's (centre -19374 68890
  16080, radius 371976). On the body's first frame after the load: "Body IK:
  the body let down 55424.2 units to put its eyes at the headset", "the
  body's upper arm 47455.8 and forearm 32058.1 units" (the measured skeleton:
  21.85 and 14.78), "the wrist at 40185479168.0 -33926496256.0
  591464687976054784.0 for the hand at -2105.1 63035.6 1182.2" - and the
  next probe of the scene's bound reads centre (5702520832, -19320637440,
  293162063759409152), radius 293162063759409152. The body's bones stand in
  the scene graph tens of thousands of units apart, and the arms solved on
  them run across the whole view. "27 of 27 bones found", "0 still held the
  writes": the engine re-poses the bones every frame, so the lengths are what
  its own transforms say, not a leftover write of OBVR's. The ratio to the
  measured skeleton is the same for both arm bones (about 2170), a scale
  rather than noise.
- The hands then read not-a-number: "Measure: right hand - "Bip01 R Hand"
  ... -nan(ind) units ahead of the grip" (the first-person arm laid onto the
  body's), later "Body IK: the right arm left to the body - no solve (... a
  number that is none)" and "forearm stump ... stump ends at (-nan(ind)
  -nan(ind) -nan(ind))".
- The session ends in "Watchdog: no frame has ended for 10 seconds - the
  render thread's last recorded step was "overlays submitted"".
- In menus the hands go ("Hands: menu Pause (0x3F5) - the hands go") and no
  arm is solved, which is why the picture is clean there.

The same build on the tester's own Windows and Linux setups reads "upper arm
21.8 and forearm 14.8 units" and "let down 1.4 units".

**Built:** `JudgeSkeleton` (BodyPlacement.h; body_placement_test: the
measured skeleton, a crouch, races at half and twice the scale, arms not
read, the reported reading, every verdict and its words). Before anything is
written, the skeleton's reading - Bip01's world scale, its parent's scale and
the lengths of the parent rotation's columns, the head's distance from
Bip01, the arm bones' lengths - is judged against a body's: scale 0.05 to
20, columns 0.8 to 1.25, the head 30 to 300 units at scale one, the upper
arm 5 to 80, the forearm 4 to 60. One that reads like no body is hidden
again for the frame, the IK stands down (`StandDownBodyIk`), and the log
says what was read, once per skeleton when sane and whenever the verdict
changes: "Body check: Bip01 scale 1.000 under a parent of scale 1.000
(columns 1.00 1.00 1.00), the head 112.4 units from it, the right arm 21.9 +
14.8, the left 21.9 + 14.8 units (0 = not read) - sane". The harness:
`body-ik.txt` expects the line and rejects "the body is left hidden".

**Open:** what makes that skeleton read so. Candidates, none read: a
skeleton or body replacer with scaled bones, a race at a scale the engine
keeps on a node OBVR reads as the parent, or the engine not placing the
third-person skeleton in first person in that game at all (the setup OBVR
patches in at the POV switch may not have run for his load). His next log's
"Body check:" line names the scale, the columns and the lengths; `[Body]
Visible=0` is the immediate test for him - no body, no IK, the first-person
hands and arms as pinned.

### Beta 3 (2026-10-16, after the beta-2 log): the skeleton comes apart in the placement

His beta-2 log turned the picture round. Frame by frame, lines 1543 to 1569:
"Body check ... the head 123.7 units from it, the right arm 23.9 + 16.2, the
left 23.8 + 16.0 units - sane" (a body a tenth larger than the measured one;
its parent's rotation has a third column of length 1.10, where ours reads
1.00 - something in his game scales the player in that matrix); then "the
body let down 55423.1 units"; then the solve's "upper arm 47455.8 and forearm
32058.1 units" in the same frame. The skeleton is sane before the placement
writes Bip01's position and runs the engine's update of it, and 2170 times
out after. Later checks alternate between sane and off from frame to frame,
which is the flicker. `[Body] Visible=0` took the flicker away for him at
once ("gone absolutely").

The hands followed from it: with the first-person arm laid onto the torn
body, the first-person hand is written under a torn parent and lands off the
controller; every guided fit he ran then froze the hand where the IK had
pushed it, measured that as the hand's place and saved it - his grips grew
0.23, 0.45, 0.62, 0.85 m over four rounds, so the hands stood "far away"
even with the body off, until the twelve calibration keys were put back.

**Built:**

- `ArmBonesReadLikeAnArm` (BodyPlacement.h): the solve refuses an arm whose
  bones read longer than any arm's right before the lay-on ("the arm's bones
  read longer than any arm's - nothing is laid onto them"), so no
  first-person arm is laid onto torn bones whatever came before. The bounds
  tightened to twice the measured skeleton (upper arm 44, forearm 30, head
  200 units at scale one): his "73.0 + 46.7" and "51.2 + 57.7" had passed
  the first bounds and were laid onto.
- The check again after the placement (`SkeletonStillReadsLikeABody`): the
  reading taken once more after the write and the engine's update, the body
  let down further than any body's height counted as off; off hides the body
  for the frame and stands the IK down. Logged once per skeleton and whenever
  it changes: "Body check after the placement (Bip01 let down 2.3 units): ...".
- The dry run (once per skeleton, on its first sane frame): the engine's own
  update of Bip01 with nothing written, the reading taken again and logged
  ("Body check after the engine's own update of Bip01, nothing written: ...").
  Off there means the update tears his skeleton by itself; sane there and off
  after the placement means the write does. His next log decides.
- `FitReadsLikeAHand` (BonePin.h): a fit whose own grip lies more than 0.35 m
  from the controller, or carries a number that is none, is refused and not
  saved ("Hands: the right hand's fit refused - ..."); the hand keeps the fit
  it had.
- Tests: body_placement_test (the arm helper, the torn-but-under-the-old-bound
  case), bone_pin_test (the fit refused). Harness `body-ik.txt` expects the
  dry-run and the after-placement lines and rejects "the body is left hidden".

**Open:** which of the two, our write of Bip01's local position or the
engine's update, tears his skeleton, and what in his game makes the parent's
rotation carry a scale. The write goes through `BodyRootLocalPos`, which
inverts the parent's rotation by its transpose; a rotation with a scaled
column has no such inverse, and a tenth of error there is not 2170 times - so
the dry run is the next fact to read.

### The cause (2026-10-16, the same day): a race's height is a scale in the player node's rotation matrix

The reporting user plays an Altmer and runs no mods. Oblivion's Altmer are
1.10 tall (the race's height), and his log's "columns 1.00 1.00 1.10" is
where the engine keeps that: a scale of the third column of the player
node's rotation matrix, not in the node's scale field. Our tester's
character and the harness character are races of height 1.00, so their
matrices are rotations and nothing of this ever showed.

OBVR took the transpose for the inverse of every engine matrix it derived a
local from - `InverseRotation` (core/Rotation.cpp, "rotation matrices are
orthonormal, so transpose is their inverse"), and `ik::Transposed` in the
body IK for the root's turn, the spine's twist, the clavicle, the thigh and
the first-person arm laid onto the body. The transpose of a scaled matrix
is not its inverse: a local written as `transpose(parent) * target` keeps
the scale, the engine's update multiplies it in again, and where the target
itself carries the parent's scale (the root turned: `transpose(parent) *
(turn * root)`) it is squared at every bone written down the chain. 1.1
squared nine times down to the wrist is what the log showed: the body let
down 55423 units, the arm bones 2170 times long, the wrist at 5.9e17, the
hands at not-a-number. Half the races have a height off 1.00 (Nord, Orc,
Altmer, Argonian, Redguard), so this was waiting for most testers.

**Built (beta 4):** `InverseMatrix` (game/NiMath.h) - the adjugate over the
determinant, the transpose only for a matrix too close to singular to invert
(a node collapsed to scale zero). `InverseRotation` is that now, and every
`ik::Transposed` that stood for an inverse of an engine matrix in game/BodyIk.cpp
and `CarriedLocal` is `InverseMatrix`. Tests: rotation_test (a rotation's
inverse its transpose; a race's height cancelled from both sides; a sheared
matrix; a flat one's fallback), bone_pin_test (the hand under a parent with
the height), body_placement_test (the root under such a parent), body_ik_test
(the shield carried under one). Harness: `body-ik-tall.txt` sets every race's
scale to 1.10 at the main menu before the save loads (the new `console-menu`
line, test/HandScriptConsole.h) and runs body-ik.txt's moves on the taller
body, expecting "columns 1.00 1.00 1.10" and every check sane. The race's
scale set at the main menu did not reach the player built from the save (run
1: every line taken and run, the body still 1.00 and 112.4), so the tester
made an Altmer save (the Imperial Prison cell, `OBVRHarnessTall`): run 2 PASS -
the body read exactly the reporting user's numbers before the placement
(columns 1.00 1.00 1.10, the head 123.7, the right arm 23.9 + 16.2) and
stayed so after the engine's update and after the placement ("let down 1.8
units" where his log read 55423), every arm reached, the far hand fell short
by 15.9 grown x1.15, the trunk held, nothing left to the body. body-ik.txt on
the usual body PASS with the new inverse, the same numbers as before it. The
guards of beta 2 and 3 stay as they are, for the next skeleton that reads
like no body.
