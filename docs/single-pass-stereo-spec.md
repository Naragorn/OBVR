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

- world frames with MSAA on (the image-space texture path stayed the dual pass until A3, 2026-11-08);
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

## A - the full path (Nadi 2026-11-04: "yolo. go A")

| Step | What | State |
|---|---|---|
| A1 | People in the right eye | nothing to build: the palettes are relative to the camera origin 0x00B3F92C, which the eye switch leaves alone (OBNG, read); measured single-pass-people-on/-off: shift far wall 106 px, near Walker 114 px, the same both ways |
| A2 | Water scene, the right eye | fixed on OBNG's side (OBNG e3bc2e9, 2026-11-04): the near terrain and SpeedTree draw through the renderer's slots 90 (0x007694F0) and 91 (0x00769700), not through 0x007672F0/0x00767520 - found with OBNG's draw-entry probe (about 30 and 400-550 calls a frame), a StereoReapply=slot14 run ruled out the constants; both now routed. single-pass-water-on/20261104-164829: 4 of 4 detours, the right eye's terrain, tree and house as in the dual pass |
| A3 | The texture path (no MSAA with image space) | built 2026-11-08 (Nadi: "go B"), harness in "A3 measured" below. Read in the EXE: RenderScene asks the image-space manager for a texture made for the frame's target (0x0040CA27 -> 0x007C23C0, the target's size read through 0x00701640/0x00701670) and writes the camera's port only when that texture's width or height differs from the target's (0x0040CA63-0x0040CA89; equal sizes jump to 0x0040CC1F and leave the port as it is) - so the left-half port stays, the engine's viewport on the texture is eye 0's and OBNG draws eye 1 into the right half; the plain copy 0x00803E40 puts the whole texture 1:1 into the back buffer. Derived, not read: the texture is frame-sized on our frames - the dual pass on the same 5136 frame draws the texture path right through the half port (OBNG's knowledge/render-thread-outside-the-pass.md, iMultiSample 0: the same eye shifts as MSAA 4); a texture of another size would get the engine's scaled port, OBNG would double nothing and the right eye would be stale (no guard for that yet). Built: `WorthSettingEyes` no longer asks for the back-buffer route; `SceneFrameCloseWanted` leaves a single-pass frame open (it was the dual pass's close between the renders and would have ended the frame before the 2D pass); the eye log names the route (`WorldRouteName`). Not covered: blur or bloom shaders on the texture path would smear a few pixels across the seam between the eyes (HDR is off on both test machines) |
| A4 | The monitor mirror | nothing to do: in VR the monitor window shows no world in the dual pass either |
| A5 | OBVR's own controllers drawn into the world (hand adjust) | built: the right eye drawn into the right half from the right eye's camera (`EyeRectFor`, `EyePositionFor`); unit-tested only - the harness has no controller models (no physical controllers), so the headset is the test |
| A6 | Culling for eye 1 | open: the cull runs from the pass camera's frustum; very near objects at eye 1's right edge may be missing. Not seen yet |
| A8 | Torch flames, near trees and leaves, the water's reflection | checked 2026-11-04, nothing to build: the eyes' best horizontal shift per picture band single against dual - Talos Bridge (single-pass-effects-on/-off, trees and house 101/102/103 both ways, water 109/115 against 106/116 with the waves moving between the runs) and a torch in the hand in the testing hall (single-pass-torch-on/-off, 109/108/125/125/153/169 against 109/110/129/129/153/169: the flame's band 4 px less, the billboards facing eye 0's camera for both eyes - a pure translation, small). The flame is drawn in both eyes |
| A7 | Release | built 2026-11-12 (Nadi: OBNG ships with OBVR from the next beta on, GPL-3.0 as OBVR): `[Render] SinglePassStereo` defaults to 1 (Config.h, OBVR.ini; ConfigTest) - without OBNG.dll or with its stereo off every frame falls back to the dual pass (`StereoPathFor`); a "One-pass stereo" toggle in the settings' Picture & performance rows (restart); `tools/package-release.py`/`.ps1` put OBNG's `build-plugin/OBNG.dll`, `plugin/OBNG-player.ini` (as `OBNG.ini`) and `LICENSE` (as `OBNG-LICENSE.txt`) into `OBSE/Plugins/`, refuse the archive while any is missing, stale against OBNG's sources, not 32-bit, or the INI lacks StereoDraw=1/StereoEyes=2 (every reason at once; `--without-obng` ships without on purpose), and write the DLL's SHA-256 beside the map (`OBVR-<version>.obng.sha256`; tools/package-release-test.py); the doctor (py and ps1) checks OBNG.dll, its OBNG.ini's two keys and OBVR.ini's switch, and reads from OBVR.log which path the last start drew (ObvrDoctorTest.py); README install and uninstall lines. Open: the headset on Windows with the bundled files; with SinglePassStereo=1 and no OBNG the frame is still created twice an eye wide (the dual pass draws the left half, as before step 4) - the right half's memory is unused; making the doubling wait for OBNG.dll needs a run that shows OBNG is loaded before CreateDevice (not checked) |

