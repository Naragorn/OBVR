# Doors by grip: open a door with the hand instead of A (proposal 2026-11-11)

Status: **B built 2026-11-11** (`[Hands] DoorsByGrip=1`, `DoorGripMoveMetres=0.12`,
vr/DoorGrip.h, camera/hands/Doors.cpp; section 5). A, C and D stay proposals;
the locked-door rattle is not built (a locked door gets A's own handling).

The tester (2026-11-11): "lass uns auch mal brainstormen wie man türen statt
mit A mit grip öffnen kann".

## 1. What the engine gives us

- Most of Oblivion's doors are **load doors**: activating one fades out and
  loads the other side. There is nothing to swing open; the "door" is a
  trigger with a model.
- Doors inside one cell are real doors that open and close. *Guessed, not
  read:* they move by an open/close animation of their own model. Whether
  that animation can be driven from outside (scrubbed to a hand's angle) is
  **unverified**; the cheapest step that settles it is a look at the door
  activation and its animation in the decompile (section 4, C-check).
- OBVR already has: the activation A and the laser use, opening by reaching
  for containers (`[Hands] ReachOpens`, container-touch-spec), the grip's
  hold with its hysteresis (vr/GripHold.h), haptics, and the engine's screen
  fader around loads (screen-fader memory, fe935c3).
- Door models carry **no handle node**. A handle zone has to be derived from
  the door's bounds.

## 2. Options

| Option | What | Cost | Why |
| --- | --- | --- | --- |
| A. Grip on the door = activate | The empty hand on the door, the grip pressed: the door opens as with A. | small | Reuses the activation; new is only "empty hand on the door leaf, grip went from up to down". Risk low. Feels like a button, not a door. |
| **B. Grab the handle, pull or push (recommended)** | Grip on the door leaf at handle height, then 10-15 cm along the way the door opens (pull or push). The move opens it; a light haptic rattle announces it. | small to medium | Physical, and no accidental opening. Reuses the grip hold and haptics. New: the opening direction from the door's plane, a handle zone (the half away from the hinge at about 1 m height, from the bounds), the move threshold. Harness-testable. |
| C. A real hinge | Doors inside a cell: their open animation follows the hand's angle - ajar, open, closed again. | large | Engine unknowns: whether the animation can be scrubbed, whether collision follows, whether NPCs and pathing cope with a half-open door. Only a minority of doors. |
| D. Load doors opened a crack | As B, but the pull already starts the engine's fade, then the load. | small on top of B | Uses the engine's fader OBVR already handles; the step into the load feels continuous. |

## 3. For every option

- **Locked door:** grip and pull rattles the door in the controller with
  the engine's "locked" sound; a second pull opens the lock's minigame
  (as ReachOpens does for a locked chest). Small.
- **A stays:** for seated players and anyone who prefers it. A switch
  "doors by grip only" is possible.
- **Conflicts:** the grip also grabs items and draws weapons. A door counts
  only when the hand is empty and on the door leaf; anything else held or
  grabbable wins.
- **Tests:** pure logic for the zone, the direction, the threshold and the
  conflicts (100 % flow coverage); harness scenarios: a load door pulled, a
  door pushed, a locked door rattled, a held item not treated as a door.

## 4. Recommendation

**B now, with D and the locked-door rattle.** It covers every door,
including load doors, and feels most like VR. C only after the C-check:
whether the engine lets a door's open animation be driven from outside
(small: a read of the activation and the animation in the decompile).

## 5. B as built (2026-11-11)

- **Which doors.** References whose base form is a door (type 0x18, xOBSE
  GameForms.h `kFormType_Door`) - load doors and doors within a cell alike.
  Their model's box is read in the model's own frame (the vertices, as
  opening by reaching reads a chest) once per door and node; a box counts
  as a leaf when its thinner horizontal side is at most 35 cm and half its
  width, and it is at least 1 m high (`vr::DoorThinAxis`). Trapdoors,
  ladders and hatches are not leaves: they keep A. *Not checked:* a load
  door whose model carries its frame or a piece of wall reads thicker than
  its leaf; the log line "Door: ... model read as a box" names every door's
  box and whether it counts.
- **The handle zone.** The leaf's whole width (and 5 cm past either edge),
  0.7 to 1.4 m above its foot (35 to 80 % of a leaf under 1.6 m), and within
  15 cm of either face. The hinge side is not known (no handle node, no hinge
  mark in the model), so the whole width counts.
- **Taken, opened.** An empty hand's grip has to close there (a grip carried
  in already closed takes nothing); a light tick in that hand. The hand moved
  `DoorGripMoveMetres` (12 cm, 5 to 40) along the leaf's normal opens it -
  either way, since which way a door swings is not in the engine's data
  (its open animation lives in its model). Opened through
  `game::ActivateByPlayer`, the call the A button ends in: a load door
  loads, a locked one gets the engine's lock handling, owners and crime as
  with A. Once per grip.
- **The controller, not the drawn hand.** The zone and the move are read
  from the controllers' own places, kept before the walls, the people and
  the doors stop the drawn hands (game/HandContactLogic.h): the first push
  run of 2026-11-11 had the hand rest on the leaf 10 cm in while the
  controller went 18 cm on, and a push could never open a door.
- **Never taken.** A hand holding something, a weapon readied (either
  hand, as opening by reaching), an item under the same hand (the hands'
  pick), a menu up, the setting off. Let go before the threshold, a menu
  coming up, the hand taking something, or the door gone: let go, shut.
- **Measured on doors (2026-11-11, Linux).** WarehouseFight's load door
  (REFR 00025ED8): its model 0.15 x 1.41 x 2.24 m, origin at its middle,
  thin along x. The hall's door: 1.46 x 0.09 x 2.08 m, origin at its hinge
  and foot. doorfulllower placed by PlaceAtMe: 1.48 x 0.16 x 2.08 m, origin
  at its hinge and middle - PlaceAtMe puts that origin at the player's feet,
  so the placed door stands half in the floor (the scenarios place the hand
  there). Every door's box is logged once ("Door: ... model read as a box").
- **Tests.** door_grip_test (every flow of the step, the zone, the leaf,
  the threshold); harness door-grip-load.txt (pulled: the player loads into
  the next cell), door-grip-push.txt, door-grip-still.txt (a grip without a
  move; the other hand with a sword readied). Holding a grabbed thing is
  covered by the unit flows only.
