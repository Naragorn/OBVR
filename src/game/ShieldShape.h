#pragma once

#include "core/Types.h"
#include "game/HandContactLogic.h"
#include "game/NiMath.h"

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

// The worn shield's disc (HandContactLogic.h, ShieldDisc), read from the
// model under the left forearm's twist bone ("Bip01 L ForearmTwist", which
// the shield hangs on - camera/FrameLogic.h) in the frame of the hand pose
// given (world from the hand: `handRot`, `handPos`) - the pose the bones were
// pinned to, as the hand's own span is read. Two walks over the model's
// vertices (game/NearbyItems.h, ForEachWorldVertex): the scatter for the
// disc's axes, then the reach along them. False, with `why`, when the node
// has no geometry this can read or what it carries is no shield's disc; the
// caller then takes the bound's ball.
bool ReadShieldDisc(const NiAVObject* node, const NiMatrix33& handRot, const NiPoint3& handPos, ShieldDisc& out,
                    const char** why);

}  // namespace obvr::game
