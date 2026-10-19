# Performance spec: people in view

Status: measured 2026-10-18 and 2026-10-19; **the cost that was measured
turned out to be mostly OBVR's own** (section 0, 2026-10-19 evening), and
with it gone the opening's five people fit 75 Hz under the dual pass.
The measurements
and the harness scenarios are in `docs/performance-profiling.md` ("Gemessen
2026-10-18" and "Gemessen 2026-10-19"); the tester's report (a Quest 3 at
120 Hz: "stuttering like crazy" with two NPCs talking in Jauffre's house,
smooth in dialogue) in `docs/controls-spec.md` "Beta reports of 2026-10-17";
Nadi's own (2026-10-19, the Beyond at 75 Hz): the drop in the opening
dungeon "sobald die Truppe aus 3 Leuten vor mir steht".

## 0. What the people cost was (2026-10-19 evening)

The render sampler (`[Performance] Sampler=1`, render/RenderSampler.h,
built for this) put 40 % of a scene pass with the opening's five people
inside OBVR's `HookedVbUnlock`: the dual-pass diagnostic that fingerprints
every dynamic-buffer write (`SumLeadingBytes`, the "Dual pool writes" log
line, the pool timeline's vertex peeks) read the first 256 bytes of each
written range back out of the mapped buffer - write-combined memory, byte
by byte, 25 us an unlock, and the engine unlocks 135 dynamic buffers a pass
with five people in view (6 with nobody): 3.6 ms a pass, 7 ms a frame.
The code's comment had called it "a cost no frame can feel". A second
45 % of the sampled pass was the call clock built the same afternoon:
two counter reads per hooked D3D9 call, twenty thousand a pass, each
ntdll plus the vDSO under Wine. Both are opt-in now
(`[Debug] PoolWriteFingerprints`, `[Performance] CallClock`).

Without them, the opening's five people under the dual pass, headless
75 Hz (perf-dungeon-five.txt, 2026-10-19 16:44):

| | before | after |
|---|---:|---:|
| frame | 26.7 ms, 100 % over 20 ms | 13.33 ms, 0.0 % |
| scene_pass per eye | 7.6 to 8.9 ms | 2.85 ms |
| idle before the compositor | 9 ms (the wait for the next vsync) | 5.9 ms (room) |
| SteamVR reprojected over the run | 31 to 32 % | 2.1 % |

The engine's own cost of a person is about 0.43 ms a pass (2.85 ms for
five against 0.72 empty), not the 1.1 to 1.3 measured in the morning. Of
a pass with five people the sampler now shows 74 % engine code (a fifth of
it the memcpy of skinned vertices into the dynamic buffers, 3.7 MB a
pass), 14 % DXVK, 4 % OBVR. At 90 Hz five people leave about 3 ms, at
120 Hz about 1 ms. Nadi's drop in the opening is explained and gone on
this machine; the headset confirms it or not.

Lessons, for the knowledge base: never read mapped buffer memory back in
a hot path; a counter read under Wine is not free; and a per-pass share
that "cannot be felt" is measured, not asserted - the sampler is the tool.

## 1. What is known (as measured with the instruments still on - the
shares hold, the absolute numbers are about 2.5 times the engine's)

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
- **Nadi's drop reproduced** (2026-10-19 afternoon, with the fingerprints still on; see section 0 for what it was - the same cell with the opening's
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
  first guess). **Corrected 2026-10-19 evening:** it was the fingerprints
  too - the "empty" house has 95 skinned draws (its own people behind the
  player) and as many unlocks a pass; without the instruments the house
  runs at 13.35 ms with 0.1 % over 20 ms, scene_pass 3.5 ms a pass
  (perf-jauffre.txt, 16:53).

