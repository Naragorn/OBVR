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
this machine. **Confirmed in the headset** (Nadi, 2026-10-19 evening, beta 5:
"performance nun super") **and by a tester** (Discord, 2026-10-20, beta 5: cities with
large groups and encampments out in the world "much much more stable").

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

### The verdict on the levers (2026-10-20, after beta 5 held in the headset and
at the testers)

Derived from the measured costs (0.43 ms a person and pass, 0.72 ms an empty
pass, about 1.7 ms of frame outside the passes), not measured lever by lever:

| lever | people within budget at 90 Hz | cost | what it is |
|---|---:|---|---|
| today (dual, beta 5) | ~9 | | |
| 2: the second pass takes the streams | ~11 | large | bigger rings, a detour with a discard-generation check, a check mode |
| 5a: everything transferable (the ceiling) | ~12 | large to huge | the streams plus the rest; D3D9 submits per eye, nothing more moves |
| 4: the aer fallback by the frame | ~20 | medium | halves everything; in the fallback moments each eye updates every other frame |
| single-pass stereo | ~20 | huge | true stereo every frame, the CPU does the scene once |

- **Lever 2 is not worth it**, alone or as a first step: about 0.12 ms a person
  and frame, two more people at 90 Hz for a large piece of engine work with
  risk, and the ceiling of all transferring is three people. Not built.
- **Single-pass stereo is the real goal** - the only way that halves the
  people's cost and stays stereo - and in OBVR it is not affordable: every
  precompiled vertex shader of the shader packages patched in bytecode (an eye
  index from an instance stream, a second view-projection, the shift into the
  eye's half, the camera-relative bone palettes shifted by the eye delta - what
  the bone lock does on the CPU today), every vertex declaration extended,
  every draw instanced twice, clip planes per eye, shadows, reflections, the
  HDR chain and the 2D pass each on their own. A renderer rebuilt from the
  outside through hooks, against shaders we do not own: weeks to months, each
  shader special case a risk of its own. It belongs to OBNG (or whatever a
  community engine with its own source is called): a renderer written anew
  gets it for free - Vulkan multiview and OpenXR from day one, one scene, two
  views - and that is the main reason such a project would pay. The shape of
  this is derived from the D3D9 model; no engine code was read for it.
- **What stays for OBVR**: lever 4, the aer fallback by the frame (medium),
  and stereo everywhere it fits - which beta 5 made the normal case.

## 3. Open

