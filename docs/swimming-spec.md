# Swimming with the arms

Nadi, 2026-11-27: "go schwimmen mit den armen". While the game has the player swimming, the player's own arm strokes
move them through the water. The stick keeps swimming as it does today.

Status: built on branch `swim-1127`, not yet tried in the headset. What was read in the EXE, what the harness
measured and what is a first guess is marked in each place (read, measured, derived, guessed).

## What the player does

- Close a hand (the grip) to hold the water, pull it through the water, open it to reach out again. The swimmer is
  carried the other way from the hand.
- Any stroke works: crawl (one hand at a time), breaststroke (both hands), a single hand paddling, up and down. The
  direction is where the hands push, not where the head looks. Nothing turns the player but the player.
- Both hands holding the water count as one stroke (their mean), so a breaststroke's sweeps out and in cancel and only
  the pull back is left.
- A light stroke already swims. The speed grows with the hands' speed, not in step with it.
- After the stroke the swimmer glides and slows over a few seconds. A stroke never slows the swimmer, except one that is
  clearly a push back against the glide.
- The stick still swims. Stick and arms together never go faster than the faster of the two alone.
- A short buzz in a hand as it catches the water.
- A hand that holds something does not swim: a drawn weapon, a torch, a bow, a grabbed object, a grip that is a
  holster's reach. A closed hand with nothing to take in the water holds the water.

## The design, and why

### How the swimmer moves: a step added in the player's Move wrap

Read (Oblivion.exe 1.2.416, OBNG `re/export/rea-all/functions/0x63c730.c`, 0x5efae0.c, `OBNG/knowledge/player-movement-pace.md`):
the player's walk is `UpdateMovement` 0x0063C730 handing a step (x, y, z in the actor's frame) to `Actor::Move`
(vtable 0x1B4), whatever the character controller's state. OBVR already wraps that call for the room walk and the
analog walk (`game/RoomFollow.cpp`, `OBVR_PlayerMove`): its step goes through the character controller, so walls, the
surface and the shore stay the game's. The stroke is one more step there, `velocity x dt`, turned from the world into the
actor's frame.

Why not the stick route alone (the analog walk's pace factor 0x00B14E58): that factor is the animation's rate, at most 1
(the sprint goes past it at 0x00672E0D). Its direction is the stick's horizontal octant step; the vertical part and a
pace of several swim speeds with a glide behind it are not in it. A step in the wrap takes any 3-D velocity and leaves
collision and the surface to the game.

The game still sees a swimmer: while the arms swim and the stick is idle, the analog walk is given a tiny push in the
stroke's horizontal direction (`SwimNudge`). Its own step is then a few per cent of the swim speed, the swim animation
plays, and the move keys show a direction (guessed, the rival mod's finding: Athletics trains only while the controls
give a direction; not verified here). The engine's own step and the arms' are not added: the sum is cut to the longer of
the two (`CombineSwimStep`), so the tiny push costs nothing.

### Speed

The game's own swim speed scales the strokes: `0x005E3920`, the swim walk speed (units a second; same shape as the
walk and run speed getters the analog walk already calls, first bytes `83 EC 10 56 8B F1 8B 06`, read in the EXE; its
body is the formula over Speed, encumbrance and Athletics). A stroke with the hands at 1 m/s swims at 3 swim speeds, the
hands' speed raised to 0.375 (the rival mod's 0.4, as three square roots: no pow in the plugin's runtime), at most 4
swim speeds, times `[Hands] SwimStrength`. These numbers are the rival mod's playtested values; here they are a guess
until the headset.

### Hand speed

Each hand's velocity relative to the head: SteamVR's controller velocity less the head's velocity (differenced from the
head's pose), so walking about in the room and leaning swim nothing. A head jump faster than 4 m/s (a recenter) counts
as no motion.

### The catch

A hand holds the water when the player is swimming, the hand is tracked, its grip is closed (the same hold the holsters
read, `HandModeResult::rightHold/leftHold`: pull or click, GripToggle included) and it is not busy. Busy:

