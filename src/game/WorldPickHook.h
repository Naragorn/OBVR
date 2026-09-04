#pragma once

#include "game/NiMath.h"

namespace obvr::game {

// Installs the narrow mid-function hook that replaces Oblivion's activation
// ray in third person. False means the executable bytes did not match or the
// trampoline could not be installed; vanilla picking is then left untouched.
bool InstallWorldPickHook();

// Supplies the most recently rendered HMD direction. The world-pick update
// may run before the current frame's camera hook, so retaining the previous
// complete pose is intentional and avoids an uninitialised first frame. Its
// origin remains Oblivion's player-safe activation origin, not the chase
// camera behind the character.
void SetWorldPickDirection(const NiPoint3& direction, bool valid);

}  // namespace obvr::game