- **The hall's lost frames (2026-10-23 20:10, found by the fast copy's A/B;
  read in compositor.csv the same evening):** not a submit window. Seven
  people in the harness save's hall lose 9 % (engine copy) to 26 % (fast
  copy) of their frames, and the loss comes in stretches of 228, 231 and 784
  consecutive frames in which every frame is shown twice (num_presents 2,
  frame_index stepping by two, the poses handed at the vsync instead of 3 ms
  before it, the compositor's update at the vsync, vsyncs_to_first_view 2):
  **SteamVR's interleaved reprojection, the half-rate fallback** (Valve:
  "if you get too close to using up the available frame time ... the
  compositor will drop into half-time mode"; "throttled after 4 dropped
  frames out of the past 10 when motion smoothing is disabled"). Each
  stretch starts right after a GPU spike of 14 to 47 ms in
  total_render_gpu_ms (13 spikes in 75 s, the first five as the seven people
  appear), and it lasts 3 to 10 s with frames of 3.5 ms of work. The faster
  frame does not cause it; it shows it longer, and the dungeon (7 to 8 ms of
  work, no spikes) never enters it. Two levers, neither built: (1) the
  spikes - most likely DXVK pipeline compilation or texture upload at a
  person's first draw; `DXVK_HUD=compiler` or a capture with the compositor
  columns against the people's appearance would say; (2) SteamVR's reaction
  - the half-rate mode cannot be switched off in today's settings
  (`allowInterleavedReprojection` is gone from default.vrsettings, Linux
  users report it as not disableable), so what is left is to shorten the
  stay or to make OBVR say it: OBVR reads the frame timing already and could
  log "SteamVR at half rate since N frames" for the tester (cheap). What the
  stay's exit condition is - and whether OBVR's cadence (WaitGetPoses right
  after the submit, 3.5 ms of work) keeps it in - is unmeasured; a run with
  the submit delayed to the running start would tell (small).
  docs/performance-profiling.md "Halle und Spaziergang".
  **Measured 2026-10-23 21:11 (`perf-people-hall-steady.txt`, the hall's
  steady state: the capture from 100 to 160 s, the people in place for over
  a minute):** 0.8 % of the run's frames reprojected (127 of 15185), 19 of
  4447 shown frames mis-presented (0.4 %, in runs of one or two), one GPU
  spike over 12 ms in 60 s (21 ms) and no half-rate stretch - the spikes and
  the stretches belong to the people's first draws, not to the hall. **Built
  the same evening, not yet seen in the game:** `render/HalfRateWatch.h`
  (pure, half_rate_watch test) is fed the compositor's finished frame (index,
  present count) from the frame-timing read HeadsetRenderer makes anyway and
  names the fallback in the log - "Compositor: SteamVR shows every frame
  twice since its frame N - interleaved reprojection, the half-rate fallback"
  after ten consecutive twice-shown frames, "Compositor: back to full rate
  after N frames at half rate" after ten shown once - so a tester's "it
  stutters for seconds" has its name. The scenario that tells first-draw
  costs from per-appearance costs, `perf-jauffre-reappear.txt` (the seven
  people appear, vanish at 100 s and appear again at 118 s inside one
  capture; the spikes and the new log lines at +4 s against +58 s), ran
  2026-10-24 08:45 (PASS) and its capture was lost twice over: the export
  failed because a stray `OBVR-performance-1` stood in the Linux game root
  (Nadi's live session of 21:22 the evening before, the live INI's
  Performance.Enabled=1 then; the runner moves only its own run's folders;
  the folder is now under `artifacts/hand-script/nadi-linux-live/`) and
  Profiler's CreateDirectoryA refuses an existing name; and the capture had
  stopped at 119 s anyway, truncated_capacity - the 32 MiB budget holds
  143395 records, about 60 s at this event rate, so the 90 s asked for never
  fit and the second appearance at 134 s lay outside it. The 10-second
  frame clock trace answers coarsely instead: neither appearance cost a
  frame over 20 ms (the buckets 80-100 s max 14.4 and 19.5 ms, 130-150 s
  max 17.2 and 17.9 ms), and HalfRateWatch logged three episodes (22, 287
  and 166 frames), all during the load and the placing before 50 s, none in
  the 150 s after. Against the hall of 21:11 the whole run is cleaner:
  there every 10 s bucket held 5 to 10 frames of 23-27 ms (a dropped frame
  and its catch-up), 28 frames over 20 ms in the capture's 60 s at gaps of
  102 to 987 frames (not the INI reload's 120), the empty house included;
  here 0 frames of 23 ms or more in the 120 s of people and reappearance.
  What differs is not the antialiasing: both runs report "texture path, 0
  sample(s)" - the Linux Oblivion.ini's iMultiSample=4 of that morning has
  no effect while bDoHighDynamicRange=1 (the engine's effective sample
  count read 0; `game/SceneFrame.h` names HDR as the condition; the Windows
  INI has HDR off, so its iMultiSample=4 counts) - but the room (house
  against hall), the install of 21:20, FoliageAA's passes and a fresh
  system; none of them measured apart. Open, by cost: the profiler picking
  a free folder name (small: a pure name picker with a test, the writer
  trying it), the record budget sized per table instead of maxRecords of
  every table (small: frames and GPU samples are a thirtieth of the
  events, the same 32 MiB would hold about 110 s), the reappear scenario's
  waits shortened so both appearances fit 60 s (cheap). **The first two
  built 2026-10-24 (67d4a47)**: the frame table is an eighth of maxRecords,
  the GPU table a quarter (the minimum without GPU timing), the writer takes
  OBVR-performance-N-2, -3 ... when the name exists; the hall run of 10:52
  started with 262144 events, 32768 frames, 1024 GPU samples and exported
  complete (116744 events in 60 s - the budget now holds about 135 s at that
  rate), so the shorter waits are not needed. **The hall again, 10:52
  (`perf-people-hall-steady.txt`, PASS, same build otherwise):** the steady
  state with the people in place repeats the evening's picture - 5 to 10
  frames of 23-27 ms in every 10 s, 57 frames over 20 ms in the 60 s
  capture (21:11: 28), 41 shown twice (21:11: 19), at gaps of 1 to 435
  frames - and the reappear house of 08:45 had none, so it belongs to the
  hall, not to the day. A long frame is not a GPU spike and not OBVR's
  work: total_render_gpu_ms 6.1-7.6 ms around every one (0 of 57 within two
  frames of a GPU frame over 12 ms), scene_pass 3.35 ms against 3.29 ms,
  and the whole extra 12 ms is inside poses_wait (20.7 ms against 8.4 ms) -
  SteamVR's WaitGetPoses holds the frame one vsync longer. Why the
  compositor does that in the hall and not in the house is not known; that
  the harness's sleeping headset plays a part is a guess, not measured. New
  too: five half-rate episodes (39 to 203 frames) during the load and the
  empty hall before the people came at 64 s, where 21:11 showed none - the
  install of 21:20 lies between the two runs, and so does SteamVR's
  refusal of OBVR's HUD overlay: every harness run since 2026-10-24 07:31
  logs "CreateOverlay('obvr.hud') failed (17)" (KeyInUse, the session
  OBVR's finding), both runs of this morning included, the run of 21:11
  not; from 07:49 the crosshair and all 11 hand HUD overlays were gone too.
  The cause: vrcompositor aborted at 07:29:45, 07:49:31 and 08:07:18
  (ASSERT "Vulkan transfer ring buffer overflow; transfer size 2315040",
  seconds after a harness game connected) and the dead games' overlay keys
  stayed registered until SteamVR was restarted at 19:24 - so the house against the hall compares like with like, today's hall
  against yesterday's does not. For every comparison from
  here on: the session obvr-6f reports the Linux install changed at 21:20 on
  Nadi's request (Plugins.txt with Ragdolls.esp and its 47 replaced
  skeletons, Maskar's Oblivion Overhaul, Enhanced Grabbing, Ascension, My
  Voice Extender; backup `Plugins.txt.bak-2026-10-23-before-ragdolls`), so
  frame times after it do not compare with the runs before; a run that
  compares only within its own capture, as the reappear scenario does, still
  holds.
- **Nadi's Windows sessions, 2026-10-23 evening (11 min, the capture
  OBVR-performance-1 of 87 s) and 2026-10-24 morning (37 min, the frame-clock
  trace alone), read 2026-10-24 morning:** RTX 4090, the Beyond at 90 Hz
  (pacing_vsync 10.97 ms), SteamVR's target 4028x3380 a eye at
  resolutionScale 100, dxvk-gplasync 3.1.1, Oblivion.ini iMultiSample=8,
  FoliageAA supersampling every leaf draw 8 times, SteamVR's
  motionSmoothingOverride 1 for the app. The game ran at 58 fps mean over
  both sessions (209 of 217 ten-second windows between 45 and 60 fps, one at
  90), the engine's frame lengths bimodal 13 and 22 ms, the precise clock's
  mean 17 ms. The compositor's frame timing names the cause: the
  application's GPU time (pre_submit_gpu_ms) 8.3 ms median and 13.8 ms p95 in
  the world, 32 % of the world frames over the 11.1 ms period; the CPU had
  room (scene_pass 1.9 ms median a pass, poses_wait 6.8 ms median,
  submit_hold 6.9 ms). SteamVR throttled the application on 54 % of the
  frames (reprojection flags 0x100: Compositor_FrameTiming's ThrottleMask
  0xF00, one throttled frame; 0x200 two, 21 frames) with motion smoothing
  triggered (0x08) on practically every frame and num_presents 2 on 58 % -
  91 stretches, the longest 181 frames: the half-rate fallback again, here
  with synthesized frames and with a real cause, the GPU over budget
  (**read again 2026-10-26, section 7: the same capture says the hold,
  not the GPU - submit_hold 7.2 ms a frame at a running start of 10.98 ms,
  which is the whole period; and the Windows headset is the Pimax Dream Air,
  not the Beyond**). The
  engine's integer clock steps by 1 ms on Windows with Display Tweaks loaded
  (engine ms 11 and 12 at 90 fps): the 16 ms steps do not show there; bare
  GetTickCount stays unmeasured. During loading the time update runs in a
  tight loop (2.7 to 8.9 million calls in 10 s, the trace's first line of
  every session) - harmless. No crash, no error in OBVR's log; the refused
  shoves, teleports and spell slots are gates working. Open, by cost:
  iMultiSample 8 to 4 and FoliageAA Passes 8 to 4 (cheap, two INI lines, the
  two multipliers of the per-pixel cost; their split is unmeasured, a capture
  with both lowered settles it), SteamVR's resolutionScale below 100 for the
  app (cheap), the Beyond at 75 Hz on Windows as on Linux (cheap, 13.3 ms of
  budget). docs/performance-profiling.md "Nadis Windows-Sessions".

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

## 4. The profile by function, and three levers found in it (2026-10-23)

Nadi's question of 2026-10-23: is Oblivion Display Tweaks (2020) still the
performance mod to use, and could OBVR carry a better one. Display Tweaks is a
timing fix (GetTickCount to QueryPerformanceCounter, fMaxTime and iFPSClamp
per frame, FOV, D3D9Ex - the last withdrawn by its author), not a throughput
mod; Stutter Remover's heap replacement and critical-section hooks crash on
Windows 10; Oblivion Performance Runtime (2026) is process priority and timer
resolution without a measurement. None of them touch what the sampler shows
below. What follows is what was measured and built instead: A, the profile by
engine function; B, the ring manager lever 2 was missing; C, the frame clock.

