# Shaking a walker off in VR

Nadi, 2026-10-28: "abschütteln in vr, ähnlich wie saints and sinners". Nadi, 2026-10-30: "go for vr
shake, mit ui einblendung dass der user controller shaken muss wie in saints and sinners".
Oblivion Walkers' side is its docs/walkers-spec.md 4.19 ("VR shake", "One holder roots").

## The split

- **Walkers decides.** It decides who holds which arm and how many strokes free that arm (6 by
  default, more in a crowd). It also decides when a holder bites and when the player dies.
- **OBVR is the sensor and the body** (`vr/WalkersShake.h`, `game/WalkersLink.{h,cpp}`, the
  block in `camera/CameraHook.cpp` after the hand update; `[Hands] WalkersShake=1`).

## The messages (xOBSE messaging)

| Message | Way | Words |
| --- | --- | --- |
| `'WGVR'` 0x52564757 | OBVR → Walkers. Sent at PostPostLoad, and again at the first world frames while it is not taken | 1 = VR. Walkers then says no line |
| `'WGRB'` 0x42524757 | Walkers → OBVR, at each grab and let-go | the count, then each holder's reference and arm (1 left, 2 right) |
| `'WGBT'` 0x54424757 | Walkers → OBVR, at each bite | the arm, which bite, of how many, 1 for the last |
| `'WGSK'` 0x4B534757 | OBVR → Walkers, at each stroke of a held hand | the arm |

OBVR hears Walkers through the listener the skill exceptions put on every plugin at PostLoad
(`game/SkillCredit.cpp`).

## What OBVR does while an arm is held

- **The call.** "Shake it off!" is shown 12 cm above that arm's controller, turned to the eyes. It
  is a small plate in the holster sign's look, 16 cm wide. Its second line counts the bites:
  "Bitten 1 of 3".
- **The jolts.** A jolt in that hand at the grab (0.15 s), a tick at every stroke, and a strong jolt
  at a bite (0.3 s).
- **The bite's flash.** A red edge, up in 0.08 s and gone in 0.8 s, stronger at the last bite
  (`BiteFlashAlpha`).
- **The stroke.** A turn of the hand's way after `ShakeStrokeMetres` (0.12) of travel, with the
  fastest frame at least `ShakeStrokeSpeed` (0.8 m/s), counted once the hand has come back 4 cm.
  Any way counts.
- **The stick.** It does not walk while any arm is held (2026-10-30, at the headset: "griff kommt
  zwar aber ich kann einfach weg"). Turning stays.
- **The arm and its controller.** The arm is the body's, so its controller is the physical one on
  that side. In left-handed mode the swapped roles call that controller the other role
  (`ShakePromptHand`).

## Tested

- **Unit:** `walkers_shake_test` covers the strokes, jitter, slow moves, a lost controller, the
  messages' parsing and refusals, the flash, the call's text and the stick lock.
- **Harness:** `walkers-shake.txt` with `[Debug] TestWalkersHold=2`, PASS 2026-10-30. See
  docs/hand-script-harness.md.
- **End to end with a walker:** `walkers-shake-e2e.txt`, PASS 2026-10-30 12:16
  (artifacts/hand-script/walkers-shake-e2e/20261030-121639), after Walkers' 5e56eb0 moved its
  OBVR listener to PostLoad (before, xOBSE refused a sender not yet loaded: "OBVR's messages not
  listened to"). Walkers' INI had `[Grab] TestGrabSeconds=15`, `ChancePercent=100` for the run,
  removed after. Seen: 'WGVR' taken, Walkers "OBVR says the player plays in VR", the hold of the
  right arm as 'WGRB', the call shown there, the stick locked, bites 1 to 3 as 'WGBT', six strokes
  sent and counted by Walkers "1 of 6" to "shaken off", the let-go as 'WGRB' with none.
- **Headset:** open (how the call looks, the jolts, the thresholds).

## Not built

- **The free hand's push on a holder** (`'WGFR'`): left out as recommended. Walkers still frees by
  blows and the Block key.
- **The held hand pulled toward the holder:** a feel only, not needed for the count.
