#pragma once

#include "core/Types.h"

namespace obvr::watchdog {

// The hang watchdog: a thread that watches the frame counter and, when the
// frames stand still, writes which step the render thread reached last.
//
// A hang leaves no dump and no exception, only a log that stops. The step
// marks around the blocking calls (see TraceStep in CameraHook.cpp) already
// say where a frame was when it did not return - but only while they are
// armed, and only if the hang falls inside that window. This keeps the last
// mark always and reports it from a thread the hang cannot take with it.
//
// The report is a log line and nothing else: the watchdog does not try to
// unblock anything, because the one call that can wait for ever is the
// compositor's, and there is nothing to do about that from inside the game.

// Records the step the render thread just reached. Pointers to string
// literals only - the thread reads the pointer without copying.
void NoteStep(const char* step);

// Counts a frame end. Called once per Present.
void NoteFrame();

// The step last noted (NoteStep), for the crash report (core/CrashHandler.h).
const char* LastStep();

// "module+offset" for an address (the bare address outside every module),
// into `out`, always terminated; and whether a word lies in a module. The
// watchdog's own, for the crash report too.
void NameCodeAddress(UInt32 address, char* out, UInt32 size);
bool IsModuleAddress(UInt32 word);

// Starts the thread once; later calls do nothing. Safe to call every frame.
void Start();

}  // namespace obvr::watchdog
