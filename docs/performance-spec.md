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

- **Built 2026-10-26: the actor shadow maps drawn once for both eyes**
  (Nadi: "Schatten zwischen den Augen teilen wie FNVVR"; `[Render]
  ShareActorShadows`, default off until the headset has seen it;
  `render/ShadowShare.h`, `render/ShadowShareHook.cpp`). Oblivion draws its
  actor shadows in 0x004073D0, called once from the scene render, so the
  dual pass drew every caster's map twice a frame; a shadow map is the
  caster seen from its light, the same for both eyes, so the second eye now
  skips the call when the first eye ran it in the same world render.
  Measured (OblivionWalkers `perf-walkers-in-view.txt` against
  `perf-walkers-shadows-shared.txt`, the same build - main 2416588 plus the
  hook - eighteen Walkers standing, `iActorShadowCountInt=10` by hand, Linux
  75 Hz): scene_pass 3.85 / 4.68 ms (mean / p95) against 3.35 / 4.52 ms,
  frames 14.59 against 13.48 ms mean, over 20 ms 6.7 % against 1.1 %, SteamVR
  reprojected 19.0 % against 3.4 %; the bone lock's off-screen rows (the
  shadow sub-passes) 1926 against 1070 a log window; the log's "drawn 600,
  kept 600" every 600 renders. The two eyes' dumps (`Debug.EyeDump`,
  `perf-walkers-shadows-shared-eyes.txt`) show the same shadows. Ten
  casters cost 1.4 ms a pass on the older build (15:02 against 15:06), the
  share takes about 1 ms a frame off. Open: the headset look; Nadi's Windows
  INI has `iActorShadowCountInt=10` and `iActorShadowCountExt=10`.

- **For later (Nadi, 2026-10-26: "notiere nur kurz in spec für später"):
  true single-pass stereo** - the CPU renders the scene once, the GPU draws
  both eyes (section 2, gradation 5, and the verdict table: about 20 people
  at 90 Hz, huge, a rewrite of the render path). Asked about after the
  Walkers' capture (OblivionWalkers `perf-walkers-in-view.txt`, 2026-10-26:
  eighteen walkers in view cost 3.3 ms a scene pass); the stream reuse
  (section 5) moves nothing there. Not planned now.

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
    obse_1_2_416.dll and 11 % in the engine's RTTI cast. That the cast came
    from xOBSE's `EventListFromForm` was read off its callers' code, not off
    the samples, and it was wrong (2026-10-26 below).
  - The first draw.
  - The load itself.
- **2026-10-26: the cast cache measured - no gain above the noise.** Frame
  Fixes' FastEventListCast (OblivionFrameFixes bf23d74) answers the three
  casts of xOBSE 22.13's inlined `EventListFromForm` (RVA 0x913DA, 0x91406,
  0x91448) from a cache of each class's answer. `script-clock.txt`, the
  Linux game, OBVR main e4dc540, FastVariableLookup on in all three:
  - Check run (20:19, every cast asked of the game too): 400,001 casts at
    the three sites, 0 mismatches, two classes - MOO's quest forms, the
    cast to TESObjectREFR failing and the one to TESQuest succeeding.
  - On (20:24) against off (20:28): MOO's house exit script 30.1 against
    31.1 ms, the coc frames 58.8 / 59.5 ms, 113.0 / 110.1 ms and 17.6 /
    19.1 ms - inside the runs' spread. The cast's samples over the whole
    capture 33 against 47 of about 88,000 (0.04 against 0.05 %).
  - So the casts at those sites cost about a millisecond a hitch at most.
    The cast samples of the MOO frames come from other callers - their
    stack words held no xOBSE return address within reach (the cast's own
    frames and the engine's command dispatch at 0x004FADD5, which calls
    every command's handler) - most likely xOBSE's commands' own argument
    casts (523 call sites of 0x009832E6 in obse_1_2_416.dll). Not deployed;
    the plugin on both installs stays 5e7f054bc7b788ba.
  - Read on the way: in the MOO outpost frame of 2026-10-25 19:14 (65 ms),
    14 of its 57 samples sat in the engine's own variable lookup 0x004FA110
    (called from 0x004FAE1F, the command path), which FastVariableLookup
    does not cover - the engine's walk, not xOBSE's. The outposts are
    random per run; today's three runs had no such frame inside the
    capture (54 to 71 samples of it over 88,000).
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

**Result, 2026-10-26 midday - Nadi's Windows run with the fix** (OBVR.dll
`7422128f995133c2` = f352316; the log and the capture `OBVR-performance-1`
with the sampler's `OBVR-samples.csv` in `artifacts/nadi-win-live/20261026-125539`;
the session ended in a crash at 12:55 that another session reads):
- The whole session, about seven minutes and 39700 compositor frames: 974
  frames at half rate in 11 episodes - 2.5 %, against 90 % the evening
  before. The frame clock's engine lengths 11 ms, 10 to 12 ms on 95 to 99 %
  of the frames of every full-rate window.
- The capture, 90 s from two minutes after the start (the cells 00005E3F
  and 00005E40, Tamriel 13,20 and 14,20, outdoors with grass, no fight):
  8050 frames in 90.0 s =
  89.5 fps, interval median 11.13 ms, p99 12.71, 10 to 12 ms on 97 %.
  `numFramePresents` 1 on 99.5 %, nothing dropped, 94 mis-presentations
  (1.2 %), the throttle flag 0x100 on 24 frames (0.3 %), flags 0xc (async
  and motion smoothing on, no extra prediction, no throttle) on 99.3 %.
- `preSubmitGpuMs` on the full-rate frames: median 3.84 ms, p90 4.46, p99
  5.11, one frame over 11.1; `totalRenderGpuMs` median 4.72, p90 5.45;
  the compositor's own render 0.86. So the 9.86 ms of the first reading was
  the hold's stretch, as suspected: the GPU takes about 4 of the 11.1 ms at
  4028x3380, MSAA 4 and FoliageAA's 4 passes on the 4090. It is not the
  limit, and the levers (MSAA, FoliageAA passes, resolution scale) stay
  unpulled.
