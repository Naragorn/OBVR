# Performance spec: people in view

Status: measured 2026-10-18 and 2026-10-19, nothing built. The measurements
and the harness scenarios are in `docs/performance-profiling.md` ("Gemessen
2026-10-18" and "Gemessen 2026-10-19"); the tester's report (a Quest 3 at
120 Hz: "stuttering like crazy" with two NPCs talking in Jauffre's house,
smooth in dialogue) in `docs/controls-spec.md` "Beta reports of 2026-10-17";
Nadi's own (2026-10-19, the Beyond at 75 Hz): the drop in the opening
dungeon "sobald die Truppe aus 3 Leuten vor mir steht".

## 1. What is known

- Seven people before the player cost about 4.3 ms of CPU per eye pass on
  the developer's machine (Linux, Proton, 75 Hz): scene_pass 0.70 ms a pass
  empty, 4.3 to 5.1 ms with them; the idle before the compositor fell from
  10 ms to 2 ms; 4 to 6 % of frames dropped to the next vsync.
- OBVR's own share is small: the bone lock 0.056 ms a pass (its own span,
  `bone_lock`), the frame-end work 0.6 ms a frame unchanged. The time is the
  engine's own render of each person - bone palettes and draws, under DXVK -
  which the dual pass runs twice a frame. About 0.6 ms a person and eye.
- A person in armour costs twice a monk (2026-10-19, the opening's prison
  cell): three Blades 3.3 ms a pass, 1.1 ms a person and eye, 20 skinned
  draws each; the cell itself is cheap (1.0 ms a pass). At 75 Hz the dual
  frame still fits (idle 9.5 ms empty, 2.6 ms with them, 0 % dropped while
  they stand, 5 % while they walk up); at 90 Hz the idle would be 0.4 ms, at
  120 Hz the three alone are over the budget.
- **Nadi's drop reproduced** (2026-10-19, the same cell with the opening's
  five: the Emperor, three Blades, Valen Dreth): 6.6 ms a pass for the five
  (1.3 ms a person and eye, 118 skinned draws), two passes 15.2 ms - 2 ms
  over the 75 Hz period, so every frame slides to the next vsync: 26.7 ms,
  100 % of frames, SteamVR reprojecting a third. Under aer the same scene
  runs at 13.4 ms with 7 ms of idle and under 1 % dropped (4.4 % reprojected
  over the run). A frame that misses its period by a little loses the whole
  period: the dual pass has a cliff, not a slope.
- At 120 Hz (8.3 ms a frame) two or three people reach the budget. A Quest
  user over Virtual Desktop at 72 or 90 Hz has more room, not much.
- Interiors can be over budget on their own: Weynon Priory's house sits at
  two vsync periods (26.7 ms) with nobody in it, scene_pass 7 ms a pass with
  322 draws; what makes that cell dear is unmeasured (its lights are the
  first guess).

## 2. The three levers

1. **Alternate eye rendering** (`Render.Stereo=aer`, exists). Each frame
   renders one eye; the other eye shows its last frame reprojected. Halves
   the per-frame cost of everything in the scene passes, people included.
   Cost: none to try - the setting is there; a tester can switch it in
   OBVR.ini. Risk: the feel of it in the headset (each eye at half the rate;
   fast head turns and fast hands may show it), untested since the dual pass
   became the default. Test: the tester's own session in the house with
   `aer` against `dual`, and the capture with `StartAfterSeconds`.
   **Measured 2026-10-19** (the prison cell with three Blades, harness,
   75 Hz): aer gives the frame 5.4 ms of idle back (8.0 ms against dual's
   2.6 ms - the whole second pass with its overheads) and still dropped
   more frames than dual (0.6 to 0.9 % against 0.0 % with the three
   standing; 12 to 50 % in the empty cell with poses_wait spikes of 21 ms).
   **With the opening's five** (same day): dual at 26.7 ms every frame, aer
   at 13.4 ms - aer is the one lever that holds 75 Hz in that scene today.
   **The empty-cell drops explained and fixed the same evening** (section
   3, "aer's frame pacing"; `[Render] SubmitPastVsync`, default on): the
   empty cell under aer now runs at 13.48 ms with 1.1 % of frames over
   20 ms (was 14 to 50 %), 23 frames dropped by the compositor in 2226
   (was 325 in 2005), the run's reprojection 3.5 % (was 5.6 to 13 %).
   What stays open for aer is the feel in the headset. Under aer the
   capture has no `scene_pass` spans: the scene render hook is installed
   for `dual` only (camera/CameraHook.cpp).
