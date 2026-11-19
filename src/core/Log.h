#pragma once

namespace obvr::log {

// Creates the log file next to Oblivion.exe. If that fails, everything still
// goes to the debugger, so the DLL stays usable.
void Open(const char* fileName);
void Close();

void Write(const char* format, ...);

// The performance counter's ticks the lines written since the last call took
// (formatting, OutputDebugStringA, WriteFile), on every thread, and the
// OutputDebugStringA share of them; both back to 0. For the profiler's
// log_write span (perf/Profiler.cpp, OnPresentEnd).
void TakeWriteTicks(unsigned long long& total, unsigned long long& debugString);

}  // namespace obvr::log

#define OBVR_LOG(...) ::obvr::log::Write(__VA_ARGS__)
