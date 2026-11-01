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

## Progress, 2026-11-01 (phases 1 and 2 carried out)

**Done, each a commit with its harness scenarios rerun headless-free on
Windows (-Direct) and 219 unit tests green:**

| Step | Commit | Moved to | Harness |
|---|---|---|---|
| 1 HandStep / HandShared | dec8e35 | camera/hands/HandStep.h | (with 2.1) |
| 2.1 teleport | dec8e35 | hands/Teleport.cpp | teleport, teleport-noise, teleport-fatigue PASS |
| 2.2 quick menu, spell ring | c0771ac | hands/Rings.cpp | quick-menu-stick, quick-menu-assign, spell-ring, spell-ring-index PASS |
| 2.3 yield, middle finger | f35264c | hands/Gestures.cpp | yield: all but the sword phase, which fails identically before the move |
| 2.4 arm swing, analog walk, sprint | 7f06096 | hands/Locomotion.cpp | analog-walk, arm-swing-run, sprint PASS |
| 2.5 holsters (reach, sign, fit) | 5bce91f | hands/Holster.cpp | holster-places, -shield-torch, -fit, -left-handed, fist PASS |
| 2.6 hand-back | ca562df | hands/Handover.cpp | handover-off, handover-torch PASS |
| 2.7 held object, stow, eat | 738b722 | hands/Held.cpp | eat-at-mouth (apple) through the moved step; potion, stow-drawn-hand, throw-hit fail identically before the move |
| 2.8 release (weapon and torch) | 2aea27c | hands/WeaponRelease.cpp | weapon-drop, weapon-release-holster, torch-drop-lit, handover-torch, half-sword x2 PASS |
| 2.8b reaching (pick, grab reach, take, inventory) | c5295cb | hands/Grab.cpp | holster-pull, weapon-grab, pick-hold, inventory-grab-hold PASS; torch-grab flaky by the drop's landing, before and after alike |
| 2.9 torches | (in 2.8 and 2.8b) | the torch's take, drop and fire moved with the release and the reaching | as 2.8, 2.8b |
| 2.10 two hands, half-swording | 8270e0c | hands/TwoHands.cpp | half-sword-right-hand, -hilt-hard, two-hand-half-sword(-off), two-hand-grip PASS |
| 2.11 bow by hand | 04ac63f, 8270e0c | hands/Archery.cpp | bow-by-hand PASS |
| 2.12 kick, left B's menu hold | 519a038 | hands/Kick.cpp | kick-hit, kick-damage PASS; kick 3 foot marks of 4, the same before the move |
| 2.13 the cuts | 0f45090 | hands/Dismember.cpp | dismember-corpse, -killing-blow, -hand, -blade-corpse PASS |
| 2.14 Walkers' shake | 434e48c | hands/WalkersShake.cpp | walkers-shake-e2e, walkers-shake PASS |
| 2.15 world push, fists, hand bodies | f8de4c3 | hands/WorldPush.cpp | body-soft-push, fist, weapon-drop-sweep PASS |
| 2.16 slap tap, reach ring | 911c2e8 | hands/SlapTap.cpp, hands/ReachRing.cpp, camera/SettledRef.h | reach-name PASS; offhand-punch stale since 2026-10-12 (the shove's wording, not moved) |

**Measured after it:**

| | Before | After |
|---|---|---|
| CameraHook.cpp lines | 18,613 | 14,509 |
| UpdateHandMode lines | 4,887 | 2,703 |
| CameraHook.cpp file globals | 370 | 278 |
| camera/hands/*.cpp | - | 4,136 lines in 12 files |

**Where it deviates from the plan above, and why:**
- Phase 1 collects only what is constant for the frame (dt, the menu, the
  mode, the backend, the hand script's mark). The plan's "the 13 reads of
  the player's action become one" was wrong: several of those reads follow
  a step that equips, draws or toggles the view in the same frame
  (UpdateHandMode at the yield's drive, the bow's loose, the ready click)
  and must see the change.
- The stow moved with the held object (2.7), not with the holsters (2.5):
  it is woven into the held object's step.
- The torches (2.9) were no block of their own: their take, drop and fire
  sat inside the reaching and the release and moved with them.
- The two-hander's functions keep their own function statics (2.10):
  five of them share names (`s_lines`), they already belong to one function
  each, and there is one instance of the state.
- Shared helpers with state of their own (ForgetTaken, StepThrowHits,
  HeldItemPoseNow, the torch shape log, MenuPulseOn, LaserAnglesFor) are
  handed in as function pointers rather than moved: moving them would take
  their state's other users with them.
- **The plan's promise that UpdateHandMode is "under 300 lines after 2.11"
  is not met.** The plan counted only the eleven named features. Still in
  UpdateHandMode (2,703 lines): the frame's filling and the hand mode's call
  (~600), the menus' laser, cursor and wheel (~400), Walkers' shake (112),
  the kick (167), the cuts (240), the slap tap (75), the reach marker (79),
  the world push (199), the early weapon and torch frame setup (78 + 61),
  and the remaining logs.

**Next, to reach the orchestrator (phase 2b, same shape, same net):**

| Step | Block | Cost | Why |
|---|---|---|---|
| 2.12-2.16 | done 2026-11-01 (the table above) | - | - |
| 2.17 | the menus' laser, cursor, wheel and drags | medium | the most log lines; native-menu-*.txt, menu-*.txt |
| 2.18 | the frame's filling (HandModeFrame) into a builder | medium | the reads the steps depend on; every scenario is its net |

Phase 3 as planned after that.

## Phase 0, done after phases 1 and 2 (2026-11-01)

The ratchet is `tests/function_sizes_test` (tests/FunctionSizes.h, the
counting; tests/FunctionSizesTest.cpp, the fixtures and the repository
check) against `tests/function-sizes.txt`: a function over 100 lines must
be listed with the length it may have, a file's mutable `g_` globals may
not exceed the count listed, and a row for a function that is gone fails
too. Smaller passes and is reported as "could tighten"; the table is
rewritten with `function_sizes_test --print > tests/function-sizes.txt`.

It deviates from the plan in one point: C++ in the unit tests, not a
Python script behind CTest. Python is not installed on the Windows
machine (the Store stub only), and a ratchet that runs on one platform is
no ratchet; the C++ test runs with the other 219 tests on both.

What it measures today (the plan's table above counted differently -
functions whose signature spans lines were missed there, and `const`
globals were counted): 200 rows; the functions over 300 lines are
UpdateHandMode 2,703, Config ReadRuntimeValues 955, MaybeSubmitOverlays
930, HandMode::Update 719, hands/Grab StepGrab 712, hands/WeaponRelease
StepRelease 625, OnFrameEnd 592, StepWeaponContact 546,
BeforeFirstScenePass 520, NativeMenuPrototype Tick 478, StepHandContacts
461, EyeMirror::Create 373; CameraHook.cpp holds 426 mutable file globals.
The plan's second item, the twelve-scenario baseline sweep under
artifacts/refactor-baseline/, was not made: phases 1 and 2 ran each step's
own scenarios before and after instead (the table above).
