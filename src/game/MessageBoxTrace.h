#pragma once

#include "core/Types.h"

namespace obvr::game {

// Every engine message box's text and button labels in the log, as it is
// built (the builder 0x005BC8B0; game/MessageBoxLogic.h;
// docs/container-touch-spec.md 8.3): a purchase that ended in a box no log
// said is the case it is for. Nothing in the box is changed; budgeted.
// False when the entry's bytes are not the ones read (another mod there).
bool InstallMessageBoxTrace();

}  // namespace obvr::game
