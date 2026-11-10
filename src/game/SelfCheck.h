#pragma once

#include "core/SelfCheckLogic.h"
#include "core/Types.h"
#include "obse/PluginInterface.h"

namespace obvr::game {

// The install self-check (core/SelfCheckLogic.h decides, docs/self-check-spec.md
// has every check and its evidence). An xOBSE task gathers the facts from the
// game's own loop - the files once, the device and the controllers until they
// are known - and writes "Self-check: <name> ok|PROBLEM|HINT|could not verify
// - ..." to OBVR.log as each check gets its answer. The native menus show the
// problems in a panel in the main menu (game/NativeMenuPrototype.cpp).

// From OBSE's Query: the Oblivion version it reports.
void NoteSelfCheckObse(UInt32 oblivionVersion);

// At load: registers the task. Without xOBSE 22's task interface nothing is
// checked, and that is said once.
void InstallSelfCheck(const obse::Interface* api);

// What SteamVR names (the controllers, the headset) comes in through
// core/SelfCheckNotes.h.

// The panel's lines when it should show: every check but the controllers'
// answered, something wrong, and [SelfCheck] ShowPanel on. False otherwise.
bool SelfCheckPanel(char (&lines)[selfcheck::kPanelLines][selfcheck::kPanelLineChars], UInt32& count);

}  // namespace obvr::game