## 2. The levers

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
2. **The second pass spared what the first already did.** Measured
   2026-10-19 evening with the call clock (`draw_calls`, `vs_constant_calls`,
   `state_calls`; docs/performance-profiling.md "Nachtrag: die Aufruf-Uhr"):
   of a pass's 8.9 ms with the opening's five, 1.9 ms are inside the D3D9
   calls OBVR hooks (draws 0.25, vertex constants 1.1, other setters 0.4,
   bone lock 0.2) and 6.9 ms are the engine between the calls - scene
   graph, the bone palettes computed per skinned shape, visibility - about
   1.2 ms a person and pass. So a palette-upload cache would take at most
   1.1 ms off the second pass; the lever is the engine's eye-independent
   computation, above all the palette build, which needs its engine
   function found and timed first (small to medium), then the second pass
   taking the first's result (large). **Evening:** the call clock's own
   counter reads were most of that "engine between the calls" (section
   0); without instruments a pass with five people is 2.85 ms, and the
   second pass's eye-independent work that is left to spare is the 3.7 MB
   of skinned vertices the engine streams into its dynamic buffers again
   (a fifth of the pass) - worth it only once a scene is over budget
   again. The original idea, for the record: the engine's
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
   **Read in the engine, 2026-10-19 evening** (objdump over Oblivion.exe,
   the sampler's stacks): what the second pass repeats for a person is the
   engine's stream packing. `0x00763130` (thiscall, six arguments; its
   object carries the stream and index managers at +0x8AC and +0x8B4 -
   the renderer, most likely) packs a geometry's vertex streams into the
   geometry's render record (`+0x24` the stream slots, each with the
   buffer at `+0x8` and the offset at `+0xC`) through the packer
   `0x0076FE20`: the lock wrapper `0x00776C90` hands the engine a
   system-memory staging buffer, the unlock `0x00776D80` memcpys it into
   the mapped buffer (the `0x00776DC2` of section 0). For geometry
   flagged static (`data+0x2E & 0xF000 == 0x4000`) with a packed record it
   returns 1 at once and touches nothing; for volatile geometry (`0x8000`:
   skinned bodies, morphing faces) it repacks on every call. The record
   persists and the draw reads it. The packing is 26 to 30 % of a pass with
   the opening's five (the copier loops under `0x0076F590`, the two
   memcpys). But the pool-timeline dumps of August show the engine's
   dynamic rings are small - about 96 KB, `DISCARD` at every wrap, 45 of
   135 locks a pass - so by the second pass the first pass's bytes are
   long overwritten: as the rings stand, the second pass cannot take the
   first's streams. The design that would work: (1) the rings enlarged to
   hold a frame's volume (3.7 MB a pass, twice) - their creation site is
   not found yet; (2) a detour on `0x00763130` that in the second pass
   returns 1 for a record packed in the first pass whose buffers saw no
   `DISCARD` since (a per-buffer discard generation counted in
   `HookedVbLock`), else packs as usual; (3) an opt-in check mode that
   packs in the second pass anyway and compares the bytes with the first
   pass's, per geometry class, before the skip is trusted (the August
   fingerprints matched in every sampled scene call, 7 to 48 writes, but
   only their leading 256 bytes); (4) a kill switch. Cost: large (engine
   unknowns in the ring manager, a device-reset path for bigger rings, and
   the harness verification wants a second-eye image the Linux harness
   does not have today); gain about 0.8 ms a frame with five people at
   75 Hz (the second pass's packing), more where more people stand.
   Decision 2026-10-19 evening: beta 5 packaged first with the fingerprint
   fix (docs/discord-hand-tracking-beta.md); the lever waits for Nadi's
   call and for the 90 Hz tester's capture with the fixed build.
   Test: the hall scenario's scene_pass per pass with and without.
3. **The engine's cost per person is vanilla.** Oblivion's own CPU cost for
   a visible NPC is not OBVR's to cut; what remains is the player's: fewer
   people in view, a faster CPU, the game's own settings (actor shadows are
   off already in the tested INI). Cost: none; it is the honest answer when
   1 and 2 are not enough. What could still be measured: whether DXVK's
   constant-upload path makes the engine's palette uploads dearer than on
   native D3D9 (the tester plays on Windows, where OBVR recommends DXVK too)
   - a capture on Windows with and without DXVK, medium.
