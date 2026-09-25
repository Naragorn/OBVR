#pragma once

#include "core/Types.h"

// Asks GitHub once, on a thread of its own, which OBVR release is the latest
// (GET https://api.github.com/repos/Naragorn/OBVR/releases/latest), so the
// main menu can say when a newer one is out. Nothing is sent but the request
// itself: no identifiers, no settings, no telemetry. Switched off with
// [Updates] CheckForUpdates=0.
//
// winhttp.dll is loaded at run time, so a system without it - or without a
// network - only costs one log line.
namespace obvr::platform {

// Starts the one request. Returns at once; further calls do nothing.
void StartUpdateCheck();

// Once the request has come back with a readable tag: copies it (e.g. "v0.2.3")
// and returns true. False while it is still running, or when it failed.
bool LatestReleaseTag(char* out, UInt32 capacity);

}  // namespace obvr::platform