2. **The second pass spared what the first already did.** The engine's
   scene render repeats per eye the work that does not depend on the eye:
   skinning palettes (the same bones, the same pose), culling (OBVR already
   replays the first eye's culling on the second, `Culling sync`), shadow
   and reflection sub-passes. Caching the first eye's palette uploads and
   answering the second eye's from the cache would take the people's share
   off the second pass. Cost: large - the palette upload path is hooked
   (`HookedSetVsConstantF`), but which uploads are eye-independent has to be
   proven per shader class, a wrong cache is the skinning collapse of
   2026-08 all over again (`obvr-skinning-kollaps-geloest`), and the gain is
   at most the second pass's people share (about 4 ms of the 8.6 here).
   Test: the hall scenario's scene_pass per pass with and without.
3. **The engine's cost per person is vanilla.** Oblivion's own CPU cost for
   a visible NPC is not OBVR's to cut; what remains is the player's: fewer
   people in view, a faster CPU, the game's own settings (actor shadows are
   off already in the tested INI). Cost: none; it is the honest answer when
   1 and 2 are not enough. What could still be measured: whether DXVK's
   constant-upload path makes the engine's palette uploads dearer than on
   native D3D9 (the tester plays on Windows, where OBVR recommends DXVK too)
   - a capture on Windows with and without DXVK, medium.

## 3. Open

- Nadi's drop at the opening: reproduced with the five people of the scene
  (section 1), CPU-side, the dual pass 2 ms over the 75 Hz period and every
  frame on the next vsync. What to do about it is the choice between the
  levers: aer holds the scene today (its pacing in cheap scenes open, below,
  its feel untested since the dual pass became the default); lever 2 would
  have to take about 2 ms a frame off the second pass to get this scene
  under the period, out of its 6.6 ms people share - and more at 90 Hz.
- aer's frame pacing: more dropped frames than dual at less CPU per frame
  (2026-10-19; poses_wait spikes of 21 ms in the empty cell, 12 to 14 % of
  frames in three runs, and under 1 % once people fill the frame - the
  spikes come when the frame is cheap, not when it is full).
  **Found and fixed 2026-10-19 evening.** Not gamescope (a desktop run was
  worse: 50 %, every other frame), not the monitor vsync (`MonitorVsync=1`
  was worse too: 28 %). The compositor's own frame timing, read into the
  capture as `compositor.csv` (built for this), showed it: SteamVR wakes
  the game at "running start", 3.0 to 3.3 ms before a vsync; a cheap aer
  frame (1.8 ms of work) was submitted 1 to 3 ms *before* that vsync,
  which the compositor takes as a frame for the vsync just ahead (the
  low-latency path), while a frame submitted after it is shown a vsync
  later (the one-frame pipeline). A frame landing right at the vsync
  flips between the two, and every flip is a mis-presented frame and a
  dropped one (`numMisPresented` 1, `numDroppedFrames` 1, `numFramePresents`
  2 on 325 of 2005 frames). With people in view the submit lands 6 ms
  after the vsync, under dual 0.5 ms after it: both stable. The fix,
  `render/SubmitPacing.h` with `[Render] SubmitPastVsync=1` (default):
  a frame finished before the vsync is held until half a millisecond after
  it - the vsync being the moment WaitGetPoses returned plus the running
  start the compositor reported for the last finished frame. Measured:
  the empty cell under aer at 1.1 % over 20 ms (from 14 to 50 %), 23
  compositor drops in 2226 frames (from 325), the hold 2.6 ms in cheap
  frames and nothing with people (`submit_hold` in the capture). Two
  sources for the vsync were tried and discarded first: the frame record
  in flight (right after WaitGetPoses it is often still the previous
  frame's) and `GetFrameTimeRemaining` (reads a period at the poses
  moment; the compositor's frame runs from running start to running
  start). The lesson for the dual pass: a dual frame finishing early in a
  cheap room is paced the same way now - the 0.4 % long frames of the
  empty cell under dual were the same flip: 0.1 % with the pacing, holds of
  0.5 ms, the three-Blade scene unchanged at 0.0 %, the run's reprojection
  1.7 % (from 3.1 to 5.6 %).
