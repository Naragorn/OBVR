#pragma once

// The script clock - see game/ScriptClockLogic.h for what the engine does
// and what is counted. [Debug] ScriptTimeTrace=1 detours the script run's
// entry at 0x00517330 the first frame it is on (never before: with the
// setting off the run is the engine's, untouched) and logs a "Script clock:"
// line for every frame whose scripts take [Debug] ScriptTimeTraceMs or
// longer, at most thirty in ten seconds, and a summary every ten seconds.
// Hot reloaded: switched off again, the detour stays and passes through.

namespace obvr::game {

// Once a frame, on the game's thread (OBVR's Present): installs the detour
// when the trace is first wanted, closes the frame's accounting and logs
// the lines due.
void StepScriptClock(bool enabled, float thresholdMs);

}  // namespace obvr::game
