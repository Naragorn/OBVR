# The device hooks' cost per draw

OBVR hooks the D3D9 device's draw and state calls. Every draw of the scene
pass runs through them, and with OBNG's single-pass stereo every draw runs
twice, once per eye. In a heavy scene that is several thousand hook calls a
frame, so a few nanoseconds of bookkeeping per call add up.

## Where the time went (Windows, 2026-11-23)

Nadi's session in the tutorial dungeon, the assassins' ambush: about 2 400 pass
records a frame, the scene pass at 11.2 ms, the frame at half rate. The render
sampler put 0.99 ms a frame of that pass inside OBVR.dll (0.32 ms in the calm
stretch before the fight):

| Where | calm | fight | What runs there |
|---|---|---|---|
| `HookedDrawIndexedPrimitive` | 0.10 | 0.35 | the draw hook |
| `HookedSetVsConstantF` | 0.08 | 0.21 | the vertex constant hook |
| `InFirstPersonPass` | 0.02 | 0.12 | asked once per indexed draw |
| `MaybeSampleAfterPassDraw`, `ProbeModelDraw` | 0.04 | 0.12 | helpers that only work in the menu's pass |
| water reprojection | 0.03 | 0.07 | called at every constant upload |
| the rest | 0.05 | 0.12 | the smaller hooks |

The function names in this table are derived: that evening's DLL no longer
exists, so the sampled addresses were matched to a build of the same day by
the address shift that puts all 86 distinct return addresses right after a
call instruction.

Oblivion Walkers was loaded in that session, in its apocalypse mode (spawns
become walkers, the dead rise); its log of that evening says so. Found on
2026-11-25. What it added to the fight's load was not measured.

## What changed (2026-11-24)

Four things a world draw paid for without needing them:

1. **Stream 0's pool membership.** The skinned-draw counters searched a set of
   up to 64 buffers twice in every indexed draw. The answer is now kept per
   binding (`Stream0Membership`, `render/DrawHookFastPath.h`) and asked again
   only when the bound buffer changes or joins the pool.
2. **The menu's draw helpers.** `MaybeSampleAfterPassDraw` and `ProbeModelDraw`
   were called from every draw and returned at once outside the menu's pass.
   The draw hooks now test that themselves (`AfterPassSampleDue`).
3. **Bone rows.** The constant hook classified every upload as a bone row or
   not, also in the single pass where no bone lock runs. It now asks first
   whether a lock runs (`BoneRowsWatched`); the classification itself moved to
   `IsBoneRowUpload` in `render/BoneRebase.h`.
4. **The water reprojection.** `BuildWaterReprojectionConstants` was called at
   every upload and did nothing unless the water's replacement vertex shader
   was selected. The vertex shader hook now keeps that answer
   (`DrawHookMemo::waterShaderSelected`) and the upload skips the call.

None of them changes what reaches the device: the same calls, in the same
order, with the same arguments.

## Measured before and after (Linux)

