# Release hardening for Full VR

The release review of 2026-11-01 (Nadi: "gib mir 1-5 dinge die dringend
gemacht werden müssen vor dem release von full vr") named five things to
settle before Full VR leaves the Discord beta. Each has its section here:
what was wrong, what was read and measured, what was built, and what is
still open. Marks: **read** (code, header, EXE bytes), **measured** (game,
harness), **derived**, **unverified**.

1. SteamVR's quit killed the game - built 2026-11-01.
2. No crash handler - see section 2.
3. The hand-to-hand handover ends the game - see section 3.
4. Scene passes while a load replaces the world - see section 4.
5. A feature freeze and beta 8 - see section 5.

## 1. SteamVR's quit

**What was wrong (read).** OBVR polled `IVRSystem::PollNextEvent` only
while its SteamVR keyboard was open (`StepVrKeyboard` in
camera/CameraHook.cpp, the `if (g_keyboard.open)` around `PollKeyboard`).
SteamVR's `VREvent_Quit` (700) went unseen and unanswered. openvr.h on
`AcknowledgeQuit_Exiting`: "Call this to acknowledge to the system that
VREvent_Quit has been received and that the process is exiting. This
extends the timeout until the process is killed." Without it, closing
SteamVR or quitting from the dashboard ends with vrserver killing
Oblivion.exe - unsaved progress gone. Not measured in a headset before the
fix (derived from the header).

**Read in the EXE.** All three of the game's ways out write the same byte,
OSGlobals->quitGame (`*(0x00B33398) + 0`), and xOBSE hooks its exit messages
at those three places: the console's qqq 0x005077EA, the main menu's Exit
0x005B5A0D, the pause menu's Quit 0x005BDE60 (game/GameAddresses.h,
kOsGlobalsPointer; ../OBNG/knowledge/quitting-the-game.md).

**Built.**
- `OpenVRBackend::PollEvents` drains the queue every frame (64 at most);
  the keyboard's events count only while the keyboard is open, as before
  (vr/VrEvents.h `TakeEvent`).
- A quit: `AcknowledgeQuit_Exiting` (IVRSystem FnTable entry 47,
  openvr_capi.h IVRSystem_026, counted 2026-11-01), then quitGame = 1 - the
  game leaves the way its own qqq does (`StepQuit`: once; kept pending while
  the engine's globals are unreadable; only acknowledged when the game
  already quits). Log: "SteamVR quit: acknowledged, the game told to quit".
- Unit flows: vr_events_test (the event numbers and the table entry, the
  keyboard's events open and closed, the buffer's end, the quit with the
  keyboard open and closed, the neighbouring event numbers refused, every
  StepQuit flow).
- The harness learned `ends` (src/test/HandScript.h, both runners,
  hand_script_test; tests/HandScriptRunnerTest.py `Ending` - not run on
  Windows, no Python there): the game ending by itself is the pass, a game
  still running 30 s after the script a problem, and without `ends` an
  ending stays "the game ended during the script".

**Measured (harness, Windows, 2026-11-01).**
- `steamvr-quit.txt` PASS 09:53 (`action steamvr_quit` stands in for the
  event): acknowledged once, the game ended by itself, exit code 0
  (`artifacts/hand-script/steamvr-quit/20261101-095354`).
- Control: the game's own `qqq` at the same mark, PASS 09:55, exit code 0,
  the same log ending (`qqq-control/20261101-095501`).
- Neither exit writes a line from OBVR's DLL_PROCESS_DETACH (tried and
  taken out again): a normal exit cannot be told from a kill by OBVR.log's
  last line.

**Not built, on purpose: `OpenVRBackend::Stop()` at exit.** The only place
OBVR sees the process leave is DLL_PROCESS_DETACH, under the loader lock.
Microsoft's "Dynamic-Link Library Best Practices" (learn.microsoft.com,
read 2026-11-01): never "Synchronize with other threads" in DllMain, and at
process exit "all the process's threads have been forcibly cleaned up ...
the ideal DLL_PROCESS_DETACH handler is empty" - VR_Shutdown talks to
vrserver and FreeLibrary unloads a DLL there. The measured normal exit does
not log from the detach at all. Calling it before the engine's quit instead
would leave the frames that still run with no runtime. The process leaving
closes the runtime's connection either way (derived, not measured in
SteamVR's own log). Stop() keeps no caller.

**Open.**
- The headset: close SteamVR (or quit from the dashboard) while the game
  runs - the game should end on its own within seconds, its log ending with
  "SteamVR quit: acknowledged, the game told to quit".
- Whether quitting asks to save: it does not; the engine's quit byte
  quits as qqq does. A save prompt would be a feature of its own.

