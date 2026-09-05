# Failed approaches and dead ends

Every entry here was tried, and the evidence for why it failed is named. A future agent
must not retry one without reading its "do not retry unless". Partial successes are
included when the part that failed is the part someone would rebuild.

Format: goal, context, implementation, observed result, why it failed, evidence, git
history, could it become viable, do not retry unless.

---

## Alternate eye rendering with one eye submitted per frame

Status: DEAD END. Scope: OpenVR (`LIKELY` every compositor).

- **Goal:** depth without drawing the world twice, by submitting only the eye drawn this
  frame and letting the compositor keep the other.
- **Implementation:** camera offset to alternating eyes in the camera hook, one `Submit`
  per frame (`432149c`).
- **Observed:** every call returned success; SteamVR faded to its Home scene after about
  ten frames; nothing in the headset, nothing in the log.
- **Why:** the compositor counts a frame as delivered only when both eyes have been
  submitted; a single eye is a successful call and not a frame.
- **Evidence:** `docs/verification/OBVR-aer-refused.log` (decisive by omission).
- **History:** `432149c` -> `5414f44` (diagnosis) -> `69fd955` (OBVR-owned copies of both
  eyes, both submitted every frame).
- **Viable again?** Only with a compositor that documents single-eye submission. Not
  OpenVR.
- **Do not retry unless** the runtime's documentation says a frame may consist of one eye.

## Submitting from the camera hook (before the frame is drawn)

Status: DEAD END for game pixels; fine for a static test pattern.

- **Goal:** the simplest frame loop, one hook.
- **Observed:** with AER the picture would not hold still and got worse on sideways head
  movement; with mono it was one frame stale (which looks exactly like a correct picture).
- **Why:** at camera-hook time the back buffer holds the *previous* frame, drawn from the
  previous camera position - under AER, the other eye's. Each eye was shown the other
  eye's viewpoint: inverted disparity.
- **Evidence:** `OBVR-aer-first-light.log`; `BackBufferEyeIsLeft` in `FrameLogic.h`.
- **History:** `547fbf4` (the `!isLeftEye` correction), `619a586` and `SubmitAtFrameEnd`
  (submit from `Present`, vtable 17).
- **Do not retry unless** submitting something that does not change per frame.

## Rendering with an unpredicted pose the compositor did not hand out

Status: DEAD END. Scope: OpenVR.

- **Implementation:** `WaitGetPoses` called, its poses discarded,
  `GetDeviceToAbsoluteTrackingPose` asked with zero prediction; order render, submit, wait.