### A. Where a scene pass with five people goes, by function

`tools/perf-functions.py` maps the render sampler's rows to the engine's
functions (Ghidra's 21240 function starts, exported with REA into
`tools/oblivion-1.2.0.416-functions.tsv.gz`) and names the callers of the
hottest. Scenario `perf-dungeon-five-functions.txt` (the capture laid over
the people alone - the earlier sampled runs hit the capture's record cap a
second into people-15). Run 14:59, dual, 11396 in-pass samples, scene_pass
3.22 ms with the sampler on:

| share of a pass | what | who |
|---:|---|---|
| 24.0 % | the CRT's memcpy (0x009812C0) | 79 % called per vertex from the indexed stream copy 0x0076E610, 19 % from the unlock 0x00776D80 |
| 7.9 % | the stream copy's own loops | from the copier 0x0076F590 |
| 2.5 % | 0x0076F3B0, the copier's memset loop | from 0x0076F590 |
| 13.1 % | DXVK | draws, constants, the unlock |
| 4.6 % | OBVR.dll | SetVsConstantF (bone lock), declarations, draws |
| 3.8 % | 0x0047DA70, plane against sphere | culling, from 0x007D2EA0 |
| 40 % | the rest of the engine's render walk | 0x0077A1F0 on the stack in 49 % of samples, 0x00767520 in 40 % |
| 4 % | ntdll, kernel32 | locks (0x006FF9C0), file reads |

