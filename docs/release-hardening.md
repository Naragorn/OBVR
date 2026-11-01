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
