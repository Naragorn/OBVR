# Immersive scroll menu: technical design and implementation status

Status: design, controller action plumbing and pure logic in progress; **not an integrated feature**. Neither
settings nor a live menu presentation are enabled by this work yet. The user has
resolved the touch interaction decision below. Integration first requires review
of existing Full VR work found on `dlss5`.

## Observed integration points

- `src/camera/CameraHook.cpp`, `UpdateHandMode` (line 349 at investigation): reads
  head and both controller poses, builds `HandModeFrame`, applies control results,
  dispatches motion strikes, mouse movement and wheel events, and places the HUD.
- `src/vr/HandMode.cpp`, `Update` and `PointAtMenu`: pure per-frame hand decisions,
  wrist or room menu target, pointer-hand selection, ray and controller-tip input.
- `src/vr/HandInput.h`: `MenuQuad`, `LaserOnQuad`, `PokeOnQuad`, `StepPoke`, button
  edges, control plans and repeat timers. No finger skeleton is exposed here.
- `src/game/HandControls.cpp`, `ApplyHandControls`: OS mouse/key edges, not an
  engine menu API. `MoveMouseBy` and `ScrollMouseWheel` are the existing input
  bridge. The bridge currently couples `menuClick` to the configured attack key;
  scroll input must instead address the UI's left mouse button independently of
  gameplay bindings. Cursor motion must precede selection.
- `src/game/MenuType.h`: `ActiveMenuId` is the menu under the cursor, and may be
  zero during keyboard navigation. It cannot acknowledge menu ownership by itself.
  `IsMenuMode` in `src/game/MenuMode.cpp` only reports a boolean.
- `src/render/HudLayer.h/.cpp`: captures the original interface into a texture;
  `ShownPixels` describes its layout rectangle. `Submit` supports wrist, room,
  and head placement. A transient absolute surface placement and texture cropping
  are needed for the scroll; the existing interface capture should be retained.
- `src/vr/OpenVRBackend.cpp`, `ReadHand` (line 633): legacy controller state and
  pose, with validity/connection checks, trigger, thumb axes, and a grip axis.
  `src/vr/OpenVRTypes.h` has different Index aliases for A and grip. Blindly
  calling the existing Index-oriented `GripDown` is not sufficient proof of Touch
  support. Runtime profile/binding verification is required before activation.
- `src/ui/SettingsList.cpp`, `src/core/Config.cpp`: existing setting registry and
  INI load path. Native settings already have uncommitted changes; preserve them.
- `tools/menu-world-run.ps1`: existing keyboard-driven menu harness, which refuses
  to take over an already running game. It does not exercise physical controllers.

Relevant history inspected: `4980116` introduced wrist poke and side selection;
`5de3e1a` expanded controller menus; `fdd6e75` introduced either-hand flat-picture
laser input; `7138848` added the gamepad control path. These are reusable paths,
not evidence that the new gesture works. Current working-tree changes predate
this implementation and must not be reverted.

## Accepted interaction: release one hand after full extension

For a scroll whose edges continuously follow both controller origins, the tip of
an edge-holding controller can only reach a small neighbourhood of that edge.
Moving the controller toward the centre moves the edge too and reduces opening.
The existing controller-tip offset is 0.08 m (`HandSettings::pokeTipForward`),
not independent finger tracking. This does not give access to an entire readable
menu between the two controllers. This is a geometry deduction, not a measured
headset result.

The user explicitly selected **full physical extension, then release one hand**:
"Erst nach vollständigem ausrollen des menüs und loslassen einer der hände kann
der user mit dem menü interagieren." This applies to both laser and touch. Full
extension alone must NOT enable interaction. After the deliberate release, the
remaining hand carries the rigid open object and the free hand interacts.
Re-grabbing the free end restores physical two-hand closing. Releasing both hands
cancels safely. Before release, and after re-grab, physical separation still
directly controls the opening; there is no automatic opening or near-open snap.

The release must be observed after reaching full extension, with the other hand
still holding. A hand already ungripped during opening cannot count as a new
release. One grip is enough to begin unrolling. Before handing control to the
free hand, the opposite grip must also be held so that a fresh release leaves a
supporting hand. Releasing on the same sample that first reaches full distance
does not establish the required ordering; first observe full extension, then the
release. The released hand becomes the interactor, naturally supporting either
hand without a fixed right-hand assumption.