## 2. The crash report

**What was wrong (read).** No exception filter, no minidump, no vectored
handler anywhere in src/ (the review's grep). A crash left OBVR.log ending
mid-sentence: no exception, no address. The tester's own crash logs came
from a crash logger plugin the public will mostly not have.

**Read on the tester's machine (2026-11-01).** Four other plugins import
SetUnhandledExceptionFilter: CobbCrashLogger (also AddVectoredExceptionHandler),
Modern Engine Fixes, GPEngineFixes, EngineBugFixes. Windows calls the last
filter set.

**Built.**
- core/CrashHandler.cpp: OBVR's filter, set on the first frame (after every
  plugin's load), keeping the one it replaced. On a crash it writes to
  OBVR.log the exception (core/CrashReport.h `DescribeException`: an access
  violation says reading, writing or executing which address), where it
  happened as module+offset, the thread, the frame's last step (the
  watchdog's), the registers, and the code addresses on the stack (the
  watchdog's `CollectCodeWords` and `NameCodeAddress`) - then calls the
  filter it replaced, so a crash logger still writes its report.
- Once only (`ReportGate`): a crash inside the report, or a chain that
  leads back to OBVR's filter, passes on at once.
- No minidump: MiniDumpWriteDump means loading dbghelp.dll inside a
  crashing process; the log lines are what a tester sends anyway.
- Unit flows: crash_report_test (every named code, the three access kinds
  and an unknown one, no parameters, a short buffer, no parameter array,
  the gate).
- The harness's crash: `action crash_test` writes to 00000010 in OBVR's
  frame; hand scripts run only with the harness's token.

**Measured (harness, Windows, 2026-11-01).** `crash-report.txt` PASS 10:03
(`artifacts/hand-script/crash-report/20261101-100255`):
- "Crash report: OBVR's filter set; the one before it OBSE_Elys_Pluggy.dlx+CC45"
- "CRASH: access violation writing 00000010 at OBVR.dll+27661 ... the
  frame's last step "poses are in"", the registers, the stack's code
  addresses (Oblivion.exe, ntdll.dll, OBVR.dll, the driver)
- the process's exit code C0000005; CobbCrashLogger wrote its own report
  as well (`Crash Logs\CrashLogger.2026-11-01-09-03-35.log`, the same
  OBVR.dll address in its call trace).

**Open.**
- A filter set after OBVR's first frame by another plugin that does not
  call the one it replaced would silence OBVR's report (none seen).
- A stack overflow leaves the filter little stack; not tested.

## 3. The handover, off until its cause is found

**What was wrong (read, docs/free-weapons-spec.md "Open bug, harness
2026-10-30").** With the fixes of DLL c7338b0f, handover-sword ended the
game right after the take back; handover-shield, -bow and -item failed;
only the torch passed. Causes not found. Beta 7 advertised the feature and
asked testers to try it.

**Decided (Nadi, 2026-11-01, the release review's prompt): off by default
until the cause is found.** The switch existed already: `[Hands]
HandoverReachMetres`, 0 off (game/HandoverLogic.h `HandOverAtLetGo` and
`StepHandBack` refuse a reach of 0, and the worn shield's take-off asks
reach > 0). The torch shares that reach, so it is off with the rest - a
separate switch for it would be a new setting for one path that passed
once.

**Built.**
- `handoverReachMetres` 0 in the code (vr/HandMode.h) and in the repo's
  OBVR.ini, its comment saying why and that 0.12 turns it on at one's own
  risk. A player's own OBVR.ini keeps the value it has.
- The held item's pass to the other hand (CameraHook `s_handBack` path)
  asks reach > 0 too: it compared `toThing <= reach` only, which a hand
  exactly on the thing's line would have met at 0.
- The let-go's log line says "the handover off ([Hands] HandoverReachMetres
  0), a drop" instead of "too far, a drop" when it is off.
- config_test: the default 0 (the thing's ball 0.15 unchanged), the key
  read when set.

**Measured (harness, Windows, 2026-11-01).** `handover-off.txt` PASS 10:10
(`artifacts/hand-script/handover-off/20261101-101022`): handover-sword's
moves with the default - the sword drawn, the left hand closed on its
blade, the weapon hand opened ("the handover off ..., a drop"), closed
again - nothing handed over, nothing taken back, the game ran to the end.
The run at 10:06 failed on the harness save's question box (Plunder Mod);
`console-at ready CloseAllMenus` closes it first.

**Open.** The cause of the handover's crash (free-weapons-spec's open bug
stays); the beta 8 text says the handover is off.

## 4. No second scene pass while the game loads

**What was wrong (read, docs/performance-spec.md "Open bug, 2026-10-27
13:54").** A crash while loading a save from inside a dungeon (Nadi,
Windows): an access violation in NiGeometry::OnVisible (0x00719BA0 ->
0x00722A10), a child pointer into freed memory - the scene culled and
rendered while the load replaced the world. OBVR hooks nothing there, but
its dual pass renders the scene a second time on every frame, loading
frames included (`request.dualEyes` and `WantsSecondScenePass` never asked
about a load), so every such frame was two renders over a world being torn
down. Not reproduced since (four harness scenarios on Linux); the cause
is derived, not proven - this gate removes OBVR's share, it cannot remove
the engine's own.

**Built.** `PassRungFor(rung, loadingFrame)` (camera/FrameLogic.h): a
loading frame - the game's loading thread, or the Loading menu on top, the
question StepWorld asks - runs on the single-pass rung (kProbeSinglePass,
the probe's rung 3, the known path that renders once and submits one
picture). `DualProbeRung()` returns it, so `WantsSecondScenePass` (the
scene hook's second pass), `DeliversDualEyes` (the camera pass's request)
and the menu stand-in's request (which promised two eyes unconditionally
before) all read the same rung and cannot disagree. Log: "Load: the game
loads - no second scene pass, one picture until it is done" / "Load: the
load is done - both scene passes again". The engine's own render stays as
in the vanilla game.

Unit flows: frame_logic_test `TestNoSecondPassWhileLoading` (the rung for
loading and other frames, the probe's own rungs, no frame shape with a
second pass while loading, one picture promised, both again after).

**Measured (harness, Windows, 2026-11-01).** `load-single-pass.txt` PASS
10:15 (`artifacts/hand-script/load-single-pass/20261101-101459`, dual
stereo): the main menu's load and a console load from the world each
logged the cut and its end, "Render: dual pass is live" after the first,
"Scene call ... (dual, rung 0)" after both, the game came through. Not
measured: whether the engine renders the world during these loads at all
(no scene call was logged between the two lines - the log names every
tenth call) - so what the gate saves per load is unmeasured.

**Open.**
- Nadi's crash itself: not reproduced before or after. If it comes back
  with this build, it is the engine's race alone (performance-spec's
  "fewer temp refs at load" or a render pause by a detour - medium,
  unmeasured).
