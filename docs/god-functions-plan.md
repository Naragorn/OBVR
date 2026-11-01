# The God functions, and the plan to take them apart

Written 2026-11-01 from a measurement of the tree at 633569a (Nadi: "do a
code review and create a plan to eliminate the God class functions").
Everything in "Measured" was counted with a brace-depth script over the
source; the numbers are approximate to a few per cent (comment lines and
string literals were stripped first). Nothing here is built.

## Measured

There is no God *class*. OBVR's hooks are free functions in anonymous
namespaces; the God is one translation unit and its biggest function.

| Unit | Lines | Functions | > 100 lines | > 300 lines | File globals `g_*` | Static locals `s_*` | `OBVR_LOG` |
|---|---|---|---|---|---|---|---|
| camera/CameraHook.cpp | 18,613 | 113 | 16 | 6 | 370 | 242 | 532 |
| render/InterfaceRenderHook.cpp | 3,878 | 77 | 5 | 0 | - | - | - |
| core/Config.cpp | 1,580 | 19 | 2 | 1 | - | - | - |
| vr/HandMode.cpp | 1,338 | 8 | 2 | 1 | - | - | - |
| game/BodyIk.cpp | 1,711 | 39 | 3 | 0 | - | - | - |
| ui/SettingsList.cpp | 2,569 | 16 | 0 | 0 | - | - | - |

The functions over 300 lines:

| Function | Lines | Static locals | What it does |
|---|---|---|---|
| CameraHook `UpdateHandMode` | 4,887 | 126 | every Full VR feature's per-frame glue, in order |
| CameraHook `MaybeSubmitOverlays` | 930 | 62 | every overlay's capture, placement and submit |
| Config `ReadRuntimeValues` | 955 | - | 587 `Read*` calls, one per INI key |
| CameraHook `OnFrameEnd` | 592 | 12 | the frame's delivery decision and submit |
| CameraHook `StepWeaponContact` | 546 | 14 | the blade against walls, bodies, blades |
| CameraHook `BeforeFirstScenePass` | 515 | 12 | the body, the arms, the held things before the render |
| HandMode `HandMode::Update` | 719 | - | the controller frame: buttons, lasers, menus |
| CameraHook `StepHandContacts` | 461 | - | the free hand and the shield against the world |

`UpdateHandMode` in detail: nesting seven tabs deep, 196 log calls, about
25 feature blocks told apart only by comment banners (the combat ring, the
trigger as finger or button, the ready-weapon click, the hand script, the
spell ring's pages, the cinema screen, the grip trace, Walkers' grab, the
hand-over, arm-swing running, menu drags, taking by hand, the item reach,
the grapple, the held object's place, the held body's physics, the weapon's
lag, the left fist, the laser on a flat frame, ...). It reads the player's
action 13 times, the crosshair target 8 times and the weapon state 6 times
in one frame instead of once.

CameraHook.cpp has 204 includes and exports 6 functions through its header.
405 of the 802 commits since 2026-09-01 touched it (50 %). No test compiles
it: the 207 unit tests cover the pure headers it calls.

## What is already right, and what the plan builds on

- **The decisions are out.** The FrameLogic pattern - pure functions over
  plain values in a header, a unit test per header - exists in 75 headers
  (FrameLogic.h alone declares 59). `UpdateHandMode` mostly *calls* them
  (`StepWeaponPull`, `StepGrabPhysics`, `StepAnalogWalk`, `UpdateTeleport`,
  `UpdateYield`, ...). What remains in the God function is glue and state.
- **The harness is a golden test.** About 380 scenarios in
  tools/hand-scripts, each asserting log lines; every feature block in
  `UpdateHandMode` has one or more. A refactoring that keeps the log lines
  keeps the scenario green - that is the safety net, not new unit tests.
- **SettingsList is data-driven** (a table of rows, no function over 24
  lines) and BodyIk, MeleeHits, OpenVRBackend are ordinary sizes. They are
  not the problem and stay as they are.

## The problem, named

Not the length. The **state**: about 600 mutable slots (370 file globals
plus 242 function statics) in one anonymous namespace, read and written
across feature blocks with no declared owner. A static local initialised on
first call ties behaviour to call order; a global named `g_handBackTaking`
is read by the hand-over, the stow and the pull. That is why the functions
cannot be cut by hand today: a block moved out takes an unknown set of
globals with it.

## The plan: a strangler, one feature at a time, no rewrite

Every step is one commit, its harness scenarios rerun headless, the log
lines unchanged. Nothing is renamed in the log; nothing changes for a
player.

### Phase 0 - the ratchet (cheap, half a day)

1. `tools/function-sizes.py` (the brace-depth count above, made a tool) and
   a CTest entry that **fails when any function grows past its recorded
   length** or CameraHook.cpp gains a file global: a table
   `tests/function-sizes.txt` of the current numbers, the test allowing
   smaller, refusing larger. Python runs in the Linux CTest only; on
   Windows the test is skipped as the other `.py` tests are.
2. The two harness sweeps that will be the before/after for every later
   step: the twelve scenarios that cover `UpdateHandMode`'s blocks most
   directly (`teleport`, `quick-menu`, `holster`, `fist`, `grapple`,
   `handover-off`, `analog-walk`, `arm-swing-run`, `yield`, `take-only-by-
   hand`, `two-hand-grip`, `walkers-shake-e2e`) run once on the baseline,
   their summaries kept under artifacts/refactor-baseline/.

Cost: cheap - the counting script exists in this review, the scenarios
exist, nothing in src/ changes. Risk: none.

### Phase 1 - `FrameContext`: read once, pass down (small, one to two days)

The frame's reads at the top of `UpdateHandMode` - dt, menuIsUp, the top
menu, the player's action, weapon state, third person, fatigue, health,
both hands' poses and buttons, the crosshair target, the hand script's
state - into one `struct HandFrame` (vr/HandFrame.h, plain values), filled
by one function, passed by const reference to every block. The 13 reads of
the player's action become one.

Cost: small - mechanical, each block's local reads replaced by a field;
the harness shows any block that read a value *after* changing it (the
only behaviour that can move). Gain: every later extraction has its input
in one place, and a frame reads the engine once.

### Phase 2 - one feature out at a time (medium, two to four weeks of steps)

For each feature block of `UpdateHandMode`, in this order (the blocks whose
decisions are already in a pure header and that have their own scenario
first, the entangled ones last):

| Step | Block | Its pure header (exists) | Its scenario(s) | Globals it takes |
|---|---|---|---|---|
| 2.1 | teleport | vr/Teleport.h | teleport, teleport-noise, teleport-fatigue | g_telepo* (4) |
| 2.2 | quick menu and spell ring | vr/QuickMenu.h, vr/SpellRing.h | quick-menu, spell-ring-* | g_quick* (6), g_spell* (15) |
| 2.3 | yield, insult | vr/Yield.h | yield-* | few |
| 2.4 | arm swing, analog walk, sprint | vr/ArmSwing.h, vr/AnalogWalkLogic.h | arm-swing-run, analog-walk | few |
| 2.5 | holsters and stow | vr/Holster.h, vr/Stow.h, vr/StowPlace.h | holster*, stow* | g_holste* (13), g_stow* (5) |
| 2.6 | the hand-over and hand-back | game/HandoverLogic.h | handover-off, handover-* | g_handBack* |
| 2.7 | the held object and its physics | vr/GripHold.h, game/RagdollHold.h | grab-*, held-* | g_grappl* (14) |
| 2.8 | the weapon: pull, take, drop, lag | game/WeaponRelease.h, vr/WeaponWeight.h | holster-pull, weapon-drop, take-* | g_weapon* (16), g_blade* (16) |
| 2.9 | torches | game/TorchFire.h, game/TorchBlowLogic.h | torch-* | g_torch* (13) |
| 2.10 | the two-hander and half-sword | vr/TwoHandLogic.h, vr/HalfSwordLogic.h | two-hand-*, half-sword-* | g_two* (10) |
| 2.11 | aiming (bow, spell, third person) | vr/SpellAim.h, vr/Archery.h | bow-*, cast-*, third-person-* | g_aim* (22), g_bow*, g_cast* |

The shape of each step, always the same:

- a file `src/camera/hands/<Feature>.cpp` with a `struct <Feature>State`
  that **owns** the block's `g_*` and `s_*` variables as members, and one
  function `Step<Feature>(const HandFrame&, <Feature>State&, <Outputs>&)`;
- the block's body moved there verbatim, its globals renamed to members,
  its statics to members with the same initial value (a static initialised
  on first call becomes a member plus a `bool started` - say so in the
  commit when it happens, that is the one place behaviour can move);
- `UpdateHandMode` keeps one call and the state instance;
- what the block hands to *other* blocks (the `g_handBackTaking` kind)
  becomes a field of a small `HandFrameOutputs` struct returned up, not a
  global the next block reads - the cross-block couplings become visible
  in the signature, which is the point;
- the feature's scenarios rerun; the ratchet table updated downward.

Cost per step: small to medium - 100 to 600 lines moved, half a day to two
days each, most of it reading which globals the block touches (a grep per
`g_` name, then the compiler says the rest). Risk per step: a global read
by two blocks that the move turns into two copies - the compiler catches
the unmoved reader (it no longer compiles), the harness catches a changed
order. The entangled ones (2.7, 2.8, 2.11) are where the real couplings
show; expect those to produce `HandFrameOutputs` fields rather than clean
cuts, and to be the steps that find a latent bug.

After 2.11 `UpdateHandMode` is an orchestrator: fill the frame, call
eleven steps in order, pass outputs along - under 300 lines.

### Phase 3 - the other five over 300 lines (medium, one to two weeks)

- `MaybeSubmitOverlays` (930, 62 statics): the overlays are already
  classes in render/ (CrosshairLayer, HudLayer, HandHudLayer, LaserLayer,
  TeleportArcLayer, VignetteLayer, ReachMarker). The function decides per
  layer whether to capture, where to hang it and whether to submit; that
  decision per layer moves into the layer (`Layer::StepFrame(const
  OverlayFrame&)`), the function becomes a loop over the layers. Scenarios:
  hud-*, crosshair-*, laser-*, teleport. Small to medium.
- `OnFrameEnd` (592) and `BeforeFirstScenePass` (515): the delivery
  decision is already in FrameLogic.h; what is left is submit plumbing and
  the body's step order. Cut by the same `State` + `Step` shape, two or
  three pieces each. Small.
- `StepWeaponContact` (546) and `StepHandContacts` (461): one decision per
  contact kind (wall, body, blade, shield) - each into game/*ContactLogic.h
  where BladeContactLogic.h already is, the function a dispatcher. Medium:
  these carry physics timing (hit-stop, parry), the scenarios blade-*,
  mace-*, shield-* are the net.
- `HandMode::Update` (719): already a class; split by its three phases
  (buttons, laser and pointer, menus) into three private methods first,
  then the laser into `vr/HandLaser.h` (pure, exists partly as
  LaserGeometry.h). Small.
- Config `ReadRuntimeValues` (955): **table-driven from the table that
  exists** - ui/SettingsList.cpp already lists every key with its section,
  type and member; a reader over that table replaces 587 hand-written
  calls, and a key present in the INI but not in the table becomes a unit
  test instead of a review. Medium (the table lacks the keys the settings
  menu does not show; those get added, which is the audit the review
  wanted anyway). Low risk: config_test and the INI/code diff tool of the
  review cover it.

### Phase 4 - the ratchet tightened (cheap, continuous)

Thresholds in `tests/function-sizes.txt` lowered as steps land; the final
rule proposed: no function over 300 lines, no new file global in
CameraHook.cpp, CameraHook.cpp under 8,000 lines by the end of Phase 2 and
under 4,000 after Phase 3.

## What not to do, and why

- **No rewrite, no big-bang split into "modules".** 50 % of all commits
  touch CameraHook.cpp; a branch that holds it open for a week cannot be
  merged against that rate, and the other sessions work in it daily.
- **No `Globals.h`.** Moving the 370 globals into a shared header makes the
  coupling compile faster to see and no easier to cut; ownership is the
  fix, not visibility.
- **No class hierarchy, no feature interface with virtual `Step`.** The
  steps run in a fixed order with typed outputs; a loop over an interface
  would hide the order that the harness depends on.
- **No renaming of log lines** during the moves. The scenarios are the
  regression test; a line changed is a scenario to re-verify by hand.

## Costs at a glance

| Phase | Rating | Why |
|---|---|---|
| 0 ratchet + baseline | cheap | script and scenarios exist; no src change |
| 1 HandFrame | small | mechanical; the harness sees any read-after-write moved |
| 2 eleven feature steps | medium overall, small each | 100-600 lines a step, the pure headers and scenarios exist; the entangled three carry the risk |
| 3 the other five | medium | overlay layers exist as classes; contacts carry physics timing; Config's table exists in SettingsList |
| 4 tighten | cheap | a number in a table |

Recommendation: Phase 0 and 1 now, before beta 8's headset round (they
change no behaviour and make every later diff smaller); Phase 2 as single
steps between feature work, never two at once; Phase 3's Config step
early (it is the cheapest gain), the contacts last.
