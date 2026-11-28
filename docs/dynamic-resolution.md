# Dynamic resolution for the single pass

`[Render] DynamicResolution` (default 0), `DynamicResolutionMin` (0.5),
`DynamicResolutionTarget` (0.85). Built 2026-11-08 on the measurement that
the heavy Unique Landscapes / Better Cities place is GPU-bound as well as
CPU-bound (OBVR capture 1-31, 2026-11-07: 15 to 19 ms of GPU in a 25 ms
frame); the occlusion culler (OBNG) takes 4 to 15 % off the GPU, the rest of
the way to the headset's period has to come from fewer pixels when the GPU
is late.

## What it does

- Once per frame, in `HeadsetRenderer::BeginFrame`, the compositor's GPU
  time of the frame finished one back (`Compositor_FrameTiming.preSubmitGpuMs`,
  "gpu work submitted between WaitGetPoses and second Submit", openvr.h) is
  read. Readings under 0.5 ms (an empty frame) or over three periods (a
  hitch) are left out; the median of the last five decides against the
  budget `DynamicResolutionTarget` x the headset's period. Over the budget:
  the scale goes down by the square root of the overrun (pixels go with the
  square of the scale), at most an eighth a frame. Under three quarters of
  the budget for fifteen frames: up one step of 1/32. Never below
  `DynamicResolutionMin`, never above 1 (render/DynamicResolutionLogic.h,
  every flow in tests/DynamicResolutionLogicTest.cpp).
- The scale takes effect in three places that read one layout
  (`DynamicResolutionLayout`): the world camera's port
  (SceneRenderHook.cpp `WorldPortScope`), the eye viewports handed to OBNG
  (`EyesFor` with the layout's extents) and the eye copy's source crop
  (`EyeMirror::CopyBackBuffer`), which StretchRect spreads over the eye's
  whole picture. Nothing downstream changes: the submit, the HUD layer, the
  held eyes, the menus and the hands stay at full resolution.
- The port's right and bottom edges are put half a pixel past the wanted
  extents, so the engine's truncation (`Do_SetCameraData` 0x00762640:
  Width = trunc((right - left) * W), Height = trunc((top - bottom) * H),
  read in ../OBNG/re/functions/0x00762640.md C7) lands on them whatever the
  rounding of the products. At the full scale the engine's own left-half
  port is kept.

## Measured (Linux harness, 2026-11-08, runs ../OBNG/artifacts/shadow/dynres-gold-road-off/20261108-182939 and dynres-gold-road/20261108-183129)

Gold Road -27 -3, single pass, the route on, the culler off, 75 Hz, 2568 px
an eye. The on run with a budget of 30 % of the period (4 ms), which the
Gold Road's 7 ms of GPU cannot meet, so the scale had to move:

- **The plumbing holds**: OBNG's stereo route accepted the scaled viewports
  in every frame with eye data (eye data set 370, refused 0; draws outside
  eye 0's viewport 59 833 against 53 784 in the off run - the inner passes,
  not the world), no crash, the harness PASS. The engine's truncation met
  the layout.
- **The GPU time did not fall** (7.26 ms mean on, 7.25 off) although the
  scale sat between 0.5 and 0.97. Derived: on the Linux harness the frame is
  CPU-paced at vsync (13.3 ms, the scene pass 4 ms); the compositor's GPU
  span then follows the CPU's draw submission, not the pixel count - the
  same reason the culler, which removes draws, moved the GPU time and a
  pixel cut does not. The pixel-bound case is Nadi's Windows place; the
  measurement that decides is there.
- The first controller moved the scale 28 times in 40 s on single readings
  (0.13 ms for an empty frame, 46 ms for a hitch); the median of five with
  the plausibility bounds replaced it (f600b90), not yet run in the game.

## Open

- The picture: the stretched copy's look in the headset (bilinear from the
  smaller region), the HUD and hands against the softer world; whether the
  compositor's distortion resample on top of the stretch shows. A later
  variant hands the compositor texture bounds instead of stretching (one
  resample less).
- The controllers drawn into the world (`ControllerModels::DrawIntoWorld`)
  read the device's viewport at the eye capture; with a scaled viewport
  `UseEyeRect` may refuse and the fallback draws them on top of the picture
  (unverified; the hands are the game's own and unaffected).
- The dual pass gets nothing (the scale applies to single-pass frames only).

## State-change audit (2026-11-28)

Checked against `D:/Modding/claude-md-state-changes.md` ("State changes without glitches or side
effects": principles P1-P8 and its verification section). Code read on origin/main 5214464 (all file:line below are origin/main's), nothing run.
R = read in the code or a spec, D = derived from what was read, G = guessed.

Findings:

1. **The scaled port on frames that are not single-pass frames** (P4, P8, "the same checks for
   both render paths"). The world port is cut to the scale on every world render of a
   side-by-side frame: `WorldPortScope` (render/SceneRenderHook.cpp:187-218, the port at :214) takes
   `DynamicResolutionWorldPort()` (render/DynamicResolution.cpp:103-111), which checks only
   `DynamicResolutionActive()`. The eye crop is cut only on single-pass frames
   (`DynamicResolutionCrop`, DynamicResolution.cpp:113-119, `IsSinglePassFrame()`). So on a
   dual-pass fallback (OBNG refuses the eyes, plan not 2) or a plain render (menu frames,
   SceneRenderHook.cpp:398-445) with the scale under 1, the world is drawn into the smaller
   corner while the copy reads the whole left half: the picture shrinks into the top left, with
   stale pixels around it, for every such frame. On top of that the scale only moves inside
   `HandOverEyes` (DynamicResolution.cpp:96-101), so it stays frozen under 1 while frames run dual.
   This contradicts "The dual pass gets nothing" above. [R the code paths; D the picture; not
   run, since DynamicResolution defaults to 0. The mirror's render (render/MirrorGlassPass.cpp on origin/main) sets its own port and is not affected.] Fix: decide the scale once per frame (1 unless
   the single pass took the eyes) and have port, eyes and crop read that one value. Add a unit
   test that they agree on both paths.
2. **The scale jumps between frames** (P5). Down by up to an eighth in one frame, up by 1/32,
   with no blend (DynamicResolutionLogic.h, described in "What it does"). The sharpness changes
   in steps. [R; D whether the step is visible in the headset.] Fix: if the headset shows it,
   take smaller steps down unless the frame is far over budget.
3. **Complies: both eyes use one layout** (VR note). The eyes, port and crop all read
   `DynamicResolutionLayout` within the same frame, and `HandOverEyes` updates the scale before
   `RenderThroughPort` computes the port (SceneRenderHook.cpp:280 before 327; EyeMirror.cpp, the crop's `DynamicResolutionCrop` call). [R]
4. **Controllers drawn into the world under a scaled viewport**: already open above
   ("`UseEyeRect` may refuse"). [R, spec]

Not verified:
- No test checks that port, eyes and crop agree per path (no test references
  `DynamicResolutionCrop` or `DynamicResolutionWorldPort`). DynamicResolutionLogicTest covers
  the controller only.
- No harness run with the scale under 1 and a dual-pass or menu frame in between. Nothing was
  run under stress either (fluctuating frame rate, toggling a menu quickly).
- The look of a scale step in the headset.
