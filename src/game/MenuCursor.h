#pragma once

#include "core/Types.h"

namespace obvr::game {

// The laser places the game's menu cursor instead of steering it
// (docs/main-menu-laser-analysis.md, 1; the tester, 2026-10-10: the main
// menu's buttons "bisher nicht sehr robust").
//
// The engine decides the hover highlight, the active tile and with them
// what a click lands on in its tile search (addr::kFindTileAtCursor,
// thiscall on the InterfaceManager with one dword argument, called once
// per interface update at 0x00582406), which reads the cursor as two floats
// of the manager, +0x2C and +0x34 (addr::kInterfaceCursorXOffset and
// kInterfaceCursorYOffset), in the pixels of the believed screen size.
// Until now OBVR walked those floats by relative mouse steps the engine
// integrated itself - a step cap, a gain, the window's focus and the
// engine's own mouse scale all between the beam and the cursor. Now the
// search's entry is detoured: with a laser on a menu this frame, the two
// floats are set to the laser's own pixel right before the search reads
// them, every frame, and the engine's own integration of the mouse never
// matters while the laser points. Without a laser on a menu the search
// runs untouched and the mouse is the cursor as in vanilla.

// Detours the search's entry; logs its outcome. False when the bytes are
// not the ones read (another mod there) - then the laser keeps steering.
bool InstallMenuCursorHook();
bool MenuCursorHookInstalled();

// The pixel the laser wants the cursor at this frame (the believed-size
// space game::InterfaceCursorPosition reads in), or none.
void SetMenuCursorWanted(bool wanted, float x, float y);

}  // namespace obvr::game