Geometry is preserved in the support hand's local frame at release, so
the transfer does not jump or become headset-locked. Re-grab requires proximity
to the free end rather than snapping a distant hand onto it.

## Branch dependency discovered during design

Current local branches: `main` (88362ed), `dlss5` (ecaedb4), `water-fix-main`
(512ce16). `git rev-list --left-right --count main...water-fix-main` returned
15/0; `main...dlss5` returned 22/1. GitHub branch enumeration returned only `main`.
The sole dlss5-only commit modifies 134 files and mixes input/Full VR with rendering.

Unlike current main, dlss5 contains `assets/input/{actions,knuckles,oculus_touch}.json`,
`src/vr/ControllerActions.h`, `src/vr/HandSession.h`, `src/game/MenuInput.h`,
extended `NativeMenuPrototype` input dispatch and `src/test/VRTestRuntime.*`.
These are source findings, not newly verified runtime behavior. The native bridge
queues menu-identity-scoped input and uses xOBSE commands on its Tick path. Its
click path currently emits a tap, so true held dragging still needs review.

Port relevant dependencies while retaining main's water test runtime, newer
reticle fixes, native settings changes and unrelated uncommitted work. Do not
replace entire main files blindly with older dlss5 versions. The earlier
integration-point analysis above describes main before this port.

## Architecture and implementation plan

1. Add pure reusable surface/contact math and scroll gesture/manipulation logic,
   with boundary, invalid-data, refusal and recovery tests. No runtime calls.
2. Port and validate the relevant dlss5 dependencies, then implement the menu session/input
   bridge and renderer adapter. Verify native menu identity/stack and input
   consumption at the actual engine path before relying on them. An unconfirmed
   OS Tab event is a request, never proof that the desired menu opened.
3. Add settings through the current registry and native menu path: Immersive Scroll
   Menu = Off / Horizontal / Vertical (default Off); Scroll Menu Interaction =
   Laser Pointer / Touch Surface. Keep technical
   thresholds internal initially. Preserve the existing menu controls when idle.
4. Build the DLL and run pure/integration tests. Extend the existing in-game
   harness with recorded state, engine menu acknowledgements, cursor targets and
   input ownership. Then collect actual controller/headset evidence.

The logical components are:

- `MenuSurface`: a finite, orthonormal plane, physical dimensions and menu pixel
  dimensions; ray/contact-to-UV mapping independent of scroll decoration.
- `ScrollMenuLogic`: deliberate gesture, exact physical amount and deterministic
  manipulation state. Inputs are plain values in one tracking coordinate system.
- `ScrollPresentation` (planned): crop the captured UI rather than stretch it;
  parchment behind transparent pixels, rolled edges and thin edge strips, separate
  from input. Absolute tracking placement follows hands, never the headset.
- Laser/touch interactors: use the same surface object that rendering consumes.
  No duplicate coordinate transforms. Ray hit enables hover and trigger-held
  selection/drag; tip proximity enables hover and hysteretic contact/drag. Wheel
  events use the free/pointing hand's stick. The existing menu-side preference can
  select the opposite pointing hand; this is not a verified dominant-hand setting.
- Menu session/input bridge (planned): sole owner of opening/closing requests,
  engine acknowledgement, menu click/wheel/back, and release of injected inputs.

## Gesture and physical mapping

Proposed internal starting defaults (not headset-calibrated): 8 cm minimum safe
controller separation, 18 cm maximum ready separation, 60 cm full extension,
350 ms ready dwell, 35 degree axis alignment tolerance, controller forwards
within 25 degrees, hands in front of and below eye level. Grip must be released
during dwell, then newly held in the ready pose. Triggers/sticks/action buttons
must be neutral. This rejects a passing pose or a grip already held for combat.

Horizontal uses the initial head-relative horizontal axis to qualify the pose;
vertical uses tracking-space up, allowing either hand above the other. After
grab, the hands define the moving opening axis. Both styles share all mapping and
state logic. Distances and geometry are metres in the backend's tracking space.

`amount = clamp((distance - closedDistance) / (fullDistance - closedDistance), 0, 1)`

Full extension is tested against the actual distance, not a rounded or filtered
amount. There is **no 90% threshold and no epsilon that activates short of the
configured distance**. At 70% the amount remains 70%; reversing hands reverses it.
Filtering may smooth only the surface centre/orientation, not continue unrolling
after movement stops. The visible opening extent remains tied to current physical
separation. Invalid/non-finite poses fail closed.