- CPU: `poses_wait` 4.76 ms a frame (idle in WaitGetPoses), `scene_pass`
  1.71 ms a pass, `present` 0.81, `submit_hold` 0.06 ms a frame (133 holds
  of 3.63 ms in 8050 frames; 8.4 ms a frame the evening before). The render
  sampler: inside the passes Oblivion.exe 54 %, D3D9.DLL 21 %, OBVR.dll 7 %,
  KERNEL32 6 %, ntdll 6 %, FoliageAA 3 %; outside them ntdll 69 % (the
  wait), Oblivion.exe 23 %, OBVR.dll 2 %. About 5 ms of CPU and 7 ms of GPU
  to spare in that scene.
- `newPosesReadyMs` is no longer the constant -10.97 of the half-rate
  state: -11.0 to -6.5 (median -9.34, p90 -7.60). Every one of the 133 holds
  followed a frame whose poses came 6.5 to 7.5 ms before its vsync, under
  the 7.5 ms cap. Two of them were followed by a doubled or mis-presented
  frame, and of the 66 doubled or mis-presented frames one had a hold near
  it: the remaining holds cost no frames, the cap stays. The 66 are spikes -
  the doubled frames' `preSubmitGpuMs` p90 is 14.2 ms, max 81 (a load), 10
  of 43 over 11.1.
- `torch_scan` (the torch cell walk every 30 frames) is in the capture:
  0.000 ms a frame.
- The last 13 seconds before the crash, after a load into cell 000CA58F
  (Fort Urasek Company Billets, an interior) with 18 Walkers in sight (the
  Walkers' spawn replacements; the exception lines name them): the frame
  clock 22 to 23 ms on most frames, given mean 18.8 and 19.4 ms in
  its two windows, half rate in 9 episodes of 20 to 288 frames - 931 of the
  session's 974. Whether that is the people's CPU cost (§0: about 0.3 ms a
  pass each on Linux, which is 11 ms a frame for 18) or the GPU is
  unmeasured; the capture was over.

**Measured and built, 2026-10-26 afternoon - what a crowd costs, and the
bone lock's share of it** (Nadi: "go 1", then "Go"):
- The Walkers session's Linux harness in Fort Urasek itself (coc
  FortUrasek02, 100 s standing, no capture): 47 Walkers on the lists and 46
  in high process held 75 Hz with 3.7 % reprojected - but only 11 of them
  came near enough for OBVR's exception lines, so that is the AI's cost
  without the drawing. OBVR's own fort attempt (14:23, the spawn
  replacement on) lost its capture to a script shorter than its capture
  window, and its frame clock stood at 35 ms before the eighteen were even
  placed - the fort's own replaced spawns closing in; the artifacts are
  under `artifacts/hand-script/perf-fort-urasek-walkers/`, the scenario
  gave way to the prison twins below.
- The Walkers session's `perf-walkers-in-view.txt` (kept here verbatim plus
  the sampler as `tools/hand-scripts/perf-walkers-standing.txt`):
  ImperialDungeon01, thirty seconds of empty prison, then eighteen Walkers
  placed at the player, their AI switched off eight seconds later. Empty:
  13.36 ms a frame, `scene_pass` 0.89 ms a pass. Eighteen standing: 16.2 ms
  mean, p95 26.7, 21.7 % of the frames over 20 ms, `scene_pass` 4.17 ms a
  pass - +3.3 ms a pass, about 0.18 ms a Walker and pass; `poses_wait` 8.1
  to 2.3 ms. The same scene fighting (`tools/hand-scripts/perf-walkers-fight.txt`,
  the AI on, an untouchable restrained player, OBVR.dll 2ae75743, 14:50):
  26.6 ms median - half rate on the Beyond - and `scene_pass` 5.6 to 5.9 ms
  a pass.
- The render sampler inside the passes, one sample about a millisecond
  (the symbols need the map of the build the run used - the game folder's
  changed under this reading, so 8f109ea was rebuilt in a worktree for
  its map; with the wrong map the hotspot read as the DrawPrimitiveUP hook):
  empty 1.64 samples a frame, eighteen standing 7.52, eighteen fighting
  10.36. Of the standing growth Oblivion.exe took 2.5, D3D9 (DXVK) 0.65 and
  OBVR.dll 2.26 - nearly all of it `HookedSetVsConstantF` (1.99) with its
  `memcmp` (0.21): the bone lock. Fighting: OBVR.dll 4.01 of the 10.36, the
  hook 3.33 and memcmp 0.45.
- The cause, read in `render/BoneRebase.h` and the hook: `FindBoneRow`
  walked the first render's log along the ring from the running position -
  one compare when the position held, up to the whole log when it did not.
  The log held 4096 rows; eighteen Walkers upload 5200 to 5700 a render
  (the "Bone lock" line: replaced 5173, reordered 136, passthrough 403) and
  Nadi's fort stretch 2753 to 6645 (reordered up to 280, passthrough up to
  368). Past the capacity nothing was logged, the running position missed
  every later row, and hundreds of rows a frame scanned all 4096: about two
  million compares a frame.
- Built (`render/BoneRebase.h`, bone_rebase_test): `kBoneLogRows` 4096 to
  16384, and `BoneRowIndex` over the log by register and rotation
  fingerprint - open addressing on an FNV-1a hash, 4-byte slots (the
  generation stamp and the key's first row side by side, one cache line a
  probe), the rows sharing a key (same-posed twins) on a chain in log
  order, a generation stamp per slot in place of a clear every frame.
  `FindBoneRowIndexed` answers exactly as the ring walk did: the running
  position first, without a lookup, then the key's first row past the
  running position, or its first row when the ring wraps. The index is
  built lazily: a frame whose rows all pair in place - the steady case,
  every row of these runs once the log holds them all - never builds it
  and pays nothing; the first row that misses its running position
  indexes the log once, later misses are lookups, and rows logged after a
  build are picked up by the next miss. (The first cut, daac1a96 in the
  runs below, indexed every row as the first render logged it: 0.2 to 0.3
  samples a frame of `IndexBoneRow` with eighteen Walkers, and the empty
  prison read 0.15 ms a pass slower in both of its runs - the sparse
  writes into 460 KB of tables, suspected, not proven; the lazy build
  removes the question.) The test compares 4235 pairings - twins, every
  register class, running positions on, off and past the end, rows never
  logged - and none differs, and covers the lazy build, the reset, the
  wrap of the generation counter and a row logged after a build. 170 of
  170 tests pass.