## A3 measured (2026-11-08, Linux, Beyond 2568x2568, HDR off, OBVR.dll 7d8b9397)

OBNG's `tools/hand-scripts/obng-profile-gold-road-eyedump.txt` through its `tools/shadow-run.sh --ini
tools/hand-scripts/obng-measure-profile.ini` (runs under OBNG's
`artifacts/shadow/obng-profile-gold-road-eyedump/`), the Linux Oblivion.ini's iMultiSample set for the runs
and put back. References from obng-67 the same evening: 193145 (4, single pass) and 193550 (0, the dual pass
before A3). Eye shifts by OBNG's `tools/eye-shift.py`, six bands, far to near; scene_pass per frame (the dual
pass's two passes added), app GPU before submit, by `tools/perf-phases.py` per spot (s1 / s2 / s3, the
"walked" phases):

| Run | iMultiSample | Path | Shift s1-y0 | Shift s2-y0 | scene_pass ms per frame s1 / s2 / s3 | app GPU ms s1 / s2 / s3 | over 20 ms s1 / s2 / s3 |
|---|---:|---|---|---|---|---|---|
| 193145 (obng-67) | 4 | single | 102 102 102 117 110 112 | 103 102 102 106 113 116 | 3.87 / 4.00 / 3.66 | 7.12 / 6.95 / 6.44 | 5.2 / 1.0 / 0.8 % |
| 193550 (obng-67) | 0 | dual (before A3) | 102 101 102 117 110 112 | 102 102 102 104 111 114 | 3.76 / 6.24 / 5.72 (first run under 0, stuttering) | 5.93 / (broken) / 6.92 | 4.5 / 47.0 / 1.7 % |
| 202413 | 0 | **single, the texture route** | 102 102 102 117 110 112 | 104 102 102 104 113 116 | 2.79 / 3.54 / 4.15 | 5.19 / 6.04 / 6.57 | 0.9 / 0.8 / 1.2 % |
| 202826 | 0 | single, repeated | 101 101 101 113 110 112 | 102 103 102 102 113 114 | 2.75 / 3.29 / 4.12 | 5.20 / 5.62 / 5.14 | 0.8 / 0.8 / 0.6 % |
| 203233 | 4 | single (regression) | 102 101 101 101 110 112 | 102 102 102 103 110 113 | 3.00 / 3.71 / 4.11 | (broken) / 6.75 / 5.73 | 0.9 / 0.5 / 0.8 % |

- Measured: with iMultiSample=0 the log reads "single pass through OBNG, the world into the image-space
  texture", the port set to the left half, OBNG took the eyes every world render (738-751 per 10 s, 0
  refused); OBNG's route counted the 5136x2568 copy among the draws outside eye 0's viewport (drawn once).
- Measured: the eye shifts with 0 single equal the 4x run band for band (within 1-4 px, the same spread as
  between two 4x runs); the left and right eye pictures of s2-y0 looked at side by side: both drawn, no stale
  or black eye.
- Measured: the 0 single runs cost about the same scene-pass CPU as the 4x single runs (2.8-4.2 ms a frame),
  against the dual pass's two passes under 0 (5.7 ms at s3, the one phase of 193550 not spoiled by the first
  run's stutter): about 1.5 ms a frame less there. App GPU before submit 5.1-6.6 ms under 0 against 5.7-7.1
  under 4 - derived, one run each and in a vsync-held scene, not a GPU budget measurement.
- Measured: neither 0 single run stuttered the way the first 0 run of 193550 did (0.6-1.2 % over 20 ms).
  The DXVK pipeline guess for 193550 is therefore not settled: the cache may simply have been warm by now.
- Not covered: the Windows headset (iMultiSample=0 in the Windows Oblivion.ini), bloom or blur on the texture
  route (bUseBlurShader=0 on both machines; a blur would smear a few pixels across the seam between the
  eyes), and a texture of another size than the frame (the engine would scale the port; no guard, see A3).
