#pragma once

#include "core/Types.h"

namespace obvr::game {

// [Debug] FlameTrace: every direct call of the engine's flames-off
// (0x004DE1C0, cdecl(NiNode*) - game/TorchFire.h) goes through OBVR, which
// logs who called it (the call site) and on which node, then calls it as
// before. Measures which engine path puts a dropped torch's fire out
// (free-weapons-spec "The thrown torch's flame"). Installed once; the sites
// are checked to hold `call 0x004DE1C0` first and left alone otherwise.
void InstallFlameOffTrace();

}  // namespace obvr::game