- **Observed:** the world morphed on head movement; motion sickness.
- **Why:** the compositor reprojects against the `WaitGetPoses` pose and assumes it was
  used (ValveSoftware/openvr issue #518); and the pose asked for described the head now,
  not at photon time (about 25 ms out).
- **History:** `3f0d557` (render with the render pose; order wait, render, submit).
- **Do not retry** at all; it is a documented misuse.

## Fixed 80 % / 100 % texture bounds for the eye pictures

Status: DEAD END.

- **Observed:** world 1.61x too large across, 2.31x down, stretched 1.43, HUD out of view.
- **Why:** the bounds bore no relation to either the game's or the eye's frustum.
- **Fix:** `PlacePicture` from both frustums (`bd1c2c8`, `b8a827c`). The mono path still
  uses the old bounds deliberately.
- **Do not retry** a fixed fraction; compute the placement.

## Forcing Oblivion onto a D3D9Ex device (the dependency-free route)

Status: DEAD END. Scope: Oblivion (the crash), General (the idea's cost).

- **Goal:** avoid the DXVK dependency: a 9Ex device can share surfaces into D3D11, which
  `Submit` accepts as `TextureType_DirectX`.
- **Implementation:** `Debug.D3D9ExProbe=1`: `Direct3DCreate9Ex` handed to the game,
  `CreateDeviceEx` for the device.
- **Observed:** `CreateDeviceEx` returned 0, then a crash right after the HUD's first
  dynamic vertex buffer, before the first 2D pass - on Microsoft's runtime and on DXVK
  alike; the native control run reached the main menu.
- **Why:** not taken from the dump. The managed pool (9Ex has none) is the obvious suspect;
  that is a reading, not a measurement.
- **Evidence:** `docs/verification/OBVR-d3d9ex-native-control.log`, `-native-9ex.log`,
  `-dxvk-9ex.log`; `HANDOFF.md` "Tried on the binary, 2026-09-03".
- **History:** `e7d7e3d`. The probe stays in the code.
- **Viable again?** Only by shimming `D3DPOOL_MANAGED` (as DXVK-style wrappers do) or by
  finding the actual crash site. Cost: a D3D9 wrapper of DXVK's size. Not worth it while
  DXVK works.
- **Do not retry unless** DXVK on Windows becomes unusable for the target audience.

## Writing `iSize W/H` into the engine's live INI settings to unify the 2D screen size

Status: DEAD END, dangerous. Scope: Oblivion (`LIKELY` Bethesda legacy).

- **Goal:** one screen size for layout, mouse and hit test on an eye-sized frame.
- **Implementation:** the `IniSettingCollection` walk (`game/IniSettings`), validated,
  written through `SafeWrite` at `CreateDevice` time.
- **Observed:** the split closed; then the game crashed at `Oblivion.exe+0x98749` (its
  fullscreen mode path meeting 4028x3380) on every later start, **even with OBVR idle**,
  because the engine had written the moved values back into the user's `Oblivion.ini`
  during the very first patched run.
- **Why:** the engine persists its settings to disk on its own; a value it persists is not
  borrowable for a session.
- **Evidence:** `f577b1c`, the Windows event log; the user's INI restored from backup.
- **History:** `bac4525`, `aaef837`, `f577b1c`. The walk stays as tested code, uncalled.
- **Viable again?** No. The cut was made at the one copy the engine never persists
  (`0x00B06C4C/50`, `7181370`).
- **Do not retry** writing any engine setting object; find the process-local copy.

## Rewriting the NiDX9Renderer's own width/height (+0xA58/+0xA5C) for the 2D layout

Status: DEAD END (harmless).

- **Observed:** the pair already held the believed size; rewriting it changed nothing.
- **Why:** real fields, wrong lever; the 2D reads the window-creation copy.
- **History:** `bd17287` -> `7181370`.

## Redirecting the cursor-movement function's renderer size getter to the screen-size copy

Status: DEAD END.

- **Goal:** fix the sprite-versus-hit-test offset under a raised copy.
- **Implementation:** `CursorMapHook` (deleted), redirecting calls to `0x403190`.
- **Observed:** the getter returns 0 for all three axes in vanilla (its fields are set
  together once and stay zero); redirecting armed an inert term; the cursor node walked
  off screen and the sprite vanished; two of three runs froze the headset picture.
- **Evidence:** the shim's own log lines; commit body `4582c12`.
- **History:** `cb9bd1e` (in), `4582c12` (out); the dead end is recorded beside
  `kUiScreenWidthCopy`.
- **Do not retry** reading a function's purpose off its call sites' names.

## Writing `SetGameResolution` without resizing the window

Status: DEAD END (fixed by completion).

- **Observed:** back buffer 3200x3200, fullscreen cleared, the window still 320x240, DXVK's
  swapchain built at 320x240, mouse mapped against it, `VK_ERROR_DEVICE_LOST`.
- **Why:** in exclusive fullscreen Direct3D sizes the window itself; windowed, nobody does.
- **History:** `af627ef`, `d6c9ab2`, `b05a2fb` (off), `44c09fa` (window sized), `858879e`
  (back on at the headset's size).
- **Lesson:** changing the frame size has three parts: the buffer, the mode, the window.

## Writing `iSize` into `Oblivion.ini` for the next run (`core/GameIni`)

Status: DEAD END by design (a detour), deleted in `af627ef`.

- **Why:** goes around the place the decision is made, needs two restarts, edits a file
  that belongs to the user. The `CreateDevice` hook is the place.

## Guessing "is a menu up" from whether the camera hook ran

Status: DEAD END.

- **Observed:** menus snapped open and shut at frame rate in the headset (stereo and
  cinema alternating), invisible on a monitor.
- **Why:** the guess flipped between frames; the truth is `IsMenuMode` (`0x00578F60`).
- **History:** `9e9be79`.

## Falling back to the cinema screen on menu frames without a world render

Status: DEAD END (the same flicker, met a second time).

- **Fix:** `HeldStereo` - resubmit the last pair with its own pose (`d8b9b6a`,
  `ee8218a`), and the worldless-streak bridge for the closing seam.

## Believing "Oblivion keeps drawing the world behind a menu, but not every frame"

Status: DEAD END (a false belief carried over from the flicker investigation).

- **Observed when measured:** the scene counter stands still for the whole time an
  inventory or Esc menu is up; no menu frame ever carries a camera pass; `WantsHudRedirect`
  requiring an open frame therefore declined every menu frame, and the menu was drawn
  into the back buffer nobody in the headset saw.
- **Evidence:** the menu trace, `HANDOFF.md` "Correction, measured".
- **History:** `364a60d`, `ef0a43e` (redirect derived from the delivery).
- **Lesson:** a claim inherited from an earlier investigation is not a measurement.

## A live menu background rendered from `Present`

Status: DEAD END (shelved, then replaced).

- **Goal:** the paused world freshly rendered per eye behind pause menus.
- **Implementation:** OBVR calling `kRenderScene` itself from the Present hook on held
  frames (`7274a3d`, `cb6b067`).
- **Observed:** the render runs to completion, issues ~344 vertex setup calls and **not
  one draw**, on world frames as on menu frames; from inside the 2D pass the identical
  call draws 461 primitives. The "empty because the culling list is empty" theory was
  wrong (`NiCullingProcess+0x08` is null on every frame).
- **Why:** the moment decides it, not the menu; what exactly Present lacks is not known
  (a suspected accumulator/BeginScene state).
- **Evidence:** `498a1e7` body; `GameAddresses.h` at `kCullingProcessListOffsetDeadEnd`;
  the place probe.
- **Replaced by:** the engine's own live background (clearing the static-background copy,
  `d14169b`) plus `MenuStandIn`.
- **Do not retry** a self-initiated world render from Present.

## A full device state restore between the two world renders

Status: DEAD END.

- **Goal:** rule out device state leaking from the between-pass work into the second
  render.
- **Observed:** bodies stayed collapsed in the left eye exactly as before, and every
  in-game menu's text turned red.
- **Why:** the fault was in the first render, and a `D3DSBT_ALL` restore mid-frame is not
  free of consequence.
- **History:** `ae0f4ef`, `ec5bea7`, `02116d5` (out; `DeviceState.cpp` deleted).

## Verbatim replay of the first render's bone rows onto the second

Status: PARTIAL - held the collapse down, froze every skinned body at the first eye.

- **Why:** the palettes are camera-relative; a verbatim row is one eye wrong.
- **History:** `a1d7791` -> `b75ca70` (shifted replay).
- **Do not retry** verbatim; shift by the eye step with the calibrated sign.

## Selective bone lock: keep "correct" rows, rebase only "mixups"

Status: DEAD END.

- **Observed:** bit-identical rows are stale (skeletons the second render never
  re-evaluated), not correct; most "mixups" judged by translation distance were ring
  collisions between same-posed instances; bodies grey, double vision unchanged.
- **History:** `91dd3c8` -> `b75ca70`.
- **Do not retry** judging rows by their own translation; the second render's
  translations are the thing being overwritten.

## Refusing off-position bone pairs a body length apart

Status: DEAD END.

- **Observed:** in frames where the second render uploaded fewer rows (1761 vs 1793) the
  ring slipped, every later pair sat off position with the mixup's translation, and 1322
  of 1414 rows were refused - the collapse itself. Ten of 48 sampled frames.
- **History:** `fcac8a3` (in), `e4342b7` (out). The "same-posed stranger" hypothesis stays
  a hypothesis.
- **Do not retry unless** the pairing can use something other than the translation.

## Pairing bone rows by upload position

Status: DEAD END.

- **Observed:** twitching only while the mouse turned the view; a camera turn reorders
  the second render's uploads.
- **Fix:** pairing by the nine rotation floats (`cd63974`).

## Shifting shadow and reflection sub-pass bone rows

Status: DEAD END (fixed by exclusion).

- **Observed:** shadows stood in the room like a body instead of lying on the ground.
- **Why:** those passes are camera-free; a shifted character inside the shadow render
  gains a body's parallax.
- **Fix:** rows heading for a target narrower than the world's pass through (`1ea2994`).

## Opening the second render's frame clock by a microsecond

Status: DEAD END.

- **Goal:** let FaceGen's time-gated rebuild see the second eye's camera (the head-part
  offset).
- **Observed:** the offset gained a vertical component on top of the sideways one.
- **History:** `ceb2389` (in), `664b56d` (back to zero). The fix lives in the FaceGen path;
  open.

## Locking the hair (c31) bone class verbatim / leaving it unlocked

Both tried: locked verbatim it displaced every helmet sideways; unlocked the helmets kept
their offset. Resolved by content pairing plus the shift (`cd63974`, `HookedSetVsConstantF`
comment). The head-part class (c14) unlocked showed as eyes and teeth beside the face.

## Turning the body to the gaze and unwinding it eased over a quarter second

Status: DEAD END - **nausea**.

- **Why:** the compensation is applied from the next frame's base rotation, so a
  multi-frame unwind lags the body by one frame every frame: a small continuous drift.
- **History:** `c79160e` (in), `e334cac` (revert). The one-frame return after the attack
  is a different fix, not a slower one (`AimReturnWanted` comment).
- **Do not retry** any multi-frame body unwind under the base compensation.

## Straightening the body on the release frame

Status: DEAD END.

- **Why:** the arrow spawns several frames after the release; the heading at *that*
  moment is the arrow's. Straightening on release sent every shot forwards.
- **Fix:** wait for `currentAction` to leave the bow states, with a time limit (`4f207b1`).

## Turning the body for the whole attack (draw included)

Status: PARTIAL - shots went where you look, the character walked sideways for the
whole draw and the bow swung and sprang back.

- **Fix:** turn only for the shot (`a90af51`, `AimTurnOnShotOnly`); then the source aim
  made the whole turn unnecessary.

## Reading the spell through the "casting" flag at `MiddleHighProcess+0x14C`

Status: DEAD END (superseded).

- **Why:** it stood for 79 frames = 54 Attack + 25 FollowThrough and could not separate
  them; the action field says the same with more resolution and two sources.
- **History:** `1897c52`, `7fbaaae`; the reader deleted, the finding kept in `PlayerAim.h`.

## Turning the spell's heading inside `CastMagicItem`

Status: DEAD END.

- **Why:** that call is the *start* of a 53-frame animation; the projectile is made at
  its end along whatever heading the body has then. `e90f411` -> `89ea515` (turn armed and
  made a measured lead time later).

## A fixed lead time for the cast turn (0.70 s)

Status: DEAD END.

- **Why:** 53 frames is 0.88 s at 60 Hz and 0.59 s at 90 Hz; the animation had ended
  before the turn came, five casts out of five. Measured per cast in frames now
  (`f5de113`).

## Wrapping only the player's animation-key handler vtable slot for the aim

Status: DEAD END (incomplete reading).

- **Observed:** swings and casts entered, the bow never did, the spell still went straight.
- **Why:** the player's attacks go through the attack update called directly from the
  input handler; the handler is the NPC route.
- **History:** `6661d55` -> `e29bb0a` -> `079cec4`.

## Modelling the third-person chase rate at 5 % per frame

Status: DEAD END.

- **Why:** the physics steps the camera at 60 Hz under a 90 Hz renderer; two moving frames
  then one still. Measured off the camera every frame instead (`73cb288`).

## Writing the player's pitch every frame in third person

Status: DEAD END (argued, then measured).

- **Why:** the third-person camera is built from `rotX` and eases towards it; a written
  pitch carries the viewpoint along a second later and `LookControl` turns that tilt into
  height. Written only while aiming, borrowed and returned (`1314478`).

## Levelling the flat anchor to yaw only (first form)

Status: reversed twice; current state yaw-only by decision. See
[ui-and-hud.md](ui-and-hud.md#screen-space-assumptions-that-break-in-vr-listed).

## A custom drawn third-person crosshair

Status: removed (`b0cd3d9`, no body). The borrowed genuine crosshair replaced it.

## Deriving the flat picture's height from the world's angular rectangle

Status: DEAD END - with `MatchHeadsetFov` the angles are nearly square, so every menu
after the frustum arrived was a 16:9 frame squeezed into a square (the main menu escaped
because it is placed before the frustum arrives). Fixed at the cause: a flat picture's
shape comes from the frame's pixel aspect or `MenuAspect` (`HANDOFF.md` "ESC and
inventory menus were squeezed").

## Cropping the flat picture to a 16:9 corner at 1:1 (the "cinema crop")

Harmless at 16:9, wrong at a square frame (halved intro films); zero since `ef0a43e`-era
fixes and replaced by the believed-size crop (`ContentBounds`).

## `HeadMovementScale` as a substitute for stereo depth

Status: PARTIAL, a preference not a measurement: 1 -> 2 -> 1.7 -> 2 -> 3 while mono. Back
to 1.0 once stereo rendered. A number that keeps climbing is chasing a missing cue.

## The 64-bit helper process for the VR runtime

Status: not tried; rejected as oversized for reading a quaternion and submitting two
images. praydog/FEAR2VR later took exactly that route (shared-memory frame publisher,
`xr64.exe`) to reach OpenXR from a 32-bit game; see
[ecosystem-and-prior-art.md](ecosystem-and-prior-art.md). Viable if OpenXR ever becomes
a requirement.

## Linux as the development environment for the headset work

Status: DEAD END on 2026-08-25 (SteamVR crashed the GNOME/Wayland/NVIDIA desktop; the
Beyond needs its Windows driver for display activation). `HANDOFF.md` section 11. Linux
under Proton remains the intended second platform for *users*.
