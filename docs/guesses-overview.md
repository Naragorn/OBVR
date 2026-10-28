# Where OBVR still runs on guesses (2026-10-28)

Nadi, 2026-10-27: "Wo in obvr existieren noch Schätzungen statt echten
Messungen". This is the answer as of main ab8ffca.

**How it was found, and what it misses.** A search of `src/`, `docs/*.md` and
`OBVR.ini` for "first guess", "guess", "not measured", "unmeasured",
"assumed", "derived, not" and "[G]", each hit read and sorted. A value whose
comment does not say it is a guess is not found this way: the list is what
the code and the specs admit, not every constant. Values set from the
tester's word in the headset (most of `[Hands]`) are not guesses and are not
listed.

## 1. Tuning values chosen without a measurement

Each would be set from a headset run's log or the tester's word.

| Value | Where | What would settle it |
| --- | --- | --- |
| Throw window 0.15 s | `game/GrabPhysics.h` `kThrowWindowSeconds` | the let-go lines' peaks and their ms before the opening (logged since 155423b) |
| Text glide 0.4 s, from 20°, settled at 2° | `vr/HandHud.h` `viewFollowSeconds`, `viewFollowDegrees`, `kViewSettledDegrees` | the headset's feel |
| Scrape felt from 0.15 m/s | `game/BladeContactLogic.h` (StepScrape) | the headset's feel |
| A blade slices out of a person at 45° off the hand | `vr/HandMode.h` `bodyLetGoDegrees` | the headset; the harness slash lay up to 63° off |
| The hand 4 cm under the controller's origin | `vr/HandMode.h` `handGripUpMetres` -0.04 | the guided fit window replaces it once kept |
| Two-hand take at grip force 0.10-0.15 | free-weapons-spec 153, `TwoHandTakeForce` | the force values in the log |
| A fist by the grip's force at 0.6 | weapon-collision-spec 19.4 | the "fist by the grip's force" lines |
| Yield pose: 50 cm, 12°, 0.9 m/s; a guard's palm | controls-spec (yield), `vr/Yield.h` | the headset; a trace of both palms against an enemy |
| The mouth's place and size from a face's build | `vr/Stow.h`, holding-objects-spec | the reach is the tester's (0.17), the place is not measured |
| The palm is the controller's +x left, -x right | `vr/HandHud.h`, hud-on-hands-spec | a look at the palm HUD (ShowOnLook=1) in the headset |
| Arm-swing run from 0.7 m/s, held 0.35 s | `vr/ArmSwing.h`, `[Hands] ArmSwingSpeed`, `ArmSwingHoldSeconds` | the "Arm swing:" lines and the headset |
| Body capsule radii when not fitted | `game/BladeContactLogic.h` (a torso 13, a thigh 9, a calf 6) | only with `FittedBodyCapsules=0`; fitted is the default |

## 2. The player's body: proportions derived, not measured on people

| What | Where |
| --- | --- |
| Arm span from the height | `vr/BodyMeasure.h` |
| The shoulder's top and breadth for the two-hander held in one hand | `vr/OneHandTwoHander.h` |
| Which way the elbow points (from the hand, as the VRST 2018 paper) | `game/BodyIkLogic.h`, player-body-spec |
| The wrist's and forearm's twist weights | `game/BodyIkLogic.h` |
| The hand's point between its bones | `game/HandBodyLogic.h` |
| The bow's grip on the hand | `game/WeaponGrab.h` (the sword and the claymore are measured) |

## 3. The engine's behaviour, not observed

| What | Where |
| --- | --- |
| When an NPC's blow is resolved against the player's blade | weapon-collision-spec (the largest risk named there) |
| The dialogue zoom's middle | `game/DialogZoom.cpp` |
| Whether the engine resets the body's root each frame | `game/BodyPlacement.h` |
| The node the first-person arms hang from | `game/FirstPersonArms.h` |
| Which of a draw's frames the eye sees | `game/WeaponTimeline.h` |
| The riding base's frame | controls-spec (riding) |
| The stick's turn in Full VR with HeadLook=0 | controls-spec |
| A hand put into one's own chest | weapon-collision-spec |
| Havok against a living person's controller (a phantom) | physical-combat-spec 3.1 |
| Within the engine's activation reach | holding-objects-spec |

## 4. Performance, not measured

performance-spec: the cost split of a dear cell's lights; what keeps the
submit in after 3.5 ms of work; the loading screen's GetTickCount; the
per-pixel cost split; a door's 120 ms load frame; the Pimax's GPU half of
the fort scene; which pass draws how many; a detour of the render's tail.
controls-spec: performance with human NPCs against creatures.

## What to do with it

Most of section 1 is settled by one headset round each, with the log lines
that already exist; sections 2 and 3 only where something feels wrong; and
section 4 when the performance work reaches it. Nothing here is a known bug.
