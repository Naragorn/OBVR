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

}  // namespace obvr::game
