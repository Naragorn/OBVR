#pragma once

namespace obvr::log {

// Creates the log file next to Oblivion.exe. If that fails, everything still
// goes to the debugger, so the DLL stays usable.
void Open(const char* fileName);
void Close();

void Write(const char* format, ...);

}  // namespace obvr::log

#define OBVR_LOG(...) ::obvr::log::Write(__VA_ARGS__)
