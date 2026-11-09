#pragma once

#include "obse/PluginInterface.h"

namespace obvr::game {

// A script line run the way the console runs it, from the game's own loop:
// xOBSE's console interface (RunScriptLine2) called from an xOBSE task, one
// frame after it was asked for. For the few things only a script command
// reaches - the bow's denock (vr/Archery.h: player.playgroup unequip, then
// idle, as the DenockArrow mods do, github.com/dannywarren/
// Oblivion-DenockArrowToo src/DenockArrowScript.txt).
//
// Installed at load; without xOBSE 22's interfaces a request is refused
// (false) and logged once.
void InstallConsoleLine(const obse::Interface* api);

// Queues a line (at most 95 characters, a few at a time). False when it
// cannot be run: no console interface, too long, or the queue full.
bool RequestConsoleLine(const char* line);

// The same, run as `ref` (xOBSE's calling reference: "playSound3D X" plays
// at it, "pickIdle" picks for it) - the slap's sound and the slapped one's
// idle (game/Shove.h). 0 runs it as no one.
bool RequestConsoleLineAs(UInt32 ref, const char* line);

// Runs the line at once, on the calling thread, as the native menus do
// (game/NativeMenuPrototype.cpp Run) - for a caller on the game's thread
// while xOBSE's tasks stand still: the message box a load raises ("This save
// relies on content ...") spins its own loop, and four queued lines stayed
// in the queue until it was full (harness 2026-11-09, run 20261109-084430).
// False without the interface, or when the interface answers false.
bool RunConsoleLineNow(const char* line);

}  // namespace obvr::game
