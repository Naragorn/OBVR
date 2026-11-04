# Single-pass stereo through OBNG - the measuring prototype (B)

Status: step 1 built (f7b7f7d, the binding), the prototype planned 2026-11-04 (Nadi: "B").
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

## Steps, each with its test

| Step | What | Test |
|---|---|---|
| 1 | Bind OBNG's three functions, log the plan (done, f7b7f7d) | obng_stereo_test; harness single-pass-bind PASS |
| 2 | The back buffer twice an eye's width when SinglePassStereo is on; the UI's believed size kept at one eye | unit: the sizes; harness: the log's created and believed sizes, the menus as before |
| 3 | The world pass's viewport the left half: the port or the reset at InterfaceRenderHook.cpp:3464 | unit: the viewport decision; harness: the left half drawn, the right black, with the plan 0 |
| 4 | One pass when the plan is 2: SetEyes before the world render (eye 0 the left half, eye 1 the right, the offset g_dualShift), ClearEyes after; no second pass, no camera shift | unit: StereoPathFor wired, the eye data; harness with OBNG-test.ini (StereoDraw=1, StereoEyes=2, StereoReapply=view) |
| 5 | Capture both halves: each eye's crop from its half (EyeMirror's source rects offset by half the frame) | unit: the rects; harness: both eyes' pictures (Windows eye dumps) |
| 6 | Measure on the fixed Walkers scene: scene pass, app GPU before submit, frame - against the dual pass | the numbers, both ways, same build |

OBNG's run protocol applies: BUSY before every game run, FREE after (obng-7c).
