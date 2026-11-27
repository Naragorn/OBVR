#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

// The left arm through a draw or a sheathe (game/ArmDrawTraceLogic.h): called
// by the left hand's pin after it wrote the forearm (game/HandBones.cpp), with
// the hand bone, the camera's turn, where the hand was pinned and the pins'
// frame number. Returns at once outside a draw or a sheathe; one log line when
// one ends ("Arm draw trace:").
void TraceLeftArmInDraw(const NiAVObject* hand, const NiMatrix33& cameraRot, const NiMatrix33& handRot,
                        const NiPoint3& handPos, UInt32 pinFrame);

}  // namespace obvr::game