- Measured with it (daac1a96 in the Linux folder for the runs,
  `perf-walkers-fight.txt` 14:55 against 14:50): the "Bone lock" line reads
  reordered 0, passthrough 0 - every row in place. Inside the passes
  OBVR.dll 1.07 samples a frame instead of 4.01 (`HookedSetVsConstantF`
  0.33, `IndexBoneRow` 0.27, memcmp 0.11, `FindBoneRowIndexed` 0.05); the
  passes 8.2 samples a frame instead of 10.4, with the engine's own share
  higher (5.2 against 4.6 - the fight is not the same fight twice). In the
  first fifteen seconds among the eighteen `scene_pass` 4.22 against
  5.59 ms a pass, 22.6 against 24.8 ms a frame, 69 against 86 % over 20 ms.
  The fixed run's later phases met GPU wall times of 22 to 29 ms p95 (the
  compositor's `preSubmitGpuMs`; 15 to 16 in the baseline) and 707
  mis-presentations in one phase - the Walkers crowding the camera, not
  the index, which touches no GPU work.
- The standing twin (`perf-walkers-standing.txt`), three runs within the
  hour: OBVR.dll 2ae75743 as the control (15:17), the eager index daac1a96
  (15:11) and the lazy index b5808b67 (15:23). Empty prison `scene_pass`
  0.82, 1.05 and 0.89 ms a pass - the eager index's tables cost the quiet
  scene 0.2 ms a pass, the lazy one nothing the run can tell from noise.
  Eighteen standing: 4.05 to 4.07, 3.14 to 3.17 and 2.97 to 3.09 ms a pass;
  frames 14.3 to 14.9 ms mean with 7 to 11 % over 20 ms against 13.34 to
  13.38 with 0.1 to 0.4 %; the compositor's mis-presentations 485 to 518 a
  phase against 6 to 45; SteamVR reprojected 22.7 % of the control run and
  4.7 % of the lazy one. Inside the passes OBVR.dll 2.48 samples a frame
  against 0.55 (`HookedSetVsConstantF` 0.27 - the register check, the
  in-place compare and the counters a row - and no index work at all: every
  row paired in place, reordered 0, passthrough 0). The engine's and
  D3D9's shares did not move (3.56 against 3.41, 0.91 against 0.90): the
  eighteen standing Walkers now cost about 2.1 ms a pass over the empty
  prison, all of it the engine's.
- Deployed to both installs at 16:05 by the OBVR session as OBVR.dll
  52a058c1797c61ef, built from main abb3b9d (the lazy index 3fd99a6, the
  Walkers session's ShareActorShadows f53cf6d, OBVR's blade slide-off).

**Open:**
- The Pimax's GPU half of the fort scene stays unmeasured (the Beyond's
  2568x2568 without MSAA is not that load). The CPU half is known: eighteen
  fighting Walkers cost the engine about 4.5 ms a pass on this machine and
  OBVR about 0.5 with the index. Answered 2026-10-27 below: crowds of
  3000 to 10,000 bone rows a render run at 13 to 20 ms frames on Nadi's
  machine, the engine's time.
