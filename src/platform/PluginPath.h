#pragma once

#include "core/Types.h"

namespace obvr::platform {

// Builds absolute paths for OBVR's own files.
//
// There are two anchors, and which one is used matters for Mod Organizer 2.
//
// MO2 virtualises the game's Data folder and nothing else. Its own issue
// tracker puts it plainly: USVFS is loaded after the load-time DLLs, so it is
// too late for anything in the game root, which is why the separate Root
// Builder plugin exists to copy such files into place for real.
//
// So a file next to Oblivion.exe cannot be delivered as part of an MO2 mod,
// while a file next to OBVR.dll - which lives in Data/OBSE/Plugins - can.
// Anchoring on the plugin is therefore the MO2-friendly choice, and OBVR
// prefers it wherever it has a say.

// Remembers OBVR.dll's own module handle. Called from DllMain, which is the
// only place the handle is handed to us.
void SetPluginModule(void* module);

// "<directory of OBVR.dll>\<fileName>", so Data/OBSE/Plugins/<fileName>.
// Fully virtualised by MO2.
//
// Returns false before SetPluginModule has run, or if the path does not fit.
bool BuildPluginPath(const char* fileName, char* out, UInt32 outSize);

// "<directory of Oblivion.exe>\<fileName>", the game root. Not virtualised by
// MO2, so a file here is invisible to the mod manager - fine for output such
// as a log, wrong for anything a mod wants to ship.
bool BuildGamePath(const char* fileName, char* out, UInt32 outSize);

}  // namespace obvr::platform
