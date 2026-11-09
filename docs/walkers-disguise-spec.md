# Oblivion Walkers' disguise in VR

Walkers 4.20.1 (Walkers 475c3f3, its docs/walkers-spec.md 4.20.1): smearing a
fallen walker's blood on oneself makes the walkers take the player for one of
theirs for a while (Walkers `[Disguise] Seconds`, 180 by default). Flat, the
player activates the body while sneaking and confirms a dialog. In VR (Nadi,
2026-11-09: "go Tarnung komplett, aber mit Vignette") OBVR gives it the hands
and the look; Walkers keeps every rule.

## The messages

| Message | Way | Words | Meaning |
|---|---|---|---|
| `'WGBL'` 0x4C424757 | OBVR -> Walkers | 1: the body's form id | the player smeared this dead body's blood over the chest; Walkers starts the disguise without its dialog, or says in its log why not |
| `'WGDS'` 0x53444757 | Walkers -> OBVR | 1: 1 on, 0 over | the disguise's state, for the look |

OBVR sends only dead bodies; whether one counts (a walker's, the player
sneaking) is Walkers' call.

## The gesture (vr/WalkersDisguise.h, StepDisguiseGesture)

1. One hand inside a dead body: within `[Hands] DisguiseBodyReachUnits` (18)
   of its trunk - Spine2 to the head and as far again below Spine2, where a
   human's pelvis is (derived from the bones' proportions, not measured on
   every race) - read at 10 Hz from the loaded dead actors near the hands
   (game/CorpseTouch.h over game/Dismember.h CollectNecks).
2. Within 4 s of that hand leaving the body, the same hand at the chest - the
   stow spot (vr/Stow.h InStowZone, at the drawn hand).
3. 'WGBL' with the body, a short pulse at both hands, 3 s before the next.

The other hand at the chest, a hand lost by tracking, a menu, leaving the
world or the setting off forget the started gesture.

## The look (StepDisguiseLook, camera/hands/WalkersDisguise.h)

While 'WGDS' is 1, eased in over 0.8 s and out over 1.5 s (smoothstep):

- a black-red vignette: the snap turn's ring (render/VignetteLayer.h, white
  and tinted by the overlay's colour, dark red 0.35/0/0.02), clear 22 degrees
  round the centre, at `[Hands] DisguiseVignette` (0.9);
- the view darker: a second, black ring clear only 1 degree round the centre,
  at `[Hands] DisguiseDarken` (0.3). It ramps over the first 21 degrees, so
  the very centre is a little less dark than the rest (derived from the
  ring's ramp, not seen in a headset);
- blood on the hands (`[Hands] DisguiseBloodyHands`): the player's blood
  decal (game/Blood.h) laid at each hand from above on the third-person body
  and on the first-person arms (game/HandBlood.h, game/CutMarks.h
  LayCutMarkOn), again every 8 s since the engine fades its decals after
  fDecalLifetime (10 s here). After the disguise the last stains fade with
  the engine's own lifetime.

Both rings are SteamVR overlays, so they are the same in both eyes in single
and dual pass, and are not in the game's pictures (the harness sees only
their log lines).

## Settings

`[Hands] WalkersDisguise` (1), `DisguiseBloodyHands` (1), `DisguiseVignette`
(0.9), `DisguiseDarken` (0.3), `DisguiseBodyReachUnits` (18). No row in the
settings menu yet.

## Tests

- walkers_disguise_test: 'WGDS' parsed, a hand in a trunk, every flow of the
  gesture (the order, the window, the same hand, the cooldown, a hand lost,
  not allowed, switched off), the look's ease in and out.
- Harness walkers-disguise (tools/walkers-disguise-run.sh): a killed walker,
  the player sneaking, the right hand into the body and to the chest.

## Open

- How it looks in the headset: the ring's red, the darkening's strength, the
  blood on the hands (whether the decals hold on the hands in first person).
- The trunk's reach is a guess (18 units round the spine); a body lying
  twisted may need more.