- The headset: a load looks the same (the loading screen on the cinema
  screen); a frame of one flat picture at the load's edges would be the
  sign of the single-pass rung.

**Beta 7's crash fixes, none confirmed in a headset yet** (collected from
the specs for the beta 8 checklist):
- Pull a weapon to the hand, then holster or stow it (30159c3; the stow
  took a weapon the pull had already taken).
- A crash at some save loads (274a978; a reference remembered before a
  load read after it).
- A torch dropped, another equipped and holstered (controls-spec "A crash
  after torches dropped and stowed", 2026-10-26).
- The session's first enemy bar under Proton (hud-on-hands-spec, 2026-10-14).
- mcgizmoo's close two seconds after the start with his mods (controls-spec
  "Beta report of 2026-10-18": "his next start is the proof").

## 5. Feature freeze and beta 8

**Decided (the release review, 2026-11-01).** 54 commits since beta 7
(a229a30..2f2d426) passed the harness only, many of them default on: the
room-scale body, the analog walk and the sprint, the kick, the shield bash,
the torch block, the pommel strike, half-swording, dismemberment with its
co-save record. From beta 8 on nothing new turns on by default before a
headset has seen it; beta 8 is where those get that look.

**Built.**
- Beta 8 packaged on Windows: dist/OBVR-0.3.0-beta8.zip, 36 files, from
  2f2d426, the DLL the harness ran at 10:15 (SHA-256 1abc25e20b1d1eb8, the
  MSVC build, statically linked - KERNEL32, USER32 and WINMM only, as the
  clang-cl build).
- tools/package-release.ps1 brought up to package-release.py: `-Tag`, the
  loading stage's model check, and the zip written entry by entry with
  forward slashes - Windows PowerShell 5.1's Compress-Archive had named
  every entry `OBSE\Plugins\...` (read in the first beta 8 zip), which
  Linux's unzip takes as part of a file name.
- The post: docs/discord-hand-tracking-beta.md "The Discord post for beta 8",
  with the testers' checklist (SteamVR's quit, loads from a dungeon, the
  features since beta 7, beta 7's crash fixes still waiting). Not posted -
  Nadi posts.

**Open.** The headset look at everything on the checklist; the Linux build
and deploy of this state (no Python and no Linux toolchain in this Windows
session - tools/build-linux.sh deploys both game folders).
