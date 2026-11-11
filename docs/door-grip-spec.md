# Doors by grip: open a door with the hand instead of A (proposal 2026-11-11)

Status: **proposed, nothing built.** Brainstorm of 2026-11-11; the tester
asked for it to go into a spec.

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