The initial UI capture can be deferred until full extension: partial opening shows
parchment only. Native menu opening is requested only after the ordered one-hand
release, so its keyboard controls cannot act before the required gesture finishes.
Once captured, closing can crop that UI behind the rolls. Rendering
the native UI early would require blocking native keyboard/gamepad navigation too;
disabling only injected controller clicks is insufficient.

## State and ownership contract

Allowed manipulation transitions:

| From | Condition | To |
| --- | --- | --- |
| Inactive | eligible deliberate pose, neutral controls held through dwell | PoseDetected |
| PoseDetected | pose lost or foreign menu appears | Inactive |
| PoseDetected | fresh grip while pose remains valid | Grabbed |
| Grabbed | hands separate | Unrolling |
| Grabbed | grip released before unrolling | Closing |
| Unrolling / RollingUp | measured distance reaches full distance | Open |
| Open | fresh one-hand release after a prior full-extension sample, opposite grip still held | HeldOpen |
| HeldOpen | matching engine acknowledgement, usable capture and neutral interaction controls | Interacting |
| Open | measured amount decreases below 1 before release | RollingUp |
| HeldOpen / Interacting | free end re-grabbed near its physical location at full physical separation | Open |
| RollingUp | measured distance increases below full distance | Unrolling |
| Unrolling / RollingUp | hands close to closed distance | Closing |
| any owned state | cancel, tracking loss, disabled, world/focus lost | Closing |
| Closing | bridge released input and finished/hands off native menu | AwaitRelease |
| AwaitRelease | relevant buttons/sticks released | Inactive |

`PoseDetected` provides one subtle visual cue; no menu open and no gameplay
capture yet. `Grabbed` captures conflicting controller actions and clears pending
swings/grabs. No attack, block, movement, spell or settings chord may leak through
while the gesture owns those inputs. Native menu input must stay blocked below
full extension. The session bridge may take longer than the manipulation machine
to acknowledge requests; neither successful rendering nor a sent key is an ack.

`HeldOpen` explicitly represents the one-hand-held object while the engine catches
up. It prevents an asynchronous open request from being mistaken for a ready UI.
On re-grab, require both proximity to the rendered free end and actual full
separation; otherwise a grab near but inside the end would produce an immediate
jump from the held-open object to a partly closed distance. Re-grab disables
interaction immediately, before another pointer/touch command can be generated.
The supporting grip must remain valid and held; releasing it cancels rather than
silently changing support hands. Loss/inactivity of an input action is a failure,
not an intentional release edge.

On the first full extension, produce a one-shot confirmation cue. After the
subsequent one-hand release, request the native menu. Interaction requires
the confirmed menu, usable capture
and cursor, focus, and neutral-to-pressed input gating. Held triggers on entry do
not become a click. A missing ray/contact hit cannot click a stale cursor target.

Closing or tracking loss releases click/drag immediately, discards stale poses,
and closes only a menu positively owned by this session. B/Escape remains the
fallback. If ownership becomes ambiguous, leave the native menu accessible via
legacy controls rather than repeatedly toggling Tab. A bounded acknowledgement
timeout must relinquish ownership and log the failure. Controller action buttons
must be released before gameplay resumes, preventing an opening grip or click
from turning into an attack. Focus loss must not inject input into another app.

## Surface math and rendering

A surface stores centre, right/up unit vectors, width/height and pixel extent.
Its normal is right cross up. A forward ray intersects that plane, then projects
to `u = dot(point-centre,right)/width + .5`,
`v = .5 - dot(point-centre,up)/height`. Outside/back-facing/parallel/invalid rays
are rejected. Map UV to the actual `HudLayer::ShownPixels` domain; clamp to the
last valid pixel at an edge, not one pixel beyond it.

Touch uses the controller tip actually available, a front approach, 8 mm press,
18 mm release, 50 mm hover and a limited penetration band (proposed defaults).
Entry from behind must not press. Losing tracking, leaving the surface, losing
input ownership or rolling below full extension releases contact. The renderer
and interactor consume the same smoothed rigid pose; crop extent remains physical.

