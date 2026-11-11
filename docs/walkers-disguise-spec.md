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
  decal (game/Blood.h) laid on the first-person arms' hand shapes - the hands
  seen in Full VR - at each hand's middle (between the hand bone and the
  middle finger's), from seven sides (vr::HandStainDirection: the eye's,
  then both ways along each of the hand bone's axes), one side a frame for
  both hands (the engine lays at most iMaxDecalsPerFrame), the round again
  every 8 s since the engine fades its decals after fDecalLifetime (10 s
  here). The decal's size is the engine's own (15 units), not asked. After
  the disguise the last stains fade with the engine's lifetime.

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

## Runs (Linux, headless, OBVR.dll 0d413a80)

- walkers-disguise 20261109-153807 PASS: 'WGBL' FF000F89 taken, Walkers
  "on (OBVR's gesture) - 40 real seconds", 'WGDS' 1 back, the look's line;
  the blood laid every 8 s; after 40 s 'WGDS' 0 and the look's "over".
  (20261109-153555 had all of it but the scenario ended before the "over".)
- The blood, first build (one stain from above at the hand bone): one decal
  gained on "Hand:HGEC" at each hand, none on the third-person body (the
  sword and scabbard were the shapes there) - and none seen in the picture.
- The blood from seven sides at the hand's middle (Nadi: "go a"),
  walkers-disguise-hands 20261109-163156 PASS (the disguise started by
  Walkers' Test=1 dialog): 14 decals gained a round, on "Hand:HGEC"; both
  hands red, backs and palms, in OBVR-Eye-Left-hands_near.png and
  -hands_again_turned.png. So the first build's stain had missed the hand
  (laid at the wrist, from above), not the drawing.
- The gesture run is not steady: the killed walker falls 90 to 120 units
  ahead, and a script's fixed hand reaches it or not (PASS 153807, then five
  misses at 16:15-16:29; a moveto does not carry a dead body).
- Regressions: eat-at-mouth 20261109-154017, fitted-capsules 20261109-154157 PASS.

## A living one is no corpse (2026-11-11)

Nadi's Windows log of 2026-11-11: 'WGBL' went out for 00023502, a living NPC
knocked down in a fist fight. The roster's dead flag came from the engine's
IsDead (0x005E33B0) with the argument 0, which reads the life state at
Actor +0xB0 and answers yes for 1, 2 and 6 (with the argument 1: 1 and 2;
read 2026-11-11). Measured (life-states.txt, "HandScript: life states" at
each mark): an essential "killed" lies at 6 and stands up again after some 8
s; one knocked down by the engine's push stays at 0 (knocked state 1); a
dying one is 1 for about two seconds, then 2; a corpse unloaded and loaded
again is 2. So a dead body is the life state 1 or 2 (game/ActorRosterLogic.h
`LifeStateIsDeadBody`, the roster's `IsDead`): this decides for the
disguise's corpse touch and for every reader of the roster - the cuts, the
neck's bleeding, a blade's hold. The corpse touch logs a hand in a living
one ("who lives (life state N) - no corpse"). Tests: actor_roster_test
TestDeadBody. Harness: walkers-disguise-living.txt PASS (an essential knocked
out, the hand in it: no 'WGBL'); dismember-limbs, -killing-blow, -corpse,
-blade-corpse, -save-load, -essential and blade-sticks PASS with it.

## Open

- How it looks in the headset: the ring's red, the darkening's strength; the
  hands come out nearly all red in the pictures - if that is too much, fewer
  sides (the eye's alone, or the eye's and the back) would leave spots.
- The trunk's reach is a guess (18 units round the spine); a body lying
  twisted may need more.