| Hand (role) | Busy when |
| --- | --- |
| weapon hand (right role) | a weapon, staff or spell is drawn; its grip is a holster's (`holster.rightClaimed` or at a place); a grab holds something with it |
| other hand (left role) | a torch is in it; a bow or a two-hander is drawn; its grip is a holster's; a grab holds something with it |

The roles are the game's roles, so a left-hander (roles swapped) is mirrored for free; the buzz goes to the physical
controller (`vr::HandDeviceForRole`).

A fist made to catch the water would raise the fists by the fist feature (a closed hand with no weapon drawn, read
from the fingers' curl or, on a Touch controller, the trigger); while a hand holds the water, and for half a second
after, the ready click is dropped (`SwimDropsReadyClick`). The grip is read through `HandModeResult::rightHold` /
`leftHold`, so a hand-tracked hand's closed fist (its grip value) catches the water the same way.

### The stroke

- The held hands' velocities are averaged: one stroke, `w`.
- A hand slower than 0.15 m/s is the tracking's wander, no stroke.
- The stroke counts only while it goes on away from where it started: the way the held hands have gone since the catch
  (`reach`) must not point against `w`. A hand held through its recovery pulls nothing. The reach starts again when the
  set of held hands changes.
- The pull is `-w`'s direction, `StrokeSpeed` long. The swimmer's glide velocity takes it:
  - along the glide or from rest: the glide's part along the pull is raised to the pull's speed, the sideways part fades
    in 0.25 s (the glide turns with the stroke);
  - already faster that way: nothing, a stroke never slows;
  - against the glide: only a clear push back replaces the glide with the pull. Clear is: the held hands' mean goes at
    least half as fast as they move apart or together (one hand alone is always clear). A breaststroke's sweep out is
    not a push back.
- The glide: the speed falls by e over 6 s when it is slow; the rate grows by one for every two swim speeds of
  speed (`SwimGlideRate`), so a fast glide ends sooner. Under 2 units a second it stops: the game has no momentum in
  water, so the swimmer hangs where they stop (the rival mod's note, not checked here).
- No stroke or glide outside the game's swimming state (`PlayerCharacterState() == 5`, `KickSwimming`): leaving the
  water clears the glide.

### Gates

On when `[Hands] SwimStrokes=1` (default on), Full VR with the hands running, the player in the world with no menu up, not
riding, no fly camera, and the game's own state "swimming".

## The settings

| Key | Default | Meaning |
| --- | --- | --- |
| `[Hands] SwimStrokes` | 1 | arm strokes move the swimmer |
| `[Hands] SwimStrength` | 1 | the stroke's strength, 0.5 to 3 |

Row "Swim with your arms" and "Stroke strength" on the Moving page of the settings menu. Not asked on the start guide
(a default-on switch with no surprise; the guide's revision is raised by hand with every beta, `ui/GuideRevision.h`).

## What was not built, and what only the headset answers

- The picture: the game's own render; no under-water changes, no body in the water.
- The engine's breath, drowning and Water Breathing are untouched.
- Whether the stroke feels like swimming, whether 3 swim speeds at 1 m/s is too fast or slow, the glide time, fatigue in
  the arms, whether the grip-close detection catches reliably at the speed of a stroke, and whether `GripToggle` players
  find a toggled catch odd (one grip catches, the next lets go).
- Whether Athletics trains from the nudged controls.

## Code

- `src/vr/SwimStroke.h`: the pure stroke logic (tests/SwimStrokeTest.cpp, `swim_stroke_test`).
- `src/game/SwimStep.{h,cpp}`: the velocity asked of the player's Move, added in the wrap (`RoomFollow.cpp`).
- `src/camera/hands/Swim.{h,cpp}`: the thin caller from the analog walk's step (`Locomotion.cpp`).
- Harness: `tools/hand-scripts/swim-arms.txt`.
