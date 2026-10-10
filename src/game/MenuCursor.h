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

// The engine's own cursor, where the engine itself last put it - not the
// pixel written for the search. A drag (a slider, a scroll bar's marker, the
// map) moves with the engine's own cursor, which only the mouse steps walk
// (the tester, 2026-10-10: "slider kann man nicht ziehen. sidebar scrolls
// auch nicht. die map im Tab menü auch nicht"), so the steps are reckoned
// from here.
//
// The engine writes the two floats only when its own cursor moves (the
// cursor update 0x0057E7C0 sets them, and the moved flag +0xB9, only when the
// sprite's position changed), so on a frame without a move they still hold
// the pixel written last. Taken in the search's entry, before the write:
// what is there is the engine's own unless it is exactly the pixel written
// last (CursorReadIsEngines). False until the engine's own has been seen.
bool EngineMenuCursor(float& x, float& y);

// Pure (menu_cursor_test): whether the two floats read in the search's entry
// are the engine's own - nothing written yet, or not the pixel written last -
// and a place on a screen at all (game::InterfaceCursorPosition's bounds; a
// NaN fails every comparison).
inline bool CursorReadIsEngines(bool wroteBefore, float lastX, float lastY, float readX, float readY) {
	const bool onAScreen = readX >= -64.0f && readX <= 16384.0f && readY >= -64.0f && readY <= 16384.0f;
	return onAScreen && (!wroteBefore || readX != lastX || readY != lastY);
}

}  // namespace obvr::game