- aer is not instrumented: no `scene_pass` span, the frames' mode `unknown`.
  Giving the single-pass render the same span would need the scene render
  hook installed under aer too (today `dual` only). Cost: small to medium;
  the hook's other work (bone lock, culling sync) must stay off there.
- The tester's own capture of the house scene (`[Performance] Enabled=1`,
  `StartAfterSeconds=90`), to see his numbers rather than this machine's.
  **His first capture read 2026-10-18 evening** (OBVR-performance-1.zip, 60 s,
  2285 frames, no GPU samples): 44 s of a mode the export calls unknown at a
  flat 30 frames a second (the main menu and the load, most likely), 10 s of
  a menu at 85 to 90, and only the last 2.3 s in the world (188 dual-pass
  frames) - the scene he complained about is not in it. Those 188 frames say
  what a frame costs him though: scene_pass 4.14 ms a pass median, 8.6 ms a
  frame for both eyes (this machine's hall with seven people: 4.3 to 5.1 a
  pass), poses_wait and interop_held 1.5 ms mean but 10 ms on every other
  frame, frames 11 to 13 ms apart with every tenth at 21 to 22 - the engine's
  own render of the people in view, twice, over the headset's budget, the
  compositor dropping frames. The lever stands: `Stereo=aer` halves the
  scene cost; and a capture with `StartAfterSeconds` that reaches the scene.
  **His answer the same evening:** no aer for him; the test ran at 90 Hz
  (11.1 ms a frame), which his 11 to 13 ms frames miss by a little - the
  Quest 3 also offers 72 and 80 Hz (Meta's developer table: 72, 80, 90,
  120), and at 72 (13.9 ms) those frames would fit; the places he names are
  the siege of Kvatch and the view at the tower in the first Oblivion gate,
  both far heavier in actors and effects than the house; his machine an RTX
  5070 Ti, a 9800X3D, 32 GB - the GPU is not the suspect, the engine's
  single thread drawing everything twice is. GPU times cannot come from the
  capture under DXVK (`GpuTiming` reads host nanoseconds, docs/performance-
  profiling.md 2026-10-06); SteamVR's advanced frame timing or fpsVR show
  them. Asked of him: a capture at Kvatch with `StartAfterSeconds`, and a
  look at SteamVR's frame timing there to see which side is at the limit.
- The interior's own cost (lever 0, so to speak): why the house takes 7 ms a
  pass with 322 draws where the hall takes 0.7 ms with 322.
- The auto vanity camera circles the player after a while without input, in
  Full VR too (found by these runs; `open-questions-and-known-issues.md`).
  **Built 2026-10-18:** `game/VanityCamera.h` resets the engine's idle clock
  (the float at 0x00B3BB08, read off the camera update at 0x00673303) every
  frame while the headset is there, unless `[Look] AutoVanityCamera=1`; the
  setting the scripts tried to raise, `fAutoVanityModeDelay`, is Skyrim's
  name - Oblivion's is `fVanityModeAutoDelay` (the console line did nothing).
