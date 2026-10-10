# The player's body in VR (notes; nothing built beyond [Body] Visible)

## What exists (read in the repo, 2026-10-09)

- `game/PlayerBody` shows the third-person skeleton in first person,
  Enhanced Camera's way (docs/vr-modding/ecosystem-and-prior-art.md,
  "Enhanced Camera 1.4b"): the root moved each frame so `Bip01 Head` stands
  under the headset, the head and both clavicles collapsed for the draw.
- `[Body] Visible` is 0 by default: the two headset runs of 2026-09-07 had
  the view lurch with every look while it was on, cause not found
  (OBVR.ini, `[Body]`).
- The body's arms are its animation's, not the player's: with the arms
  shown (`HideArms=0`) they do not follow the controllers.

## Open (the tester, 2026-10-09, for later): IK for the player's body

The tester: "dann notiere IK für den player body." Inverse kinematics so
the body follows the tracked player: the arms reaching to the controllers
(shoulder, elbow and wrist solved from the hand's pose), the torso turned
and bent towards the headset, and the legs under it - the picture VRIK
gives Skyrim VR. Nothing researched for it:

- Enhanced Camera has "no inverse kinematics anywhere in it" (the
  ecosystem doc); the engine's own head-tracking IK (`0x00603AAA` per its
  source) turns the head to a target and is no arm solver.
- It needs the body shown first, so the lurch above comes before it.
- Unknowns: where a bone write after the animation reaches the draw (the
  body's own placement runs from the scene render's first callback for
  that reason, game/PlayerBody.h), whether a two-bone solve per arm is
  enough against Oblivion's skeleton (`Bip01 L/R UpperArm`, `Forearm`,
  `Hand`, and the twist bones), and how the first-person hands and the
  body's hands are made one.
