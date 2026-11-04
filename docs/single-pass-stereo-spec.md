# Single-pass stereo through OBNG - the measuring prototype (B)

Status: steps 1 (f7b7f7d, the binding), 2 (41b9cca, the side-by-side frame), 3 (506dc09, the world
in the left half), 4 (02bbeec, one pass through OBNG) and 5 (2ed5f7c, the right eye from the right half) built, step 6 measured 2026-11-04 (below), the prototype planned
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

## Step 6, measured (2026-11-04, Linux, MSAA 4, HDR off)

The fixed Walkers scene (save OBVRHarnessWalkers), phase "settled to watched", through OBNG's
`tools/shadow-run.sh --ini tools/hand-scripts/single-pass-obng.ini` (runs under OBNG's
`artifacts/shadow/`), cut with `tools/perf-phases.py <run> ready settled watched done`.
scene_pass is a CPU span per pass (GpuTiming=0):

| Run | Path | Frame ms | over 20 ms | scene_pass ms | poses_wait ms | app GPU before submit ms |
|---|---|---:|---:|---:|---:|---:|
| single-pass-measure-off/20261104-144236 | dual, frame one eye wide | 13.44 | 0.8 % | 2.94 per pass (5.88 for two) | 6.26 | 6.46 |
| single-pass-measure-off/20261104-150032 | the same, repeated (OBNG's timed DLL) | 13.54 | 1.4 % | 3.23 per pass (6.46 for two) | 5.46 | 7.10 |
| single-pass-measure-wide-dual/20261104-144544 | dual on the 5136 frame (StereoEyes=1) | 14.27 | 7.0 % | 2.86 per pass | 7.24 | 6.26 |
| single-pass-measure-on/20261104-145734 | single pass, StereoReapply=view (timed DLL) | 13.57 | 1.7 % | 4.26 for the one pass | 7.65 | 5.08 |
| single-pass-measure-on/20261104-150318 | the same, repeated | 13.42 | 0.6 % | 4.19 for the one pass | 7.58 | 5.07 |
| single-pass-measure-on-noreapply/20261104-144858 | single pass, StereoReapply=none (picture wrong) | 13.82 | 3.7 % | 4.12 for the one pass | 7.91 | 5.19 |
| single-pass-measure-on/20261104-143932 | single pass, StereoReapply=view, the first run | 29.61 | 91.6 % | 14.78 | 6.65 | (unreadable) |

OBNG's timed route summary in 145734: "switch 0.27 us, re-apply 0.10 us per applied switch",
about 1 090 switches a frame, the eye data taken every frame, none refused.

What it says:

- Measured: the one pass costs 4.19-4.26 ms of CPU against 5.88-6.46 ms for the dual pass's two,
  about 2 ms a frame less; the app's GPU time before the submit drops from 6.46-7.10 to about 5.1 ms.
- Measured: the frame mean does not move (13.4-13.6 ms either way) - this scene is held at the
  75 Hz vsync, the saved time shows up as a longer poses_wait (OBNG's note: the fixed scene cannot
  show the frame gain).
- Measured: the re-apply costs nothing worth naming (the none run is within 0.1 ms of the view run).
- Not reproduced, cause open: the first "on" run (143932) took 14.78 ms a pass with 92 % of frames
  over 20 ms; the three later runs of the same scenario did not. A guess, unverified: the first run
  of the Walkers scene into the 5136 MSAA frame compiled its DXVK pipelines (Proton's DXVK is not a
  gplasync build). A repeat after a fresh start would show it again if so.
- One run per row in the repeated pairs; the off-to-off spread (0.3 ms a pass) is the noise the gain
  stands against.

Decision state: the prototype saves about 2 ms of CPU and 1.5-2 ms of GPU a frame on a scene the
vsync hides. Whether A pays off depends on a scene that is not vsync-held - a heavy outdoor scene
(the CPU-bound ones of 2026-11-02, Windows) - and on the open pieces A needs (eye 1's bone palettes
and water constants, image space, the monitor mirror).

## Step 6 outdoors (2026-11-04 afternoon, Linux, Unique Landscapes installed)

Unique Landscapes was installed in the Linux game for this (Full Merge Standard Version, the core BSA,
the Robert's Male meshes; file list in `D:\Modding\_mod-backup-2026-11-04-linux-ul\installed.csv`,
load order before MOO). The Gold Road scenario (perf-gold-road.txt: three spots between Kvatch and
Skingrad, eight head directions and a walk at each, save OBVRHarnessMOO) as
`single-pass-gold-road-on/-off.txt`, one run each, the loads between the spots left out:

| Run | Frames | Frame ms | over 20 ms | scene_pass per frame | poses_wait | app GPU before submit |
|---|---:|---:|---:|---:|---:|---:|
| single-pass-gold-road-off/20261104-153026 | 6 895 | 13.49 | 1.0 % | 4.80 (2 x 2.40) | 4.61 | 6.38 |
| single-pass-gold-road-on/20261104-152647 | 6 894 | 13.36 | 0.2 % | 3.26 (one pass) | 6.10 | 5.54 |

Per spot (scene_pass per frame off -> on): s1 4.28 -> 2.62, s2 5.12 -> 3.36, s3 5.00 -> 3.85. At s2
the dual pass left 1.31 ms of poses_wait, the single pass 4.81. The capture counts one scene_pass a
frame with the setting on and two without (8 865 against 8 880 + 8 875). OBNG: switch 0.26 us,
re-apply 0.09 us, about 480 switches a frame.

Measured: about 1.5 ms of CPU and 0.8 ms of GPU a frame less outdoors, a third of the scene passes'
CPU; the frame mean stays vsync-held on this Linux setup.

## The first run's outlier (143932)

Not reproduced: four later runs of the same scenario took 4.19, 4.26, 3.70 and 3.68 ms a pass (the
last two after Unique Landscapes went in, so not comparable with the pre-UL off runs). What set 143932
apart, read from its log and capture: SteamVR went to interleaved reprojection just before the capture
and stayed there for 2 792 frames, and its scene_pass was slow even in the frames under 20 ms (11.15 ms
against 4.31 ms in 145734). Half-rate frames carry a longer scene_pass in the dual pass too (an older
run, perf-walkers-standing/20261026-151742: 9.30 ms in frames over 20 ms against 4.88), so half rate
explains part of it but not the slow full-rate frames. Cause open; no build or other harness run of
another session is on record in that window.
