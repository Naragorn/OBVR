#pragma once

#include "core/Types.h"

namespace obvr::game {

// GetMousePos for scripts while OBVR drives the menus (game/OsCursorLogic.h):
// xOBSE's import of user32's GetCursorPos is replaced, so a script reading
// the mouse in a menu reads the engine's interface cursor - where the laser
// or the drag put it - and not the desktop's resting mouse. Out of menus,
// and whenever the cursor cannot be read, the real GetCursorPos answers.
// False (and a log line) when xOBSE is not loaded or imports it otherwise.
bool InstallOsCursorAnswer();

}  // namespace obvr::game