- **Next session (Nadi, 2026-10-26: "ob wir patchen können das mehrere npcs
  weniger ms kosten, vll multi threading"):** what a person costs once the bone
  lock is out of the way. Measured already (`perf-walkers-standing.txt` 15:23,
  the lazy index, eighteen Walkers frozen, inside the passes 5.44 samples a
  frame against 1.61 empty): the shape dispatcher 0x007693E0 36 % inclusive
  and 0x00767520 28 % (the draws themselves, 1186 a pass against 261); the
  light constants 0x007C9140 15 % - three light positions transformed by
  D3DXVec3TransformCoord for every draw, about 0.8 ms a frame; the palette
  builder 0x007655F0 7.6 %, about 0.4 ms a frame, built in both passes
  because the dual pass closes the frame between them - and the second
  build's rows are overwritten by the bone lock anyway. Candidates, to be
  measured before any is built:
  - The engine's own thread switches: both Oblivion.ini files have
    `bUseThreadedMorpher`, `bUseThreadedTempEffects` and
    `bUseThreadedParticleSystem` at 0 - three of the eight thread keys that
    exist in the EXE (2026-09-28). The morpher is per head. Cheap: an A/B
    with the scenario, then the headset for stability (the reason they are
    off is unverified).
  - The second pass's palette build skipped (its frame stamp kept), the
    bone lock then rebasing every row: medium, the skinning collapse's
    neighbourhood; its eye-delta calibration would lose its samples.
  - The light constants cached per draw while the object and its lights
    have not moved: medium; up to 0.8 ms a frame.
  - Fighting adds about 1.5 ms a pass over standing - combat AI, hit
    reactions, animation; outside-the-pass samples of `perf-walkers-fight.txt`
    are unread.
  - Real multithreading of the per-actor work (palettes and light constants
    built on workers before the passes): large; D3D9 itself stays one
    thread (DXVK's CS thread already records Vulkan on its own).
- ~~OBVR knows no headset refresh rate~~ **built 2026-10-26 evening** (Nadi:
  "go for 4"): `vr::OpenVRBackend::DisplayFrequencyHz` reads
  `Prop_DisplayFrequency_Float` (2002, openvr.h) through IVRSystem's float
  property getter (FnTable slot 23, the order checked against
  openvr_capi.h) on the first call and every 900th after, and logs the
  rate and every change ("OpenVR: the headset's display runs at ...").
  Two users:
  - The submit pacing (`render/SubmitPacing.h`, submit_pacing_test): a
    running start counts up to 55 % of the period or 7.5 ms, whichever is
    lower - 7.33 ms at 75 Hz, 6.11 at 90, 3.82 at 144; the fixed 7.5 ms
    while the rate is unknown. Why: at 144 Hz a runtime that hands the poses
    out a whole period early reports about 6.8 ms, under the fixed cap, and
    its frames would be held past their vsync - the Pimax's half rate again
    (a tester plays Virtual Desktop at 144 Hz, 2026-10-23; whether Virtual
    Desktop hands the poses out that way is unverified). The share sits
    between what was measured: the Beyond's running starts 23 to 25 % of
    its period and 52.5 % under load, the Pimax's hand-outs 58.5 to 99 %.
    At 90 Hz it also ends the Pimax's remaining 133 holds a capture (poses
    6.5 to 7.5 ms early, 59 to 67 % of the period), which cost no frames.
    The hold's own cap stays 4 ms: a hold ends at the vsync plus the margin,
    so it is never longer than the running start plus the margin. The
    pacing line now names the cap it used.
  - `[Hands] PhysicsRate` below 0 (`game/HandBodyLogic.h`
    ResolvePhysicsRate, hand_body_test): the headset's own rate, so a body
    steps once a frame - 75 Hz on the Beyond instead of a fixed 90 that
    steps 1.2 times a frame there; the game's own step while the rate is
    unknown. The default stays 0 (the game's 60) and Nadi's live INI keeps
    its 90: following the headset at 144 Hz costs 144 physics steps a
    second, not measured. The settings row goes down to -10 for it.
  Harness: `tools/hand-scripts/headset-rate.txt` (the Beyond's 75 Hz read,
  the cap 7.33 ms in the pacing line, the physics step 1/75 s).
- OBVR's D3D9 timestamp timing on the native driver (medium: read how the
  driver answers TIMESTAMP and TIMESTAMPFREQ queries; low priority while
  the compositor's timing exists).

**Result, 2026-10-26 evening - Nadi's two Windows sessions with the lazy
index and the display-rate cap (read 2026-10-27).** OBVR.dll
08b764afc2de9da0 (main 12a8568), Walkers' ShareActorShadows=1, PhysicsRate
-1, Performance.Enabled=1 (set back to 0 on 2026-10-27). Saved as
`artifacts/nadi-win-live/20261026-214600` (21:20 to 21:46, 127,607
in-game frames, capture 1) and `20261026-224300` (22:28 to 22:43, 69,162
frames, capture 1 as `OBVR-performance-1`, plus the render sampler's
`OBVR-samples.csv`, which only the second session kept). The game folder's
`OBVR-performance-1 - Copy` was byte-identical to `OBVR-performance-1`;
the second session's export went to `OBVR-performance-1-2` because the
first folder was still there. No crash in either session.

- **The display rate and the caps work:** "OpenVR: the headset's display
  runs at 90.00 Hz, a period of 11.11 ms"; every pacing line "a running
  start counts up to 6.11 ms, 55 % of the headset's period or 7.5 ms";
  "Hands: the physics step 0.01670 s -> 0.01111 s (90 Hz)". The poses
  came 9 to 11 ms before the vsync in 70 % of the logged frames (the
  Pimax's whole-period hand-out, nothing held), 5 to 8 ms in the rest;
  519 and 486 holds a session, about one a second, 3.7 to 3.8 ms each
  (`submit_hold` in the captures). The physics step is set and restored
  often (HandBodies.cpp StepPhysicsRate, whenever the rate asked for
  changes between a value and 0) - one float write and a log line each,
  no cost.
- **Steady state holds 90 Hz:** both captures (120 to 210 s into the
  session, calm scenes) have frame intervals p50 11.12 to 11.13 ms, p99
  12.03 to 12.13; scene_pass means 1.02 and 1.34 ms; interleaved
  reprojection inside the captures 4 of 8100 and 4 of 8019 compositor
  rows.
- **Half rate over the whole sessions: 5.0 % and 4.3 %** (6394 frames in
  93 episodes, 2975 in 22), against 2.5 % in the midday run - but it is two
  things, not one:
  - **Crowds.** Session 1 windows 23 to 36 (the ten-second frame-clock
    windows, right after a load): 2700 to 6378 bone rows replaced a render,
    frames 12.8 to 20.4 ms mean, 19 episodes of 100 frames and more holding
    4170 of the 6394 half-rate frames. Session 2 windows 64 to 72: 3760 to
    **10,004** bone rows a render (18 Walkers were 5200 to 6645 in the
    harness, so about 30 skinned bodies), frames 12.4 to 19.7 ms mean, 5
    long episodes with 2554 of the 2975 frames. The lock itself paired in
    place (reordered 0, passthrough 0 in every "Bone lock at scene call"
    line; "Bone far"/"Bone unpaired" lines 30/12 in every run since
    2026-10-24, which is the log's cap, not a count). So the crowd frames
    are the engine's, as the harness said: the next-session bullet above.
  - **Short episodes of 20 to 23 frames** (72 of the 115 episodes, about
    1600 frames): SteamVR's reaction to one dropped frame. The dropped
    frames are 80 to 113 ms spikes in otherwise clean windows, and the
    second capture caught two of them in world mode (frames 2019 and 2144,
    102.5 and 104.2 ms, 1.5 s apart). **Both are OBVR submitting a texture
    the compositor has not seen:** 53 render-thread samples each, 52 in
    OBVR's `present` span (`interop_held` 93.8 and 94.3 ms, so inside the
    submit bracket), the eip in ntdll's ZwQueryAttributesFile (19),
    ZwOpenFile (6), ZwCreateSection (5), ZwClose (4) - file probing and
    section mapping, a DLL load - with the UTF-16 strings
    "\WINDOWS\System32\" and "NVIDIA GeForce" (the adapter description)
    on the stack (the sampler's stack words are filtered to Oblivion.exe's
    range, which 16-bit characters of ASCII text also fall into; that is a
    sampler weakness, not a finding). SteamVR's own log
    (`C:\Steam\logs\vrclient_Oblivion.txt`) has "Created shared texture
    'Scene create Vulkan' 640x640" at 22:31:12.651 and 22:31:14.130 -
    1.479 s apart, the capture's two stalls 1.5 s apart at 22:31:12.5 and
    22:31:14.0 (capture start 22:30:50, the arming's 120 s after the
    22:28:50 start in OblivionPerformanceRuntime.log). 640x640 is
    `ui::kQuickMenuCanvas`: the quick menu's and the spell ring's
    `CanvasOverlay` textures, each created on first use. **Every shared
    texture OBVR creates costs about 90 to 110 ms on Windows**, in
    vrclient's import of the D3D9 texture (not on Linux: the recent
    harness runs' frame-clock maxima are 36 to 41 ms with the same menus).
    Session 1's vrclient log lines land on its spikes one for one: the
    eye textures 4028x3380 x7 and the HUD's 2048x2048 x10 at 21:21:21 (86 ms
    apart each) during the load, the crosshair cache 256x256 at 21:21:23,
    the two 640x640 at 21:21:32 and 21:21:43 (windows 4 and 5, 101 ms
    each), a 512x512 at 21:27:00 (window 37, 105 ms; which layer is 512 is
    unverified - the settings menu is the candidate), and the enemy health
    bars' 256x64 (`kEnemyHealthCanvasWidth`) at 21:30:10, 21:33:12,
    21:36:05 and 21:36:36 (windows 56, 74, 91, 95: 112, 95, 103 and 100
    ms) - one texture per bar slot, created when a bar first shows. Three
    more spikes are the INI re-read after a write ("Config: OBVR.ini
    changed - re-read in 86.6 to 91.2 ms", windows 114, 117, 146; OBVR
    writes the INI through WritePrivateProfileStringA without logging, so
    who wrote is unverified - the settings menu is the candidate). Eight
    spikes of 80 to 106 ms in session 1 stay unexplained; the two the
    second capture caught in menu mode (frames 6391 and 6701, 79.5 and
    80.2 ms) were the engine loading textures - eips in d3dx9_27.dll and
    the archive reader 0x0042Cxxx - which is the inventory's, not OBVR's.
- **Lessons:** the compositor's `pre_submit_gpu_ms` of 81 to 83 ms in
  those frames was the CPU stall seen from the GPU timeline, not GPU work;
  the ntdll eips (file syscalls, not win32u's
  NtGdiDdDDISubmitWaitForSyncObjectsToHwQueue, which the normal present
  waits sit in) told them apart. Two tools for the next time:
  `tools/win-log-timeline.py` (the ten-second windows with half-rate
  episodes, bone rows and marks) and `tools/pe-exports.py` (the export a
  system DLL's eip falls in).

**Open from this run:**
- **Pre-create the overlay textures** (quick menu, spell ring, stow spot,
  enemy health bar slots, the 512 one, hand HUD) at the first world frame
  after a load instead of on first use: small - the `CanvasOverlay` and
  the bar slots exist, they need a warm-up call and a log line counting
  creations ("Overlay: texture NxN created, n so far") so the harness can
  prove one creation per layer per session; the Windows cost itself (90 to
  110 ms per import) is vrclient's and stays, moved into the load. Saves
  about eight 100 ms frames and their half-rate episodes a session.
  **Built 2026-10-27** (harness PASS headless 07:19, run
  `artifacts/hand-script/overlay-warmup/20261027-071926`: all fourteen
  warm-up lines at the first world frame after the load, the quick menu
  opened later without a "created in play" line; deployed to both installs
  as OBVR.dll b5c7397e5ba18776 from f07c542; the headset proof is Nadi's
  next Windows session): `[Render] WarmOverlays=1` (default on; 0 = made at first use,
  as before). At the first world frame after a load without a menu
  (render/OverlayWarmup.h, `StepOverlayWarmup`, re-armed by every load
  and by a new game device; tests/OverlayWarmupTest.cpp) the camera hook's
  `MaybeWarmOverlays` makes each layer's texture and overlay and hands the
  texture to `SetOverlayTexture` once, hidden: `CanvasOverlay::Warm` for
  the quick menu, the spell ring, the eight enemy health bar slots, the
  holster-fit guide panel and the stow spot, `VignetteLayer::Warm` and
  `CrosshairLayer::Warm`. Only layers the settings can show are listed
  (Full VR for the panels, EnemyHealthBars, either vignette, Crosshair):
  each import costs about 0.1 s of the load on Windows, all fourteen
  about 1.4 s, once a session - a texture handed over once is not handed
  again at the next load ("already warm"). The hand HUD needs none: it
  hands its atlas to every element's overlay at its first submit, the
  load's first world frame already (HandHudLayer `m_warmed`). Proof in
  OBVR.log: `Overlay warm-up: "OBVR Quick Menu" 640x640 handed to the
  compositor (1 of 14, 1 handed this session, frame n)` per layer, and
  `Overlay: "<name>" WxH texture created in play - not warmed` from any
  show that hands a texture over for its first time - with the warm-up
  on, that line should never appear. On Windows the proof is SteamVR's
  "Created shared texture" lines all inside the load. Harness:
  tools/hand-scripts/overlay-warmup.txt. Not handled, as before: a
  Direct3D device reset - nothing releases or re-creates these
  default-pool textures (`CanvasOverlay::Destroy` runs only in the
  destructor and leaves `m_textureTried` set).
- **Skip the INI re-read after OBVR's own write** (apply the value in
  memory, remember the mtime as ours): small, in core/Config.cpp's
  reload gate; saves 90 ms per settings change.
- **The sampler's stack words** should accept return addresses in any
  loaded module's code range and name the module (the modules CSV exists),
  and reject words that read as UTF-16 text: small; then a stall like this
  one names vrclient/d3d9/nvwgf2um frames instead of ntdll alone.
- The crowd itself: unchanged, the next-session bullet above. Nadi's
  10,004-row scene is the biggest seen so far.

**Oblivion.ini A/B for the crowd, 2026-10-27 morning (Nadi: "C + D").**
Three headless runs of `perf-walkers-standing.txt` (eighteen Walkers placed
and frozen, OBVR.dll bff28a45 = main 0533da7, Walkers' 64-look build), the
Linux prefix's Oblivion.ini changed one value at a time and restored after
(it stood at iActorShadowCountInt 5, Ext 3; Nadi's Windows INI has 10 and
10, so the baseline was raised to 10). Phase "people-placed to
people-watched", about 3300 frames each:

| run | iActorShadowCountInt | bUseThreaded Morpher/TempEffects/ParticleSystem | scene_pass mean | frame mean | over 20 ms | "GPU before submit" |
|---|---:|---|---:|---:|---:|---:|
| 07:35 | 10 | 0 | 3.12 ms | 13.72 ms | 2.8 % | 8.03 ms |
| 07:39 | 4 | 0 | 2.87 ms | 13.38 ms | 0.3 % | 7.45 ms |
| 07:43 | 10 | 1 | 2.95 ms | 13.35 ms | 0.1 % | 7.30 ms |
| 2026-10-26 15:23 | 5 | 0 | 3.01 ms | 13.37 ms | 0.3 % | 7.72 ms |

- **C, the shadow count:** 10 to 4 saves about 0.25 ms a pass of CPU with
  eighteen people and takes the long frames from 2.8 % to 0.3 % - the five
  shadows between 5 and 10 cost the same again the other way (the 10-26
  run). The "GPU before submit" column is wall time on the GPU's timeline
  and shrinks with the CPU, so its 0.6 ms is not GPU work alone. Zero code:
  Nadi's Windows INI at 10/10 is the dearest setting; 4 is the
  recommendation, the headset says whether the missing far shadows show.
- **D, the three thread keys:** on, the run was 0.17 ms a pass faster and
  as clean as the 4-shadow run, with no crash and no warning in 7 minutes -
  but one run each, and the runs scatter by 0.1 to 0.15 ms a pass (section
  7, the control runs), so this is a hint, not a result. The empty phase of
  that run was the worst of the three (13.99 ms mean, 4.7 % over 20 ms), a
  hitch cluster during the prison's first seconds that the other runs did
  not have - unexplained. A second pair of runs settles it; the stability
  question (why Bethesda shipped them off) only the headset over hours.
- Both together are not measured.

**The crowd scenario, 2026-10-27 (Nadi: "bau ein harness mit 25, 50 und 100
walkers").** `tools/hand-scripts/perf-walkers-crowd.txt`: the prison empty,
then 25, 50 and 100 Walkers placed at the player in one heap, frozen (tai
once - it is global), fifteen captured seconds each, one capture with the
sampler. First run 07:48 (OBVR.dll bff28a45, the Linux INI's 5 interior
shadows, run `artifacts/hand-script/perf-walkers-crowd/20261027-074800`):

| phase | frames | frame ms mean | scene_pass mean | in-pass samples: Oblivion.exe / D3D9 / OBVR.dll |
|---|---:|---:|---:|---|
| empty prison | 1500 | 13.33 | 0.75 | |
| 25 Walkers | 1095 | 13.70 | 2.85 | 62 % / 16 % / 12 % |
| 50 Walkers | 620 | 24.21 | 5.10 | 64 % / 14 % / 12 % |
| 100 Walkers | 153 | 50.96 | 10.67 | 67 % / 12 % / 14 % |

The 100 phase got 8 of its 15 seconds (the capture's 95 s ended first; the
script's waits drift against the capture's start by a few seconds - give
the capture 110 s or the pre-phases less for the next run; 168,848 of
262,144 events were used). A person costs about 0.085 ms a pass at 25,
0.087 at 50 and 0.099 at 100: nearly linear, with a quadratic term on top.

- **The quadratic term is one engine function:** the 256-byte bin
  0x007D5F00 holds 5.5 % of the in-pass samples at 25, 11.3 % at 50 and
  18.4 % at 100 (2532 of its samples on one instruction, 0x007D5FC3). Read
  in Oblivion.exe (REA/Ghidra, 2026-10-27): FUN_007D5F80 is a thiscall on a
  NiTList (head at +4; nodes next, prev, data) that walks the list comparing
  each node's data with a key and returns the match ref-counted. Its hot
  caller is FUN_007D6940, a method of a ShadowSceneLight-class object: it
  takes the geometry's type-4 property (NiAVObject::GetProperty is
  FUN_00707530, the property list at +0x98 per xOBSE NiObjects.h), asks the
  property's light count (vtable +0x54, accepted between 1 and 10), and then
  searches the geometry in the list at this+0xE4 - in xOBSE's
  ShadowSceneNode layout +0xE4 is `lights`, a NiTPointerList, so the owner
  being a ShadowSceneNode and the list its lights, or the owner a light and
  the list its geometries, is unresolved (the key compared is the geometry
  pointer, which only the second reading matches). The outer caller
  FUN_007C5750 (from 0x007C6100, from 0x007C7050, the shader's per-geometry
  light setup next to the light constants 0x007C9140) iterates the
  property's lights (list +0x70, count +0x78, cursor +0x7C) and calls
  FUN_007D6940 for every light whose byte +0xF4 is clear. **Derived:** for
  every drawn shape, for each of its lights, a walk of a list that holds
  every shape that light touches - in a crowd under one light that is every
  actor shape, so the cost is shapes squared: about 0.16 ms a pass at 25
  Walkers, 0.58 at 50, 1.97 at 100. What the lookup is for (a shadow
  caster/receiver test, a light's geometry registration) is unverified.
- **OBVR.dll's own share** at 100 is 14 % of the pass, about 1.5 ms:
  HookedDrawPrimitiveUP 3.7 %, HookedDrawIndexedPrimitive 1.9 %,
  HookedSetTransform 1.2 % and a 1.8 % bin the map names
  MenuViewYScaleFromRows+0x210, which is not that function (a CRT routine
  after it, unresolved). The hooks' fixed cost per draw times the crowd's
  draws; not looked into yet.
- **Options from this** (the next-session list above stays):
  - *An index for the per-light geometry lookup* (a detour of
    FUN_007D5F80 that remembers (list, key) -> node and verifies the node
    before trusting it: data equals the key, prev/next links consistent,
    else walks and refreshes; NiTList nodes come from a pool, so a stale
    node is readable but must be verified): medium - the detour pattern and
    the pure-logic tests exist (the bone lock's index), the risk is the
    node pool's lifetime; the gain is the quadratic term, about 2 ms a pass
    at 100 Walkers, 0.6 at 50, 0.15 at 25. Measurable with this scenario
    before and after.
  - *Fewer shapes per Walker* (Walkers' meshes merged): hits the draws, the
    palettes and this walk's n squared at once - still the largest lever.
  - *Fewer lights on a crowd* would shrink the outer loop; which lights a
    property lists is not understood.
- The three sizes make every optimisation comparable: run the scenario
  before and after, read the three scene_pass means and the bin 0x007D5F00's
  share.
- **Draws per Walker** from the "Dual draws" heartbeat of the same run: 242
  a pass for the empty prison, 1254 / 2012 / 3572 with 25 / 50 / 100 -
  **35 to 40 draws per Walker per pass**, twice a frame. That is the
  linear cost (0.085 ms a pass, 0.17 ms a frame per Walker); nothing in
  OBVR or the engine makes one draw cheaper, so the lever is fewer shapes
  per Walker (merged same-material skinned shapes, outfits baked into one
  body NIF per look, variety by texture, no hair or jewelry on zombies;
  the head keeps its own shape for the facegen morph) - handed to the
  Walkers session 2026-10-27 with the measuring recipe. Walkers' answer
  (pyffi over the deployed looks): 10 to 21 NiTriShapes per look, mean
  15.1, 7 to 11 NIF parts; the look the crowd scenario places (0x801) has
  17. So **about 2.3 draws per shape per pass**: the rest of the 35 to 40
  are the engine's extra passes per shape - the lighting passes when more
  lights touch the shape than one pass carries, and the shadow-map pass
  for every shadow-casting light that sees it (derived from the two
  counts; which pass is how many is unmeasured - the shape clock plus a
  draw-per-shape probe would say). Two multipliers then: shapes per Walker
  (content) and passes per shape (lights in range, shadow count - Nadi's
  10 interior shadows against the Linux run's 5). Nadi's call on the
  quadratic walk's index (2 ms a pass at 100 against 51 ms frames): not
  worth building alone; the analysis stays in OBNG
  knowledge/shadow-light-list-remove.md.
- **Open bug, the bone log overflows at 100:** "Bone lock at scene call"
  lines in the 100 phase show 20,305 to 22,872 rows replaced a render
  against kBoneLogRows 16384, and "a body length off" 332 to 492 (rows
  paired to a wrong twin - the frozen heap puts every Walker in the same
  pose, so the rotation fingerprint cannot tell them apart once the running
  position is lost past the log's end). The "passthrough" count in those
  lines is not a symptom: it is the rows heading for shadow and reflection
  targets, which the lock lets through by design. At 50 (11.7k rows) no
  wrong twins; at 25 clean. **Fixed 2026-10-27, kBoneLogRows 32768**
  (1.7 MB of rows, the index's UInt16 row numbers still fit; bone_rebase_test
  checks the hundred-Walker count and the UInt16 ceiling). Run 13:09
  (`20261027-130945`, PASS): 22,715 to 23,199 rows a render inside the
  log, wrong twins 24 to 39 a render (the identical poses), frames 13.41 /
  26.0 / 53.6 ms and scene_pass 2.87 / 5.65 / 11.37 at 25 / 50 / 100 -
  against 13.70 / 24.2 / 51.0 and 2.85 / 5.10 / 10.67 in the 07:48 run on
  an older main (bff28a45) and 27.1 / 25.6 / 60.3 in the 12:59 run on the
  current main with the smaller log: the bigger log costs nothing the runs
  can tell apart; the 5 to 10 % between the mornings' runs sits in the main
  tree's other work and the run-to-run scatter under load, unseparated.
- **The second eye does not repeat the relight** (relight clock,
  render/RelightClock.h, run 12:59, `artifacts/hand-script/perf-walkers-crowd/
  20261027-125927`): per 120 scene calls the engine relit 840 geometries in
  the first pass and 120 in the second with the prison empty, 25,048 against
  218 with 25 Walkers, 52,759 against 248 with 50, 92,355 against 428 to
  502 with 100 (first pass about 770 relights a frame at 100 - not every
  shape every frame). So the dual pass pays the crowd's light-list walks
  once; the hoped-for OBVR-side skip does not exist, and the clock stays as
  a diagnostic: "Relight at scene call" on the heartbeat, `relight` in a
  capture with the call clock. Its first build killed the load: the hook
  assumed `ret 8` from the analysis, the function is __cdecl
  (`add esp, 0xC; ret`, ecx unread) - the convention is read off the RET
  now, never off a report.

**Nadi's Windows sessions of 2026-10-27 (13:40 to 13:54 and 13:55 to
14:27, OBVR.dll 7617fbd7 = main ccbaf9a + relight clock + bone log 32768;
no capture, Performance.Enabled was 0; saved as
`artifacts/nadi-win-live/20261027-135450` and `20261027-142731` with the
vrclient logs).**

- **The overlay warm-up is proven on Windows.** SteamVR's vrclient log
  shows every "Created shared texture" of both sessions inside the load's
  first seconds: 13:40:28.6 to 13:40:31.0 and 13:56:06.6 to 13:56:09.2 -
  the fourteen warmed layers plus the holster sign and the HUD's ten
  2048x2048, each 97 to 110 ms apart (the import's cost, now paid in the
  load: about 2.6 s once a session), and **not one shared texture created
  later in play**. OBVR.log: 14 and 15 "handed to the compositor" lines per
  session, later loads "already warm". One exception: in the first session
  OBVR's new "OBVR Holster Sign" (384x96) logged "created in play - not
  warmed" at the first world frame after the load - its Show runs earlier
  in that frame than MaybeWarmOverlays; one 100 ms stall per session,
  handed to the OBVR session (the sign is its layer).
- **Half rate 4.2 % and 6.5 %** (3082 frames in 20 episodes; 9966 in 136).
  The long episodes are crowds again (session 2: windows with 5000 to 9600
  bone rows a render at 15 to 19 ms frames, 6471 of the 9966 frames in 20
  episodes of 100 and more). The short ones (86 episodes, 1951 frames in
  session 2) follow single frames of 25 to 45 ms in otherwise clean
  windows and eight 75 to 89 ms spikes with no mark in the log - not
  textures any more (vrclient shows none), not INI re-reads (one in that
  session). In session 1 four of the six spikes over 60 ms were INI
  re-reads after a settings write (90 to 105 ms each, five in the
  session) - the "skip the re-read after OBVR's own write" item above
  stands. The remaining spike class needs a capture with the sampler:
  Performance.Enabled set to 1 in the live INI for the next Windows test
  (2026-10-27 afternoon).
- Nadi's Oblivion.ini still has iActorShadowCountInt/Ext 10 (the
  measured 0.25 ms a pass against 4 is not yet taken).
- Calm windows hold 11.11 ms mean throughout; the relight clock's
  heartbeat shows up to 79,450 relights per 120 scene calls in the first
  pass (the crowds) and the second pass near zero, as on Linux.

**Open bug, 2026-10-27 13:54: a crash while loading a save from inside a
dungeon (Nadi, Windows).** Report `C:\Steam\steamapps\common\Oblivion\Crash
Logs\CrashLogger.2026-10-27-12-54-52.log` (the name is UTC; the session's
OBVR.log is `artifacts/nadi-win-live/20261027-135450`, its last lines "Hud: a
world frame while the game loads" and "Menu pause: behind menu Loading").
Read: EXCEPTION_ACCESS_VIOLATION at 0x00722A1A, the first store of
FUN_00722A10 (NiGeometry's visit: copies +0xAC/+0xB0 property states into the
culling process's record and runs the controllers), called from 0x00719BA9
inside 0x00719BA0 - a small virtual that is NiGeometry::OnVisible(culling
process) (the +0x200/+0x204/+0x20C checks on the process, then its vtable
+0x16C). ecx/edi held 0x47077D68, a Lighting30ShaderProperty, where the
geometry belongs: the child pointer the culling walked led to freed memory
reused by a property. On the stack: Creature FF0013A9 and FF0013AA (temporary
refs, base 25000802 Walker), the BSFadeNode "(FF0013AA) -> Walker", a
bhkCharacterController, SaveGameFile. Walkers' log of that session never
names those two refs (the Walkers session, 2026-10-27): they belong to the
world being torn down, not to its plugin's actions. OBVR hooks nothing at
these addresses. **Derived:** the scene was culled and rendered while the
load replaced the world, and one of the old world's actors was already
freed - a use-after-free on the engine's render path during a load. What
OBVR adds to the exposure: the dual pass runs both scene passes during a
load too (`request.dualEyes` is not gated on loading; the log's "a world
frame while the game loads" confirms world frames then), so every load has
twice the traversals of a vanilla frame over a scene being dismantled.
Whether that is the difference between surviving and dying is unverified.
Two other archived crashes have other signatures: 2026-10-26 11:55 in
0x00765990 under the culling dispatch with a torch's "Sparks" particle
system (the torch-flame work of that day), 2026-10-26 21:34 in xOBSE's
EventManager::TryGetReference under a script's SetGoldValue/AddItem
(Keyring.esp) - not this bug.
- **Cheapest step, built:** `tools/hand-scripts/load-after-crowd.txt` -
  twenty-five Walkers placed and killed in the prison, then the console
  loads `CrashRepro` (a copy of Nadi's Save 22, Waterfront, placed in the
  shared Saves folder 2026-10-27); the game dying reproduces it, and then
  `[Render] Stereo=mono` or a second pass skipped while
  `LoadingThreadActive() || Loading menu` says whether OBVR's share
  decides. **Run 15:39: the load survived** - not reproduced with this
  approximation (frozen killed clones in the prison, an exterior save; run
  `artifacts/hand-script/load-after-crowd/20261027-153920`). A real save
  from inside the dungeon with living Walkers is the missing piece - Nadi
  saving before such a load next time gives it.
- **Run 16:14, `crash-load-dungeon.txt`** (four cycles of coc MemorialCave,
  20 Walkers killed and 20 alive with AI on, then a console load of the
  exterior save while they move; `artifacts/hand-script/crash-load-dungeon/
  20261027-161429`): **all four loads survived** with 130 to 180 actors
  loaded, 90 to 117 dead. Not reproduced on Linux. What still differs from
  Nadi's case: the Windows install's mod batch of 2026-10-26 (UOP, EGO,
  VGR, HGEC, Better Dungeons, Maskar's mods - the Linux install has none of
  them, memory oblivion-mod-setup), his real save (Walkers spawned by the
  spawn replacement with horde packages, not placed clones), and one load
  in a 15-minute session against four here. Next: the scenario on Windows
  (the PowerShell runner, tools/hand-script-run.ps1, in a Windows session),
  or Nadi's own save from inside the dungeon.
- **With Windows' mods mirrored on Linux** (2026-10-27: the 11,329 Data
  files only Windows has, its Plugins.txt and plugin mtimes, OblivionWalkers
  at 0x25; restored from a manifest after each session):
  - 16:37, `crash-load-dungeon-win.txt`: **the second load crashed - in
    OBVR.** RefStillThere+0x28 from UpdateHandMode via SettledRef::Step:
    the pick ring's settled reference of the old world checked in the
    first frame after the load, its page given back, the read itself
    faulting (`Crash Logs/CrashLogger.2026-10-27-15-40-27.log`, kept with
    the run). **Fixed ad5c54f:** every fingerprint carries the world it was
    taken in (a counter stepped on every loading frame at the top of
    OnPresent), a reference of an older world is gone without a read, and
    RefStillThere reads only committed, readable memory (VirtualQuery).
  - 16:58 with the fix: PASS, four loads, ten "Refs: a load runs" lines.
  - 17:04 with the fix: a third crash, before any load, in
    ReadSkinnedShape+0x42B (ShapeName reading a bone's name pointer that
    read 0x3F800000, a float 1.0) - [Hands] FittedHands reading the
    first-person arms node every few frames, the stack holding
    "Decal:Foot:upperHGEC_C" (HGEC, a Windows mod) and Player1stPerson,
    Walkers hitting the player (`CrashLogger.2026-10-27-16-05-37.log`).
    Not load-related; the Ragdolls session owns FittedHands (abdee3c) and
    has it. SkinRead's pointer checks are range checks only.
  - Nadi's own crash (the engine's NiGeometry::OnVisible) is still not
    reproduced: the three crashes share its class (a pointer into memory
    a load or a mesh swap took away), not its site.
- **The fix on OBVR's side if it does:** no scene passes while a load runs
  (the world is behind the cinema screen then anyway): small, a gate in the
  dual-pass request and the scene render hook, tested by the scenario.
- If the engine dies without OBVR's passes too, it is the engine's race
  (background loading against the render) widened by many temporary
  actors; then only fewer temp refs at load (Walkers) or a load-time pause
  of the render by a detour would help - medium, unmeasured.

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
- Comparability: every run above predates 2026-10-26 09:31, when the
  Walkers' install turned its horde on by default (walkers out of combat
  form groups outdoors and get packages by the console once a second). A
  later Gold Road run is only comparable with `[Horde] Enabled=0` in
  OblivionWalkers.ini (a single-value change, put back afterwards), or
  against runs of its own kind.
