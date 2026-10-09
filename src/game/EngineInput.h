#pragma once

#include "core/Types.h"

namespace obvr::game {

// The controllers' keys straight into the game's input state, for while the
// game's window is behind another one (game/InputRoute.h says when).
//
// A detour at the entry of the poll's tail (addr::kInputPollTail) writes
// what the controllers hold into the state the device read just left - so
// the game, the double-click pass and xOBSE (its HoldKey, DisableKey and
// OnKeyEvent run after it) all see them the way they see a real key. Only
// while active: the rest of the time the detour passes straight through.

// Installs the detour; logs its outcome. False when the bytes are not the
// ones this was built for - another mod at the same entry - and then the
// controllers cannot reach a game in the background (InputRoute::Off).
bool InstallEngineInputHook();
bool EngineInputHookInstalled();

// Whether the overlay goes into the polls. Off also lets go of everything.
void SetEngineInputActive(bool active);

// Held or let go, by virtual key (the same key the Windows route sends).
void EngineHoldKey(UInt32 virtualKey, bool down);
bool EngineHoldsKey(UInt32 virtualKey);

// Movement and wheel for the next poll, added up until it takes them.
void EngineMoveMouse(SInt32 dx, SInt32 dy);
void EngineScrollWheel(SInt32 delta);

// Lets go of everything held and drops movement not yet delivered.
void EngineReleaseAll();

// Whether this process owns the window in front.
bool GameWindowInFront();

}  // namespace obvr::game
