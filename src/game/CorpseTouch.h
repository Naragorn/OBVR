#pragma once

// A hand inside a dead body (vr/WalkersDisguise.h, Oblivion Walkers' disguise):
// the loaded dead actors near the hands (game/Dismember.h CollectNecks: Spine2
// and the head of each) and which of them each hand is in (vr::HandInBody).

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// For each hand (0 left, 1 right; `valid` false skips it): the form id of the
// dead body it is inside, 0 for none. `reach` the units round the trunk.
void DeadBodiesUnderHands(const NiPoint3 hands[2], const bool valid[2], float reach, UInt32 out[2]);

}  // namespace obvr::game