Allocator and critical sections together are under 0.5 % of a pass: a heap
replacement or lock hooks in the Stutter Remover tradition would have found
nothing here. The stream packing - the engine copying every skinned body's
vertices into its dynamic buffers again each pass - is 35 %, and its largest
single piece is a loop that CALLS memcpy once per vertex with 8 or 12 bytes
(0x0076E690: read the partition's index, compute the source, call). The
straight-run path of the same routine has unrolled 8/12/16-byte copies; the
indexed one, the one skinned bodies take, has none.

**Lever A1, built: the fast stream copy** (`render/StreamCopyLogic.h`,
`render/StreamCopy.cpp`, `[Render] FastStreamCopy=1` default). An entry
detour on 0x0076E610 with the same contract (the copier's descriptor in,
the counted total out; zero-fill, indexed and straight paths) and the
element size decided once per call. `stream_copy_test` checks it against
the engine's loops mirrored in StreamCopyReference; `[Debug]
StreamCopyCheck=1` runs the engine's routine first on every copy in the
game and compares: 202000 copies, 0 mismatches
(`perf-dungeon-five-copycheck.txt`, 15:05). It takes the 27 % (memcpy per
vertex plus the loop) down to 10 % (the copy itself, run 15:09).

**Lever A2, found by A1 and set in dxvk.conf: `d3d9.cachedWriteOnlyBuffers
= True`.** Run 15:09 with the fast copy was SLOWER (3.68 ms a pass): the
engine's other memcpy, the one at the unlock that moves the staging buffer
into the mapped D3D buffer (0x00776D8F, `rep movsd`), had gone from 0.15 to
1.2 ms a pass. The engine writes every dynamic buffer through a system-memory
staging copy (the lock wrapper 0x00776C90 hands out `[this+0x40]`, the unlock
copies it into the mapped pointer `[this+0x48]`); DXVK places a WRITEONLY
dynamic buffer in device-local host-visible memory when it can
(D3D9CommonBuffer::CreateBuffer, deepwiki doitsujin/dxvk), and a dword copy
into VRAM over the BAR runs at PCIe speed - 3.7 MB a pass, 1.2 ms. Run 14:59
had the same copy at 0.15 ms, so its buffers must have sat in system memory;
which of the two a run gets varied from start to start (read from the copy
rates, not from a DXVK log - unverified beyond that). With the option the
buffers stay in cached system memory; the GPU reads them over PCIe at the
draw instead. Run 15:19 (`--env`): scene_pass **2.13 ms**; run 15:32 (the
line in dxvk.conf, DXVK confirmed reading it): **2.57 ms**; before the day's
work 3.22. The unlock's function costs 0.15 to 0.2 ms a pass in every run
but the PCIe one (memcpy and DXVK's Unlock trade places), so the option
guards against the 1.2 ms case and does no more; the fast copy is the 20 %.
Why 15:19 lies lower still (fewer samples in the whole render walk) is
run-to-run variance of the scene, not explained. In the live dxvk.conf,
README and Nexus text since 15:25. Cost of both levers together: small (a
detour, a test, a config line); the gain 0.65 to 1.1 ms a pass, 1.3 to
2.2 ms a frame at 75 Hz with the cast placed, more where more people stand.
Headset: not yet. **The roster's "x2" settled (2026-10-23 16:10):** the
actor roster of run 15:32 lists the five base forms once BEFORE the
placement (mark prison: 00023F2E x1, 00023F2A x1, ... the cell's own
copies of the opening's cast, loaded with the cell, the nearest actor 818
units away) and twice after it (the placed copies, the nearest 33 units
away). The bone lock replaced 520 palette rows a scene call in the empty
phase and 2136 with the five placed - the placed five are what is in view,
the cell's own copies stand out of sight. The per-person numbers of this
file, taken as (people minus empty) over five, stand.

**Flat, as far as the harness can tell (16:24 and 16:28, `Stereo=none`, one
world render a frame like a monitor player's, five people):** busy time 5.43 ms
a frame with the engine's copy, 5.06 with the fast one - 0.37 ms a frame, 7 %
of the work; the copy path itself from about 1.2 to 0.7 ms. The true flat
mode (`[Render] Enabled=0`) stops the hand script and the sampler with the
headset path and cannot be run by the harness; the render sampler runs in
every stereo mode since this afternoon. Display Tweaks is loaded in the test
game and ran beside the precise clock all day without a conflict.

What is left in the pass after A1 and A2 (run 15:19): the engine's render
walk (shape traversal, shader and state setup, constants; 57 % "unsorted"),
DXVK 15 %, OBVR 18 % (the copy itself 13 %, the bone lock's constant hook),
culling's plane tests 4 %. The next piece of the stream packing would be the
copier around the copy - 0x0076F590 and 0x0076FE20 computing offsets per
stream - and lever 2's skipping of the whole packing in the second pass.

### B. Lever 2's missing creation site

`HookedCreateVertexBuffer` logs the caller since 2026-10-23: every dynamic
vertex buffer the engine makes (2048 to 120384 bytes) is created by
0x00782A08 inside the ring manager 0x007829A0 (thiscall(ring, bytes, ...),
ret 14h). The ring's capacity (`+0x08`) is the LARGEST SINGLE REQUEST seen,
rounded to 16 plus 32: a bigger request frees the buffer and creates a new
one of that size (usage 0x208, or 0x218); a request that fits behind the
used mark (`+0x14`) locks NOOVERWRITE (0x1000), one that does not resets the
mark and locks DISCARD (0x2000). So lever 2's step (1), rings that hold a
frame's volume, is a detour on this function that raises the capacity before
the creation path; steps (2) to (4) stand as written in section 2. Not
built: with A1 and A2 the packing's share of a pass is smaller than when
lever 2 was costed, and the verdict of 2026-10-20 (two more people at 90 Hz
for a large piece of engine work) has not improved. Measure the pass with
A1 and A2 in the headset first; if people still break the budget, lever 2 is
now a known-size piece of work (large, no unknown left in the ring manager).

### C. The frame clock

The engine's frame length is GetTickCount in whole milliseconds
(`game/FrameClockLogic.h` for the reading of 0x0047D170). Measured under
Proton at 75 Hz with `[Debug] FrameClockTrace=1`: 13.0 ms on 460 of 750
frames, 14.0 on 230, the rest frame drops - the 13.33 ms period quantised,
3.7 % of speed per frame in every animation and every moving thing. The
test game carries Oblivion Display Tweaks with iTickFix=1, so the tick the
engine got all day was ODT's QPC-based replacement in whole milliseconds:
the 13/14 say that ODT's fix does not remove the quantisation and this one
does, beneath it, the two side by side without a conflict. What bare
GetTickCount does - 16 ms steps on Windows without a raised timer
resolution - is unmeasured here; the trace line shows it in a game without
ODT. **Built:** `[Render] PreciseFrameClock=1` (default), an entry detour on
the time update that lets the engine do its integer bookkeeping and then
replaces the frame's seconds with the performance counter's delta, held to
the engine's own 1 to 166 ms and multiplied by its global multiplier; frozen
and iFPSClamp paths left to the engine. With it the given length reads
13.333 ms mean (run 14:59). Cost: cheap, built. Headset: not yet; what to
look for is animation smoothness and NPC walking at 75/90 Hz.

## 5. Lever 2 built: the second pass takes the first pass's streams (2026-10-23 evening)

Nadi's call after section 4: "ok dann hebel 2 mit dem ring-manager". Built as
`render/StreamReuseLogic.h` (pure) and `render/StreamReuse.cpp`, behind
`[Render] StreamReuse=0` (off until the headset has seen it),
`StreamRingBytes=4194304`, `[Debug] StreamReuseCheck`. Two entry detours:

- **The ring manager 0x007829A0** (section 4 B): when a ring's capacity is
  below StreamRingBytes the detour releases its buffer and sets the
  capacity, and the engine's own creation path makes the bigger buffer
  (usage 0x208/0x218 as the engine chose). Three rings grew in every run
  (2048 to 4 MB; one of them the HUD's).
- **The stream packer 0x00763130** (thiscall(renderer, record, geometry,
  a3, desc, custom, force), al=1). First pass: pack, and note the record
  (frame, the buffers its locks touched with their DISCARD generations,
  and a hash of the ELEMENTS each copy wrote - taken in the fast copy,
  render/StreamCopy.cpp, over sixteen elements spread along the run; a
  hash over the written span read the garbage the staging buffer keeps
  between the elements of an interleaved vertex and called every record
  eye-dependent). A pack that locked nothing (a static record the engine
  found packed) leaves no note. Second pass: a record noted this frame
  whose buffers saw no DISCARD since is a candidate; its own history
  decides: verified (packed and compared) three times, then skipped and
  re-verified every 120 frames, and never skipped again after one
  mismatch - that is the eye-dependent array, whatever its class.
  Everything else packs as the engine would.
- The DISCARD generations come from the Lock hooks (vertex and index
  buffers); `CurrentScenePassIndex()` in render/SceneRenderHook.h tells
  the eye. The unit test `stream_reuse_test` covers every flow (73 checks).

**Runs** (the opening's cast, 75 Hz, `perf-dungeon-five-reuse-check.txt`
and `-reuse.txt`; docs/performance-profiling.md "Hebel 2"):

| run | what | second-pass packs skipped | verdict |
|---|---|---:|---|
| 12 (17:00) check, span hashes at unlock | volatile class only | 49 % | 16 % "mismatches" - the staging garbage, not the engine |
| 13 (17:00) live | volatile class only | 79 % | scene_pass 2.38 ms - an upper bound, the "mismatching" records skipped too |
| 14-16 (17:13) per-class verdict | tri shapes only | | every pack is NiTriShapeData; the 1275-vertex head record "mismatches" (span hash) |
| 17-18 (17:26) span-sampled trust | every pack | 0 % | every verification "mismatched" (garbage in the gaps), nothing skipped, 2.75 ms |
| 21 (17:43) check, ELEMENT hashes | every pack | 40 % would be | **98539 packs compared, 0 mismatches**; 24 "eye-dependent" verdicts are static records the second pass packed nothing for (hashes 6/0 - a false label, to fix) |
| 22 (17:47) live, element hashes | every pack | **94 %** (generation 5.5 %) | **scene_pass 2.82 ms - no gain**; the fast copy's samples as many as without the reuse (1078 against 1050), the unlock memcpy too |

**Settled with run 23 (17:58, the copies counted per pass and per path):**
the skip does what it should - the second pass's copies inside the detoured
packer fell from 20.5 GB to 0.04 GB over the run (1.37 MB a frame to
nothing), 92 % of its packs skipped - and the frame did not get cheaper
within what the runs can tell: busy time 7.31 ms a frame against 7.23 to
7.64 in the runs without the reuse (run 22, the same code, read 8.46 - the
runs scatter by half a millisecond). Two reasons, both measured: after the
fast copy (A1) the packer's copies cost about 0.14 ms a frame in the second
pass, not the fifth of a pass they cost when lever 2 was costed; and a
second entry into the packer 0x0076FE20 - 0x00771790 and 0x00773330 out of
the shape render 0x00767B40, not through 0x00763130 - carries another 0.5
to 0.6 MB a pass in 210 small copies that the detour never sees. The
verdict of 2026-10-20 holds on the numbers of today: lever 2 is correct
(98539 compares, 0 mismatches), it skips what it set out to skip, and the
gain is inside the noise. **Default off; the code stays as an opt-in for a
headset look** ([Render] StreamReuse=1): what would be seen there is
nothing, if it works, and a body drawn from the other eye's data if a
record the trust let through is eye-dependent after all. The rings: 4 MB
wraps 0.6 and 0.3 times a frame (two rings); DXVK's slice renaming on a
DISCARD was not measured as a cost (the frame's long-frame share stayed at
0.1 %).

## 6. Leaving a house and crossing cell borders (2026-10-25)

avolpe (beta, 2026-10-24): a stutter when leaving houses, and about every
five minutes outdoors. Two headless runs on the Linux game, 75 Hz, DLL
d40662d6cdfebd44, profiler with GPU timing and the render sampler,
OblivionWalkers' `[Replace] Enabled=0` for both (its frame tick and ESP
still loaded - not a vanilla game), the HUD overlays missing (CreateOverlay
failed with KeyInUse after vrcompositor had restarted itself at 13:50):

- **The house exit** (`perf-cell-border.txt`, 14:05). After the loading
  screen (3.4 s of 100 to 300 ms frames, the load itself) the first world
  frame takes **158.7 ms**; then 13 frames alternate 23 and 16 ms - half of
  their samples in vrclient.dll, SteamVR's pacing finding the beat again -
  and from 0.3 s on the game holds 75 Hz with nothing over 16 ms. OBVR's own
  spans in the 158.7 ms frame are about 18 ms (scene_pass 12.5 ms for both
  eyes, a first draw; 2.9 ms a pass in the steady state).
- **The border walk** (`perf-cell-walk.txt`, 14:14): eight crossings of the
  corner of cell (-13, 21). The first leg: six frames of 24 to 32 ms right
  at the crossing (a third of their samples in ntdll under 0x00893B90,
  0x008DFFB0 and 0x008E4590 - Havok's range, a wait on a lock, read from the
  stacks; that it is the physics attach of the new cells is derived, not
  verified). The second leg: **one frame of 107.8 ms**, 4.9 s into the leg,
  about a second after the player passed x = -53248 (at the harness's
  36 units a second). Legs three to eight, the same border with the cells
  still in memory: nothing over 32 ms, four of the six legs nothing over
  20 ms. A real walk enters new cells every time, so the second leg's frame
  is the one a player meets at every border into fresh ground (derived).
- **Whose the two long frames are** (the sampler, 1 ms period): 77 % of the
  158.7 ms frame's samples and 62 % of the 107.8 ms frame's sit in one loop
  of xOBSE, obse_1_2_416.dll RVA 0x91480 to 0x91495: a walk down a linked
  list comparing a 16-bit index - read in the disassembly. deepwiki
  (llde/xOBSE) places it in `ScriptToken::ReadFrom`
  (obse/obse/ScriptTokens.cpp): a token naming another script's variable
  reads the variable index and looks it up in that script's
  `ScriptEventList` by walking its variable list. 50 of the whole capture's
  58 samples in that loop fall into the one 107.8 ms frame. The engine
  function under it on the stack in 67 % of the frame, 0x005169A0, runs
  one script line, one command at a time (corrected 2026-10-25: it does
  not hold xOBSE's ScriptRunner::Run hook address 0x0051737F, as first
  written here - that lies in the next function, 0x00517330, the run of a
  whole script, which calls 0x005169A0 at 0x0051754D; read in the
  disassembly, the function table has both starts) - so a mod's script,
  run by the engine when new cells or references come
  in, evaluating OBSE expressions over another script's variables tens of
  thousands of times (derived from the loop's share; the count not
  measured). Which script: **unknown**. Maskar's Oblivion Overhaul (in the
  load order on both installs) is the first suspect, unverified.
- **The grass**: the first build after a load costs 20.7 ms (the cow) and
  33.0 ms (the house exit); every rebuild while walking 0.5 to 1.05 ms.
  Whether the 33 ms falls into the loading screen or into the 158.7 ms
  frame, the log cannot tell (its lines carry no tick).

So on this install the hitch on leaving a house and at a border into new
cells is a script's, through xOBSE's variable lookup - not OBVR's, not the
engine's cell attach itself (the attach shows as the 24 to 32 ms frames).
avolpe's own load order decides what avolpe feels; "every five minutes"
is not explained by this (a border into new ground comes every 30 to 60 s
at a running pace, more often on a horse).

Open, in order of cost:
- **A script clock** - built 2026-10-25, see "The script clock" below. It
  named MOO's scripts, so the A/B without MOO first listed here is no
  longer needed.
- **A harness save made with MOO** - made 2026-10-25 19:57
  (`make-harness-save-moo.txt`, `OBVRHarnessMOO.ess`, its co-save 574 KB
  against OBVRHarness's 309 bytes).
  - `script-clock.txt` from it (20:01, Frame Fixes' variable lookup on) had
    no run of `MOOUpdateWorldSpacesAddNewFunctionScript` at all, so MOO's
    map survives a save and a load (measured).
  - The exit's longest frame was 63.2 ms, against 75.7 ms from the old save
    with the fix and 164.0 ms without it. So in a running playthrough the
    house exit costs the load and the first draw, nothing of MOO's.
  - No outpost frame came up on that run's walk.
- **The root cause in xOBSE** (large, outside this repo, an upstream
  change to llde/xOBSE):
  - The cost is a variable lookup by list walk for every token. An index
    or cache in `ScriptEventList::GetVariable`'s use in
    `ScriptToken::ReadFrom` would cut it for every mod.
  - MOO's map build is the workload to measure it on: thousands of
    `MOO.` array reads in one run.
  - How much of the 130 ms the lookup is: measured with the stopgap
    below, about three quarters (127.6 ms without, 30.4 ms with).
- **MOO's side** (not OBVR's): its map build and outpost generation run
  whole inside one frame. Spreading them across frames is the mod's
  author's to decide.

### The script clock (built 2026-10-25)

`[Debug] ScriptTimeTrace=1` (game/ScriptClock.h, game/ScriptClockLogic.h;
default off, `ScriptTimeTraceMs=4` the threshold).

- **What it hooks.** An entry detour on 0x00517330, the run of one whole
  script. It is a thiscall with the Script, the reference, the event list
  and the containing reference, then three bytes and a float, `ret 20h`.
  - Every run comes through it: Script::Execute 0x004FBE00 and the calls
    at 0x004F9F20..0x004F9F82 all reach it via 0x00517810, which takes
    the cached runner at 0x00B36208 or a new one while that one is busy.
  - Read in the disassembly; the callers were found by a call-target scan
    of the whole .text.
  - The detour goes in the first frame the setting is on. Until then the
    run is the engine's, untouched.
- **What it counts.**
  - Each run reads the counter on entry and on exit.
  - Nested runs give each run its own time, without the runs inside it.
    That xOBSE's `Call` reaches the run too is measured: function scripts
    show more time with their nested runs than without.
  - The runs are summed per script, per frame and per ten-second window.
- **What it logs.**
  - A frame line for every frame whose scripts take the threshold or
    longer, at most thirty in a window. It gives the frame's end tick and
    the five scripts with the most own time, each with its form ID, kind,
    runs and longest run, and the reference that run ran on.
  - A window line every ten seconds.
  - The runs off the game's thread are counted, not timed.
- **The engine's own timer.** The engine has one too, unused: 0x005792E0
  gates a QueryPerformanceCounter pair whose seconds go to Script+0x34.
  It gives a sum per script, nothing per frame (read).

**Run of 2026-10-25 17:36** (`script-clock.txt`, PASS, the Linux game, DLL
3837b18769d4d8e5, the profiler and sampler beside it).

The form IDs' first byte, 0x0D, is Maskar's Oblivion Overhaul's place in the
load order. This was derived from the plugins' file times, and the low 24
bits of all 19 IDs looked up match MOO's own SCPT records. The editor IDs
were read from Maskar's Oblivion Overhaul.esp.

- **The house exit.**
  - The first world frame after the load took 160.4 ms in frames.csv; the
    clock's tick lies inside it.
  - 137.0 ms of that frame was scripts, in 362 runs. 127.4 ms of it was
    one run of 0D58EC96 `MOOUpdateWorldSpacesAddNewFunctionScript`.
  - The sampler puts 67.9 % of the same frame in obse_1_2_416.dll.
  - The function's source shows why. Entering a worldspace that MOO has
    no event map for, it builds one: it loops over width x height x
    `ini_event_density` / 100 cells, picks random cells and marks them in
    `MOO.worldspaceevents`. Every `MOO.` variable it touches is a
    variable of another script, which is the lookup of section 6.
  - For a worldspace that already has a map, the function returns after
    one search.
  - The harness save is older than MOO's install (2026-10-23 21:20), so
    every harness run builds Tamriel's map anew.
  - The 18:12 run (below) measured that a second exit in the same session
    no longer runs the build. Whether a saved game keeps the map across a
    save and a load (xOBSE arrays in MOO's quest script) is unverified.
- **The border walk.**
  - Leg one had a frame with 148.4 ms of scripts in 5135 runs of 106
    scripts, all from MOO's dynamic dungeons ("outposts"):
    - `MOOOutpostCheckDirectionAllowedTunnelFunctionScript`: 757 runs, 28.1 ms.
    - `MOOOutpostExecuteTunnelDiggersFunctionScript`: 18.5 ms of its own,
      98.8 ms with what it called.
    - `MOOOutpostCreateStaticsFunctionScript`: 35.4 ms with what it called.
    - `MOOOutpostSetCurrentLocationTunnelFunctionScript`: 251 runs.
  - Legs two and three had no frame over 4 ms.
  - This frame lies outside the capture, so there is no frames.csv
    interval for it.
- **The loads.** A load's long frames name the harness's console
  commands: FF000EF2 (the coc) took 1802.6 ms and FF000EF3 (the moveto)
  2041.8 ms.
  - A console command runs as a script, and the load it starts runs
    inside that run.
  - A run is counted in the frame it ends in. A loading screen presents
    inside the run, so the 2041.8 ms land in a frame that the profiler
    measured at 260 ms.
  - Loading the save named MOO's `MOOGetGameLoaded*FunctionScript`s,
    2.2 s together, inside the loading screen.
- **Between the hitches**, the window lines say 0.10 to 0.16 ms of scripts
  a frame. Most of it is Enhanced Grabbing's quest 0E002A28, 0.02 ms a
  frame, and quests 0E003101 and 0002480B.
- **The table.** Loading the save ran more distinct scripts than the first
  table held (384, with 36213 runs beyond it in that window), so it holds
  3072 now. That is 137 KB a table, two tables, in the DLL's zeroed data.

**Run of 2026-10-25 18:12** (the same scenario with a second house exit
in the same session, PASS, DLL f13ba270c99b4283 on e8a343f, the profiler
over both exits):

- **The first exit.**
  - The first world frame took 168.7 ms: 137.2 ms of scripts, 130.2 ms of
    them in `MOOUpdateWorldSpacesAddNewFunctionScript` again.
  - 0.27 s later came a 177.3 ms frame with 165.6 ms of scripts in 5531
    runs, MOO's outpost functions. This time they ran beside the priory,
    not at the meadow. MOO's event map picks its cells with `Rand`, so the
    outposts land elsewhere in every harness run (derived from the
    script's source).
  - The border walk afterwards had no frame over 4 ms.
- **The second exit** (the house again, then out again): its frame took
  120.8 ms, and 105.0 ms of that was the harness's moveto, the load itself
  with the cells still in memory. Nothing of MOO's was in it.
- **The table:** no line counted runs beyond it.

So on this install both MOO hitches are content MOO generates the first
time a place is seen: the event map of a worldspace, once per session at
least, and an outpost's layout when its cell first loads. A second exit
costs only the load. A door's load does not run inside a script, so with
a door the load's frame would not be counted as script time; that the
door's load frame costs the same 120 ms is unmeasured.

What this leaves for avolpe's report (all unverified until avolpe's log
says otherwise):
- With MOO in avolpe's load order, a stutter on new ground every few
  minutes would fit MOO placing its events.
- A stutter on every house exit would not fit the map build, which runs
  once.
- `[Debug] ScriptTimeTrace=1` in avolpe's OBVR.ini names the scripts on
  avolpe's own install.

### The stopgap: Oblivion Frame Fixes' variable lookup (2026-10-25)

Nadi chose to build it into the standalone plugin first and send the fix to
xOBSE afterwards. It lives in `../OblivionFrameFixes` (`[Main]
FastVariableLookup`, src/VarLookup*.h/.cpp, local commits fa327a9 and
58e8481, no remote). OBVR does not carry it.

- **The patches.**
  - xOBSE 22.13 inlines the walk into `ScriptToken::ReadFrom` at
    obse_1_2_416.dll RVA 0x91479..0x91498, read in the disassembly. Those
    32 bytes become a jump to a stub that asks a cache. The DLL is
    relocatable, so the address is the module base plus the RVA, and the
    68 bytes from the token read to the store after the walk are checked
    first.
  - The engine's 0x004FA080 frees a list's variables. It is the only free
    of variables found, called by the destructor 0x004FB4E0 and the reset
    0x004FBA50; xOBSE's function contexts free their lists through the
    destructor too (read in FunctionScripts.cpp). An entry detour on it
    makes the cache forget the list first.
  - The engine's own one-entry variable cache (0x004FA110), which xOBSE
    switches off at 0x004FA11E, failed on exactly that: new lists at freed
    lists' addresses.
- **The cache.**
  - A list whose walk passes 16 entries gets an index by id after four
    lookups; short lists are walked as before.
  - A changed head drops an index.
  - Everything runs under one lock.
- **Measured** (`script-clock.txt`, the Linux game, OBVR main 6e8a836, the
  plugin with only this fix on):

| | off (19:18) | on (19:14) |
|---|---|---|
| `MOOUpdateWorldSpacesAddNewFunctionScript` at the house exit | 127.6 ms | 30.4 ms |
| that frame's scripts / the frame (frames.csv) | 139.7 / 164.0 ms | 36.1 / 63.2 ms |
| the longest frame at the exit | 164.0 ms | 75.7 ms, the first draw of the exterior: 41 of its 57 samples in the scene passes, none in xOBSE |
| a frame of MOO's outpost generation at the meadow | 117.6 ms of scripts in 3876 runs, 123.4 ms frame | 55.7 ms in 4037 runs, 65.3 ms frame |
| scripts between the hitches, per ten-second window | 0.07 to 0.25 ms a frame | 0.06 to 0.24 ms a frame |

  - The outposts are not the same dungeon in both runs (MOO's `Rand`), but
    they took about as many runs.
  - The check run (19:09, `VariableLookupCheck=1`, every lookup walked as
    well and the walk's answer used) compared 800,001 lookups with 0
    mismatches. 336,891 of them came from an index, 11 indexes were built,
    and one list was forgotten when the engine freed it.
- **What is left of the house exit hitch:**
  - The 30 ms of MOO's own work: a sampler share of 45 % in
    obse_1_2_416.dll and 11 % in the engine's RTTI cast, which xOBSE's
    `EventListFromForm` calls for every token.
  - The first draw.
  - The load itself.
- **Open:**
  - The PR to llde/xOBSE with these numbers (medium: build xOBSE with its
    Visual Studio project, the same index in `ScriptEventList` lookups,
    and invalidation in the destructor and reset).
  - Shipping Frame Fixes is Nadi's call (no remote yet).
  - The plugin is not tested on Windows or in a headset.

## Open, 2026-10-25: Nadi's Windows run of 2026-10-24 evening to read

Nadi: "Performance checken vom Lauf." The run's OBVR.log is saved as
`artifacts/scratch/OBVR-win-20261024-2229.log` (frame clock trace on, as the live INI had it). No capture was made in that run:
the game root has only OBVR-performance-1 of 2026-10-23, and `[Performance] Enabled=1` was
set the next morning. To read: the frame clock's lines and the half-rate episodes
("Compositor: SteamVR shows every frame twice", episode 221 by about line 37000) against the
places played (outdoors with grass since GrassAllAround, the fights).

**Read 2026-10-25** (the session's copy in
`artifacts/nadi-win-live/20261024-215400`): the Windows headset is a Pimax
Dream Air at 90 Hz (vrserver.txt), rendering 4028x3380 a eye (2.1 times
the Beyond's pixels, 2.5 times a second), MSAA 4 in effect (no HDR),
FoliageAA 4 passes. 102517 of about 118000 game frames ran at half rate
(87 %, 251 episodes, the longest 9742 frames); the engine's frame times
bunch at 12 to 15 ms (47 %) and 22 to 23 ms (41 %), 11 ms on 1 % - the
work just over the 11.1 ms period, not a cap: Display Tweaks' FPS fix is
off (`bFPSFix = 0`), Oblivion Performance Runtime only sets priorities and
a 1 ms timer. The session of 06:46 the same (55 fps). GPU or CPU: not
measured; `[Performance] Enabled=1, GpuTiming=1` set in the live INI on
2026-10-25 for Nadi's next Windows session. **Read 2026-10-26: section 7**;
the two keys are back at 0.

## 7. Nadi's Windows run of 2026-10-25 evening: the half rate is OBVR's own submit hold (read 2026-10-26)

The session 20:38 to 21:17 (the Pimax Dream Air at 90 Hz, 4028x3380 a eye,
MSAA 4, FoliageAA 4 passes, Oblivion Frame Fixes with the variable lookup
on), its log and the capture `OBVR-performance-1-2` (90 s from two minutes
after the start, a fight) saved in `artifacts/nadi-win-live/20261025-211700`;
the capture of 2026-10-23 copied to `artifacts/nadi-win-live/20261023-215400`
and read the same way.

**Measured, the whole session** (the frame clock trace, 224 ten-second
windows of play, 128528 frames):
- 115084 frames at half rate in 292 episodes - 90 %. Given mean 17.4 ms a
  frame, 57 fps; the engine's frame lengths 13 to 14 ms on 39 %, 22 to 23 ms
  on 43 %.
- No error, no crash. The refused shoves and the combat ring's refused steps
  are gates working.

**Measured, the capture** (5232 frames; 44 % at 13 ms, 39 % at 22 ms):
- `submit_hold` median 9.29 ms (p10 6.4, p90 9.8), on the 13 ms frames and
  on the 22 ms frames alike. `present` median 10.24 ms, of which the hold
  is 9.3. `poses_wait` 0.23 ms on the 13 ms frames, 9.43 ms on the 22 ms
  frames (SteamVR holding the game back). `scene_pass` 1.07 ms a pass.
- Frame length minus hold minus poses_wait, the frame's own work: median
  3.76 ms, p90 6.4.
- SteamVR's compositor timing: `newPosesReadyMs` -10.97 ms on every one of
  the 5234 frames. On the Beyond at 75 Hz under Linux it is -2.99 ms (the
  run of 20:01 the same day). `pacing_vsync` 10.97 ms - the running start
  OBVR used.
- `numFramePresents` 2 on 55 % of the frames, in runs of 24 (SteamVR's
  motion smoothing throttles in blocks); `numMisPresented` on 77 %; the
  throttle flag 0x100 on 49 %.
- `preSubmitGpuMs`: 9.86 ms median on the full-rate frames, 3.99 ms on the
  half-rate frames of the same fight. `totalRenderGpuMs` over 11.1 ms on
  37 % of the frames.
- The capture of 2026-10-23 (MSAA 8, FoliageAA 8 then): `newPosesReadyMs`
  -10.98 ms on every frame, `submit_hold` 7.2 ms on the 13 ms frames, the
  frame's own work 5.7 ms median, `preSubmitGpuMs` 9.67 ms on full-rate
  frames and 5.42 ms on half-rate frames.
- OBVR's own GPU timing (`GpuTiming=1`, D3D9 timestamp queries): 0.001 ms
  a scene pass at a frequency of 1e9 on the native D3D9 driver - unusable
  there (DXVK on Linux gives real numbers). The compositor's timing is the
  GPU source on Windows.
- Oblivion Frame Fixes on Windows: the variable lookup indexed, 2.0 million
  lookups in the session, 11 indexes (one of 3533 ids, MOO's quest script),
  check mode off.

**Derived:**
- The Pimax's compositor hands the poses out right after a vsync, a whole
  period before the next one. `render/SubmitPacing.h` took the 10.97 ms
  for a running start (its cap was 12 ms), put the vsync 10.97 ms after
  the poses and held every frame until half a millisecond past it. A frame
  of 0.23 + 3.76 + 9.3 ms is 13.3 ms long, more than the 11.11 ms period, so
  every frame landed late and SteamVR ran the game at half rate. The hold
  is the frame's length; the frame's own work would fit the period three
  times over.
- The `preSubmitGpuMs` of 8 to 10 ms, read on 2026-10-24 as the GPU over
  budget, is not a clean GPU cost: the same scenes take 4 to 5.4 ms of it
  on the half-rate frames. What stretches it on the full-rate frames is
  unverified (the compositor's own GPU work in the same window is the
  candidate). So whether the GPU alone would hold 90 Hz at MSAA 4 is open;
  it needs between 4 and 10 ms of the 11.1.

**Built 2026-10-26** (`render/SubmitPacing.h`, submit_pacing_test): a
running start counts only up to 7.5 ms (below any headset's period from
72 Hz up; SteamVR's is 3 ms, 7 ms was seen under load on the Beyond), and
a hold is at most 4 ms (the running start window plus the margin - a frame
early by more than that has nothing to flip for, and a hold would only add
latency, or a period when the estimate is wrong). On the Pimax nothing is
held now, and the pacing log line says why. The Beyond's 3 ms running start
and its 2.6 to 3.4 ms holds are unchanged. Deployed to both installs as
6a45758d3eeff56e.

**Open:**
- The next Windows session decides it: the half-rate episodes
  ("Compositor: SteamVR shows every frame twice") should become rare and
  the frame clock's engine lengths 11 ms. If the frames stay over 11.1 ms
  with the hold gone, the GPU is the limit after all, and the levers are
  MSAA 4 to 2, FoliageAA's passes, SteamVR's resolution scale for the app
  (all cheap, one INI line each).
- OBVR knows no headset refresh rate; the physics step takes 90 Hz from
  `[Hands] PhysicsRate`. Reading `Prop_DisplayFrequency_Float` would make the
  pacing's limits period-relative and the physics step automatic (small to
  medium: the FnTable slot to verify against openvr.h, a unit on the
  derived limits, a headset check).
- OBVR's D3D9 timestamp timing on the native driver (medium: read how the
  driver answers TIMESTAMP and TIMESTAMPFREQ queries; low priority while
  the compositor's timing exists).

## 8. The Gold Road under beta 5, beta 6 and main: no build regression above the run-to-run noise (2026-10-26)

avolpe (beta 6, 2026-10-23): "I feel like the performance may have regressed
a bit from the previous beta walking down the same area used to run at a
solid 90 fps but now looking in random directions drop the frame rate
seemingly randomly. Virtual desktop just brought back 144 so playing with
144 with ssw on feels great for now but might be something to look into.
Could also be an isolated incident". The area: the walk from Kvatch to
Skingrad. Nadi, 2026-10-26: replay it in the harness, beta 5 against beta 6.

**The scenario** (`tools/hand-scripts/perf-gold-road.txt`, docs/hand-script-harness.md):
the save OBVRHarnessMOO, three spots on the Gold Road by `cow Tamriel`
(cell -33 -4 on Kvatch's east slope, -27 -3 midway, -21 -1 before
Skingrad's pastures), at each the body turned east, the head through eight
yaws (0, -45, -90, -135, 180, 135, 90, 45) for two seconds apiece, then an
eight-second walk east; the profiler with the compositor's timing, read
per view with `tools/perf-phases.py`. The headless Linux harness: the Beyond
at 75 Hz, the RTX 4090, the dual pass, the live INI (WaterReflectionMode=2,
beta 5's default, for all three builds; main's other defaults as they
are), night with rain in every run (the save's state). The builds, all
clang-cl from git worktrees: beta 5 = eaf578c (4c9d8cd408cb9b75), beta 6 =
070ca9a (77b73df32c496f7d), main = a2d192f (242170f357da76a9). The betas'
event table holds 143395 events, 68 s, so their first capture ends inside
spot 2; `perf-gold-road-late.txt` (the capture from 95 s) covers spots 2
and 3. Runs: `artifacts/hand-script/perf-gold-road/20261026-075222` (beta
5), `-075724` (beta 6), `-080126` (main); `perf-gold-road-late/20261026-080524`
(beta 5), `-080927` (beta 6); with the render sampler
`perf-gold-road-sampler/20261026-081323` (main), `-081717` (beta 6).

**Measured.** The frame's median is 13.33 ms in every view of every run:
75 Hz held throughout, 0 to 2 frames over 20 ms per 170-frame view, the
loads aside, and no half-rate episode in main's watch. The cost per view is
in the compositor's pre-submit GPU ms (wall time on the GPU timeline,
section 7) and OBVR's scene_pass ms (both eyes, CPU):

| Spot 1, Kvatch's east slope | GPU ms by view | scene_pass ms by view |
|---|---|---|
| beta 5 | 3.9 to 6.2 (east the heaviest, west the lightest) | 1.2 to 2.2 |
| beta 6 | 4.0 to 6.2, within 0.3 of beta 5 in all eight views | 1.1 to 2.1 |
| main, first run | 4.9 to 7.0: +0.9 to +1.1 in the west and north views, equal in the east | 1.6 to 2.5 |
| beta 6 again, with the sampler | 4.1 to 7.1 | 1.3 to 2.6 |
| main again, with the sampler | 4.6 to 7.3, within 0.3 of beta 6's sampler run | 1.5 to 2.6 |

- Beta 6's own two runs differ by up to 1 ms of scene_pass in the same
  view (facing west: 2.54 then 1.59 ms) with the same picture at the mark.
- Spots 2 and 3: beta 6 against beta 5 within the noise, mostly 0.2 to
  0.6 ms under it; main against the betas both ways. Facing east at spot 3
  (Skingrad's pastures and vineyards): 7.3 to 8.5 ms of GPU wall time, with
  people on the road (three actors fitted in main's first run near spot 3,
  one in its sampler run, none in beta 6's).
- OBVR's D3D9 timestamp timing returned zero spans on DXVK this time
  (gpu_samples.csv start and end equal), although it worked on 2026-10-23;
  the compositor's number stands in. Why is unverified.
- The render sampler on main, facing west at spot 1: OBVR's own code is
  0.2 to 0.3 ms of the passes a frame (HookedSetVsConstantF 0.12,
  StreamCopyFast 0.08 at the east view), the rest the engine's and DXVK's.

**Derived.**

- No regression from beta 5 to beta 6 is measurable here: the two builds
  differ by less than two runs of beta 6 do. The same holds for main.
- The run-to-run noise is up to 1 ms of scene_pass and of GPU wall time
  for one build in one view. What moves between runs - the road's people
  and creatures (MOO's, the Walkers'), the rain's density - is the suspect;
  which, unverified.
- What avolpe felt is not reproduced on this hardware: at 75 Hz the frame
  never left 13.33 ms. What the runs do show is the budget: the east views
  cost 6 to 8.5 ms of GPU wall time for two eyes on a 4090 at the harness's
  resolution. At 90 Hz (11.1 ms) over Virtual Desktop with a smaller GPU the
  same views can miss the vsync in some directions and not in others -
  "looking in random directions". That is the scene's cost against the
  budget, not a change between the betas (avolpe's GPU and Virtual Desktop's
  resolution unverified).
- The submit hold is no beta 6 difference: SubmitPastVsync went in with
  6284621, before beta 5's eaf578c. But Virtual Desktop's running start is
  unknown; if its compositor reports the poses a period early as the
  Pimax's does (section 7), beta 5 and beta 6 both held every frame, and
  the caps of a2d192f end that. avolpe's OBVR.log says which ("Submit
  pacing:" and, since a2d192f, "not a running start ... nothing is held").

**Open.**

- avolpe's OBVR.log with the pacing line, and the GPU model; the fixed
  build (a2d192f or later) on avolpe's setup.
- The noise's source: a line at each mark with the actors in view and the
  weather's state (small: the harness's mark hook, one log line).
- The D3D9 timestamp timing on DXVK: why the spans are zero now (small to
  medium: compare the device and the query path with the 2026-10-23 runs).