Horizontal cropping changes width, vertical cropping changes height while keeping
the unrolled UI pixel scale/aspect. `HudLayer` needs a transient override rather
than mutating normal HUD placement settings. Decorations are separate surfaces;
failed capture/presentation disables interaction and restores fallback input.
Overlay-based faceted rolls are feasible with existing absolute transforms and
raw-texture overlays, but visual quality and cost are unverified until measured.

## Evidence and validation boundaries

No working feature or in-game result is claimed. At investigation, `vrserver` and
`vrcompositor` processes were present, but Oblivion was not running. Process
presence does not verify connected/tracked controllers or headset usability.

The user subsequently confirmed **Valve Index controllers**, availability for a
physical trial, and a requirement to record that trial for automated replay and
harness iteration. Touch bindings remain a compatibility target; Index is the
actual hardware acceptance target for this session.

### Required live recording and replay harness

The recording boundary belongs before gesture/interaction decisions, after the
backend has obtained the live poses and action states. Keep the algorithm pure:
the same input frame and previous state must produce the same next state and
commands without a running game. Do not use wall-clock time inside the algorithm.

Capture an explicit, bounded test session rather than logging indefinitely:

- Schema version, source commit/build identity, effective thresholds/settings,
  runtime/controller identity, tracking-space convention and live/replay source.
- Monotonic frame number and actual delta time; head and both controller positions,
  orientations, validity and action availability; normalized buttons/trigger/sticks.
- Focus, world/loading/menu context, menu identity/generation and bridge request
  acknowledgements; surface/capture availability and menu pixel extent.
- Separate audit outputs: scroll phase, physical amount, support/interactor hand,
  surface pose/extent, ray/tip hit, input ownership, commands and release events.

Raw sampled inputs drive replay. Recorded outputs are diagnostic comparison data,
not an unquestioned oracle: recording a bug must not make that bug the expected
test result. Validate requirement invariants and reviewed event checkpoints, then
compare results between builds. Persist the original recording unchanged; derive
named variants for tracking loss, time gaps, premature release, aborted opening,
menu rejection/replacement, focus loss and held-input recovery.

Two replay levels are required: a fast standalone regression runner for geometry,
state and ownership; and an explicit synthetic input source in the game harness
that exercises the real menu bridge/capture and records engine acknowledgements.
Synthetic input must be impossible to confuse with live tracking in logs or
reports, must be opt-in, and must clean up keys, click/drag state, overlays and
temporary settings on completion or failure. The harness must refuse to silently
take over an unrelated active game session. Do not treat a replayed menu command
as evidence of UI selection until the resulting engine/menu state is observed.

One real recording can then support repeated automated diagnosis without asking
the user to repeat the same gesture. It cannot prove unrecorded physical comfort,
controller compatibility or UI actions; retain those boundaries in the report.

Required runtime matrix: horizontal opening; vertical opening; partial opening
and reversal; full opening; laser selection; touch selection; wheel scrolling;
physical closing; tracking loss while opening; tracking loss while open; gameplay
after closing; legacy Tab behavior with feature Off. All are currently NOT RUN.

External research: Exa is absent from available tools. Available official search,
DeepWiki and Context7 (`/valvesoftware/openvr`) were consulted. The DeepWiki answer
incorrectly combined Index grip with its A alias, so it is not sufficient binding
evidence. The actual enum and runtime profile/bindings take precedence:

- https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h
- https://github.com/ValveSoftware/openvr/wiki/VREvent_t
- https://github.com/ValveSoftware/openvr/wiki/IVRDriverInput-Overview
- https://github.com/ValveSoftware/openvr/blob/master/docs/Driver_API_Documentation.md

Exact physical Touch mappings and real haptic delivery: **I could not verify this**.
Read the deployed binding/profile and record button-mask changes with the actual
controllers before connecting those actions. Optional haptics can be replaced by
the readiness/confirmation visual cue until the backend API is verified.

## Initial pure-logic validation

`ScrollMenuTest.cpp`: 509/509 checks passed on 2026-09-21. Tests cover mapping at
every integer partial percentage, stationary input, reversal, exact full-distance
boundary and the preceding float; deliberate horizontal/vertical poses and refusal
conditions; ready dwell/reset; surface projection and ray refusals; and touch
front-side arming, press/release hysteresis and cancellation. This is not a claim
of integration coverage. The later state-machine validation is recorded below.