Scenario `obng-profile-prison-fight-hold.txt` from the OBNG repo: the same
fight, the head held still, 1 780 to 2 000 records a frame (OBNG's count). Run
with Nadi's Windows mod list, OBNG's routes as played, OBNG.dll 7700b6fd, no
SteamVR (the harness's mock headset), each run alone on the machine. Read
between the marks `h04` and `done`: 42 s, about 2 930 frames. The numbers are
the render sampler's samples inside OBVR.dll, in ms per frame; two runs a side.

| OBVR.dll | before (b994357) | after |
|---|---|---|
| inside the scene pass, all of it | 0.470 / 0.479 | 0.285 / 0.275 |
| `HookedDrawIndexedPrimitive` | 0.142 / 0.160 | 0.103 / 0.106 |
| `HookedSetVsConstantF` | 0.100 / 0.092 | 0.054 / 0.049 |
| water reprojection | 0.042 / 0.044 | 0 |
| `MaybeSampleAfterPassDraw` | 0.029 / 0.025 | 0 |
| `ProbeModelDraw` | 0.024 / 0.030 | 0 |
| `InFirstPersonPass` (not changed) | 0.046 / 0.037 | 0.044 / 0.043 |
| outside the scene pass | 0.315 / 0.310 | 0.289 / 0.290 |

OBVR.dll's part of the pass falls from 0.475 to 0.280 ms a frame, 41 % less.
OBNG.dll's part did not grow with it (1.399 / 1.255 before, 1.355 / 1.190
after), so the time did not just move next door.

All four runs loaded Oblivion Walkers (DLL and ESP, apocalypse mode; each
run's OBVR.log shows it). The Linux game had carried it since 2026-11-20
without anyone meaning it to, found on 2026-11-25. It was the same on both
sides, so the comparison stands; the level is that of a game with Walkers in
it.

What these runs do not show:

- **A game without Walkers.** Not run. The saving is per hook call, so it
  should follow the number of draws; that is derived.

- **The scene pass as a whole.** It read 6.71 / 6.10 ms before and 6.38 / 5.64
  after. Two equal runs differ by 0.6 ms, three times the gain, so the pass
  total proves nothing here.
- **Windows.** Not measured after the change. Scaled from the Linux result the
  fight there should lose about 0.4 ms of OBVR.dll's 0.99 ms; that is derived,
  not measured.
- **The left-handed mirror.** Not run. With it, `InFirstPersonPass` is asked
  twice per indexed draw; that is unchanged.

## One thing learned on the way

The first build wrapped the sampler's test in a small inline function. The
compiler inlined the big sampler into the wrapper instead, since the wrapper
had become its only caller, and then called the wrapper from all four draw
hooks: 0.039 / 0.032 ms a frame for a test of three flags. The test now stands
in the draw hooks themselves.

## Still there

Per frame in the fight under Linux, by the render sampler:
`HookedDrawIndexedPrimitive` 0.10 ms, `HookedSetVsConstantF` 0.05 ms,
`InFirstPersonPass` 0.04 ms.

**A hardware profile of the same fight** (OBNG, 2026-11-24 evening, `perf`
with cycles and AMD's IBS samples on the render thread, run
`obng-profile-prison-fight-hold/20261124-201627` in the OBNG repo, OBVR.dll
`fa14e359`, about 2 170 draws with a second eye a frame) reads higher: OBVR.dll
0.41 ms a frame inside the pass (0.35 to 0.44 over four runs), of it
`HookedDrawIndexedPrimitive` 0.157, `InFirstPersonPass` 0.116,
`HookedSetVsConstantF` 0.033, every other hook 0.015 or less. The two
instruments differ in level (the profiler's own cost, another OBNG.dll); they
agree that the indexed draw hook and its first-person question are what is
left.

Where those cycles sit says most of them are not the hooks' own work:

- In the draw hook they lie on its first stores (the frame it sets up, the
  arguments it pushes again) and on the instructions right after a call comes
  back, the device's draw above all. A wait that the code before a call
  caused is charged to the code after it. OBNG saw time move across the
  module borders that way when its own DLL got faster: OBVR.dll rose from
  0.184 to 0.207 microseconds a draw while OBNG.dll and D3D9.DLL fell.
- `InFirstPersonPass` is the first code after the device's draw returns. It
  holds 28 % of OBVR.dll's cycles but 7 % of the time its sampled operations
  take from start to finish, 4 % of its instruction fetch waits and 4 % of
  its data cache misses. Its instructions are cheap; the waiting happens
  there.
- OBNG's timer round the device calls of its eye switch puts a hooked
  `SetViewport` at 10 ns a call and an unhooked `GetViewport` at 11, with the
  timer's own 8 ns in both. A simple hook costs about what no hook costs.

Read in the code:

- **`CallClock`** costs one flag test a hook. It reads the clock only with
  `[Performance] CallClock=1`, and that is off unless the INI sets it.
- **The indexed draw hook** registers an exception frame at every call
  (`CallClock` has a destructor) and makes three calls that return at their
  first test for a world draw (`TakeAfterPassCursorQuad`, `SwapCullForMirror`,
  and the back faces' question to `InFirstPersonPass`, which checks two
  pointers and reads one byte).

So how much a leaner draw hook would give back is not known. The cycles on it
are the upper bound, 0.27 ms a frame; what the hook does itself points at a
few hundredths of a millisecond (derived, not measured). The step that would
settle it is one run in which the draw hooks are taken out of the device's
table and put back every few seconds, the pass compared between the two
halves. It needs a switch in InterfaceRenderHook.cpp, which is at its size
limit, so something has to move out of that file first. Not done: too much
for a question worth one to four percent of the pass.

## How to repeat it

From the OBNG repo, once per DLL:

```
python3 tools/as-played-ini.py <ini> Log.SummaryTimer=1 Log.SummaryEverySeconds=5
python3 tools/run-with-windows-mods.py -- \
  tools/with-game-ini.sh Display:fSpecularLOD1=500.0000 Display:fSpecularLOD2=800.0000 Display:iActorShadowCountInt=4 -- \
  tools/with-plugin-dll.sh OBVR.dll <the OBVR.dll to measure> -- \
  tools/shadow-run.sh --ini <ini> tools/hand-scripts/obng-profile-prison-fight-hold.txt \
    --save OBNGPrisonFight --run-timeout 420 --no-steamvr --exclusive
```

Then read the runs side by side, each with the link map of the DLL it loaded
(the build writes `OBVR.map` beside `OBVR.dll`; a map of another build names
other functions):

```
python3 tools/perf-dll-marks.py h04 done \
  before1=<run folder>=<before/OBVR.map> before2=<run folder>=<before/OBVR.map> \
  after1=<run folder>=<after/OBVR.map> after2=<run folder>=<after/OBVR.map>
```

It prints the frames between the two marks, OBVR.dll's time a frame inside
and outside the scene pass, every module's, and OBVR.dll's functions. Keep
OBNG.dll the same on both sides, and keep the two DLLs with their maps until
the numbers are written down: the run folders hold neither.