4. **The automatic aer fallback, per frame** (proposed 2026-10-19 evening,
   Nadi: "ja das kann so in die spec"). Where the people are, there is no
   lever on OBVR's side that keeps the dual pass under budget: the limits,
   linearly from the measured 0.43 ms a person and pass (two passes, the
   empty dungeon pass 0.72 ms, about 1.7 ms of frame outside the passes;
   heavier NPCs than the Blades may cost more), are about 11 people at 75 Hz,
   9 at 90, 6 at 120 in the dungeon, and 7 / 5 / 1 to 2 in Jauffre's house.
   The stream lever (2) lifts them by about 15 %; aer halves the people's
   cost a frame and lifts them to about 20 at 90 Hz. Since aer is a fallback
   by Nadi's word, the fitting form is a fallback by the frame: dual while
   the frame fits the headset's period, aer as soon as it does not, with a
   hysteresis (a few frames over budget to switch, many under it to switch
   back) so it does not flicker, and a log line and an HUD status word so a
   tester knows which mode he is in. Cost: medium. Both paths exist, but the
   scene render hook is installed for `dual` only (camera/CameraHook.cpp) and
   the alternation state lives in the aer path - both would have to be
   installable and switchable at runtime; the budget comes from the frame
   time OBVR already measures for the capture. Risk: whether the switch
   itself is felt in the headset (one eye's rate halves) - unmeasurable, only
   feelable. Test: the five-people scenario with a forced budget that makes
   the switch happen, counts of switches, and the capture's long-frame share
   before and after.
5. **Compute once, draw for both eyes - the three gradations** (Nadi's
   question of 2026-10-19 evening: "die Berechnungen nur pro Auge zu machen
   und dann zu übertragen - eig die beste Lösung?"). What a pass does for a
   person splits into work that does not depend on the eye and work that is
   the drawing itself:
   - *Transfer the eye-independent results* (lever 2 generalised). Done
     already: the culling (`Culling sync` replays the first eye's visible
     set) and the bone rows (the bone lock rebases the first pass's rows by
     the eye delta - the engine still computes them, OBVR only replaces the
     upload; the matrices are read precomputed at `skin+0x20`, so the
     per-pass cost is the upload, not the build). Not done: the stream
     packing (26 to 30 % of a pass, blocked by the 96 KB rings, lever 2).
     What is left after that - shape traversal, shader and state setup, the
     constant map, the draw calls (the engine's 46 % and DXVK's 14 % of a
     pass) - is submission, which D3D9 needs per eye. Ceiling of this
     gradation: roughly a third of the second pass, a sixth of the people's
     cost a frame. Measured shares; the ceiling is derived.
   - *Single-pass stereo* (compute and submit once, the GPU draws both
     eyes): instancing with an eye index per instance, every vertex shader
     of the shader packages patched in bytecode to pick the eye's matrix and
     shift to its half of a shared target, every vertex declaration and
     every draw given an instance stream, a clip plane per eye, and every
     special pass (shadows, reflections, the HDR chain, the 2D pass)
     handled on its own. This is the best solution in principle and the one
     modern engines take; in Oblivion's D3D9 renderer it is a rewrite of
     the render path: huge, a project of its own (the OBNG idea), not a
     lever for OBVR. Unverified beyond the shape of it: no engine code read.
   - *aer*: compute once a frame, show the other eye's last render
     reprojected. The only gradation that halves everything, at the price
     of each eye updating every other frame. Lever 4 makes it a fallback by
     the frame.

## 3. Open

- Nadi's drop at the opening: reproduced in the afternoon, explained and
  gone in the evening (section 0) - it was OBVR's own fingerprint reads.
  Open: the headset's word on it, and the testers' (avolpe's Kvatch at
  90 Hz on a Quest 3 has a different budget; the per-person cost of
  0.43 ms a pass gives five people about 3 ms of room there).
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
  pass with 322 draws where the hall takes 0.7 ms with 322. **Closed
  2026-10-19 evening:** the fingerprint reads on the house's 95 skinned
  draws; 3.5 ms a pass without them, the frame at 13.35 ms.
- The auto vanity camera circles the player after a while without input, in
  Full VR too (found by these runs; `open-questions-and-known-issues.md`).
  **Built 2026-10-18:** `game/VanityCamera.h` resets the engine's idle clock
  (the float at 0x00B3BB08, read off the camera update at 0x00673303) every
  frame while the headset is there, unless `[Look] AutoVanityCamera=1`; the
  setting the scripts tried to raise, `fAutoVanityModeDelay`, is Skyrim's
  name - Oblivion's is `fVanityModeAutoDelay` (the console line did nothing).