Rebuilt `hand_mode_test`, `hand_input_test`, and `scroll_menu_test` with the CMake
binary bundled with Visual Studio BuildTools, then ran:

`ctest --test-dir build-ci-tests -C Release -R '^(scroll_menu|hand_mode|hand_input)$' --output-on-failure`

Result: 3/3 test executables passed. Initial build failures were missing CMake on
PATH, duplicate environment keys PATH/Path in MSBuild, and a missing test-only
`<initializer_list>` include. The explicit CMake path, process-local Path casing,
and include fixed those failures. No DLL or game validation is implied.

## Session state validation (2026-09-21)

`src/vr/ScrollSession.h` implements the pure session transition function separately
from geometry, engine requests and rendering. `tests/ScrollSessionTest.cpp` passed
677/677 assertions. This count includes repeated setup checks; it is not a claim
of 677 distinct flows or measured 100% coverage. Both orientations and both support
hands are exercised. Cases include all partial integer percentages, stationary
hands, ordered full-opening then release, release on the first full-distance
sample, premature release, re-grab placement/distance, engine acknowledgement,
timeouts, menu replacement, tracking/action loss and neutral recovery.

The bridge contract distinguishes a command request from an observed owned-menu
generation. A bridge must supply that generation and implement cancellation of
pending requests before this state machine may be connected to live input.
`freeEndReached` must come from real surface geometry. These are explicit adapter
requirements, not capabilities already implemented by the native bridge.

The first test run failed vertical closing at the exact nominal closed distance:
subtracting translated tracking positions rounded slightly above 0.08 metres.
The close test now permits one micrometre of numerical error. The full-open test
has no such allowance and still rejects the preceding representable float.
Review also caught a pending-open re-grab path that could permit another open
request; re-grab is now accepted only after engine acknowledgement. A contradictory
neutral flag cannot override actual held grips when arming the ready pose.

Evidence: `artifacts/scroll-session-validation-20260921/{build,session,ctest}.log`.
The two scroll targets were freshly built. CTest passed 7/7 selected executables:
scroll_session, scroll_menu, hand_mode, hand_input, controller_actions,
openvr_backend and controller_action_assets. Non-scroll targets were reused from
the preceding validated controller build. No game or headset test was run.

## Native input research and outstanding integration

The native menu bridge remains unfinished. Do not copy `dlss5`'s click-only bridge
and label it drag support. Primary sources checked on 2026-09-21:

- [xOBSE Commands_Input.cpp](https://github.com/llde/xOBSE/blob/master/obse/obse/Commands_Input.cpp):
  `Cmd_MenuHoldKey_Execute` and `Cmd_MenuReleaseKey_Execute` explicitly require
  `keycode < 256`. DeepWiki's claim that `MenuHoldKey 256` sustains a mouse press
  was contradicted by this source and rejected.
- [xOBSE Hooks_Input.cpp](https://github.com/llde/xOBSE/blob/master/obse/obse/Hooks_Input.cpp):
  `SetHold`/`SetUnHold` manipulate mouse hold masks for codes >=256; the input poll
  applies those masks. This verifies a candidate held-mouse path, not correct
  dragging or release timing in the installed game.
- [xOBSE GameMenus.cpp](https://github.com/llde/xOBSE/blob/master/obse/obse/GameMenus.cpp):
  `GetMenuByType` indexes the tile array by actual menu type. A generic-menu root
  is not a valid identity for every menu.
- [xOBSE PluginAPI.h](https://github.com/llde/xOBSE/blob/master/obse/obse/PluginAPI.h)
  exposes native event registration, but a menu lifecycle callback was not
  verified. Observing only a root pointer cannot prove an intervening close and
  reopen did not reuse that pointer. Resolve this before claiming stale-packet
  protection across native menu lifetimes.

Exa was unavailable; Context7's xOBSE library lookup returned unrelated libraries,
so none were used. Official source takes precedence over unsupported summaries.

Two worker attempts at the bridge/recovery slice produced no usable report after
focused status recovery. Both were interrupted. The parent used the AGENTS.md
emergency exception for the bounded read-only recovery, source verification and
pure session implementation above. The unfinished `HandSession.h` was preserved;
it is not integrated or validated as a gameplay safety feature. Native bridge,
scroll presentation/settings, live capture/replay and the runtime test matrix
remain outstanding. No feature-completion or runtime-correctness claim is made.
