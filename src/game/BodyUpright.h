#pragma once

#include "game/BodyUprightLogic.h"
#include "game/BoneWrite.h"
#include "game/GameTypes.h"

namespace obvr::game {

// Stands the trunk's live pose upright as it is taken (game/BodyUprightLogic.h):
// `chain` is NonAccum, the pelvis, the spine's bones, the neck's and the head,
// `writes` their booking, `count` of each; `skeleton` the root (Bip01) the
// hips are put over; `bodyYaw` the way the body faces. The pelvis to the neck
// are turned about the body's forward and NonAccum moved across, through the
// booking, the world brought up to date after each. Runs once per take, not
// every frame. Answers what was done; logs it.
TrunkUpright StandTrunkUpright(NiAVObject* const* chain, BoneWrite* writes, UInt32 count, NiAVObject* skeleton,
                               float bodyYaw);

}  // namespace obvr::game
