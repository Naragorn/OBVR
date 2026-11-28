#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

struct ShieldHoldNote;

// The left arm through any action of it (game/ArmDrawTraceLogic.h): called by
// the left hand's pin after it wrote the forearm (game/HandBones.cpp), with
// the hand bone, what the shield's hold saw (game/ShieldHold.h: the twist,
// the action, the animation's locals), the camera's turn, where the hand was
// pinned and the pins' frame number. Returns at once while the player's
// action is none; one log line when an action ends ("Arm draw trace:").
void TraceLeftArmInDraw(const NiAVObject* hand, const ShieldHoldNote& note, const NiMatrix33& cameraRot,
                        const NiMatrix33& handRot, const NiPoint3& handPos, UInt32 pinFrame);

}  // namespace obvr::game
