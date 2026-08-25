#pragma once

namespace obvr::log {

// Legt die Logdatei neben Oblivion.exe an. Schlaegt das fehl, geht alles
// weiterhin an den Debugger, die DLL bleibt also nutzbar.
void Open(const char* fileName);
void Close();

void Write(const char* format, ...);

}  // namespace obvr::log

#define OBVR_LOG(...) ::obvr::log::Write(__VA_ARGS__)
