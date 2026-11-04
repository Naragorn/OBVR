# Single-pass stereo through OBNG - the measuring prototype (B)

Status: steps 1 (f7b7f7d, the binding), 2 (41b9cca, the side-by-side frame), 3 (506dc09, the world
in the left half), 4 (02bbeec, one pass through OBNG) and 5 (the right eye from the right half) built, the prototype planned
2026-11-04 (Nadi: "B").
OBNG's half is built and measured on its side: OBNG.dll's `plugin/src/StereoEyesApi.h`
(ObngStereoPlan / ObngStereoSetEyes / ObngStereoClearEyes) and its
`docs/subsystems/single-pass-stereo.md`, "Step 4". With eyes set, OBNG draws every partition twice:
eye 0 the pass camera as the engine set it, in the viewport the engine's own SetViewport leaves;
eye 1 that camera moved by `secondEyeOffset` (world units, OBVR's g_dualShift) into `viewport[1]`.

## Why a prototype first

OBNG projects one pass at about 3.9 ms against 5.6 ms for OBVR's two (the fixed Walkers scene) -
a projection from its switch cost, not a measurement of a frame. OBVR's whole eye pipeline assumes
one eye-sized picture per pass, so the full path (A) touches ten places, several of them where the
old stereo bugs lived. The prototype builds only what a measurement needs; if the fixed scene shows
no clear gain, A is off the table.

## What OBVR does today (read 2026-11-04)

- One back buffer, the size of one eye (`render/ResolutionHook.cpp:114-119`, chosen at
  `camera/CameraHook.cpp:12368-12391`); the UI's believed size follows it (`UiSizeForFrame`,
  `render/UiScreenSize.cpp`), and with it the HUD texture (`render/HudLayer.cpp:127`) and the cursor.
- Both passes draw the whole frame; each eye is a StretchRect crop of the whole frame
  (`render/EyeMirror.cpp:273-276`, `CopyBackBuffer` 492), captured after the first pass
  (`CameraHook.cpp:7799`) and after the second (7890), submitted without bounds
  (`render/HeadsetRenderer.cpp:680-683`).
- The camera frustum is the widest of both eyes, symmetric (`CameraHook.cpp:10141-10219`); OBVR
  never writes the NiCamera port (+0x110, `game/GameCamera.h:39-43`) and resets the world viewport
  to the full frame before each pass (`render/InterfaceRenderHook.cpp:3464`).
- Without MSAA the world goes through a frame-sized texture and a fullscreen copy
  (`game/SceneFrame.h`) - image space across a double-wide target would cross the eyes' seam.
- Camera-relative per pass: bone palettes (`render/BoneRebase.h`, `SetBoneEyeShift`), the water's
  vertex constants (`render/WaterReprojection.cpp:243+`), culling (`render/CullingSync.h`).

## The prototype's scope

Everything behind `[Render] SinglePassStereo` (default 0). Only:

- world frames with MSAA on (the image-space texture path stays the dual pass);
- water reflections and shadow sharing off on the single path;
- menus, the HUD frames that need it, loading frames: the dual pass as today;
- culling from the frustum widened to cover both eyes from eye 0 (no union yet).

Not in the prototype, written down so nobody takes it for done: the bone palettes and the water
constants for eye 1 (people and water may look wrong in the right eye), image space, the monitor
mirror, the controllers drawn into the world.

## What step 2 leaves for later

The setting is read at device creation, so it needs a restart. Until step 3 the dual pass draws the
world across the whole wide frame (stretched 2:1) and each eye's crop squeezes it back: the
geometry holds, half the horizontal pixels are wasted. The flat picture's placement log reads the
frame as 5136x2568 at 75 degrees; its rectangles came out the same as with the setting off.

## What step 3 rests on

RenderScene 0x0040C830 writes the main camera's port (+0x110) itself in two places, read
2026-11-04 with llvm-objdump: 0x0040CBEF for a scaled target (a ratio below 1), and 0x0040CF93 after
image space, 0, 1, 1, 0. OBVR writes the left half only over exactly 0, 1, 1, 0 and restores what it
read after each world render; any other port is logged and left alone. Not checked: image space
(bloom, HDR) on the half port - the prototype's runs are MSAA without HDR.

## Steps, each with its test

| Step | What | Test |
|---|---|---|
| 1 | Bind OBNG's three functions, log the plan (done, f7b7f7d) | obng_stereo_test; harness single-pass-bind PASS |
| 2 | The back buffer twice an eye's width when SinglePassStereo is on; the UI's believed size kept at one eye (done: `BackBufferForEye`, refused past DXVK's 16384, `doitsujin/dxvk src/d3d9/d3d9_caps.h` MaxTextureDimension) | obng_stereo_test; harness single-pass-frame PASS 2026-11-04: 5136x2568 for a 2568x2568 eye, the 2D believes 2568x1444 as with the setting off, the Esc menu at the same back-buffer pixels, every headset placement line identical. Not checked: the world in each eye (no eye dumps on Linux) |
| 3 | The world pass's viewport the left half (done: the main camera's port, `DecideWorldPort`, written only over the engine's (0, 1, 1, 0) and given back after each world render; the viewport reset at InterfaceRenderHook.cpp puts back the left half; the eye crops read the left half, `WorldPictureWidth` - brought forward from step 5 so the dual pass stays right on the wide frame) | obng_stereo_test; harness single-pass-left-half PASS 2026-11-04: the port read 0, 1, 1, 0 and set; both eye dumps the same view as with the setting off |
| 4 | One pass when the plan is 2: SetEyes before the world render (eye 0 the left half, eye 1 the right, the offset g_dualShift), ClearEyes after; no second pass, no camera shift (done: `HandOverEyes` / `RenderSinglePassFrame` in SceneRenderHook.cpp; only with the frame side by side and the left eye first; betweenPasses and afterSecondPass still capture and draw the 2D layer, the camera stays) | obng_stereo_test (path, WorthSettingEyes, EyesFor); harness single-pass-one PASS 2026-11-04 through OBNG's tools/shadow-run.sh --ini tools/hand-scripts/single-pass-obng.ini: plan 2, the eyes taken every frame (OBNG: 746 set per 10 s, 0 refused), offset 4.339 units, one pass a frame; OBNG's viewport gate (its commit of 2026-11-04) kept 799 draws per 10 s single (shadow maps, water pre-pass). Both eyes still from the left half until step 5 |
| 5 | Capture both halves: eye 1's crop from the right half on the single pass (the left half for both eyes is done in step 3; done: `EyeSourceOffset` / `ShiftedCrop` in EyeMirror::CopyBackBuffer) | obng_stereo_test; harness single-pass-one PASS 2026-11-04 with `Debug EyeDump=1` (OBVR's own dumps, Linux too): the best left-to-right shift 103 px on the far wall and 167 px on the near floor, exactly the dual pass's; against the dual pass's dumps RMSE 0.07 % (left) and 0.26 % (right) |
| 6 | Measure on the fixed Walkers scene: scene pass, app GPU before submit, frame - against the dual pass | the numbers, both ways, same build |

OBNG's run protocol applies: BUSY before every game run, FREE after (obng-7c).
