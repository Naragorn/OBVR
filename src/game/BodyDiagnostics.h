#pragma once

// The body IK's diagnostics that read the drawn tree (game/BodyIk.cpp):
// Debug.BodyTrace's drawn arms and the hand script's bounds walk.

#include "game/GameTypes.h"

namespace obvr::game {

// After a traced frame's draw: how far each arm bone turned in the root's
// frame since the frame drawn before - per side the body's upper arm, its
// twist, forearm, its twist, hand, then the first-person hand; "h" after a
// bone hidden or collapsed. One "Body drawn" line (tools/drawn-arm-flicker.py):
// what was seen, not what the animation or the IK step left. `arm` holds the
// side's five body bones in that order.
void LogDrawnArms(const NiAVObject* root, const NiAVObject* const* rightArm, const NiAVObject* const* leftArm,
                  const NiAVObject* rightFpHand, const NiAVObject* leftFpHand, UInt32 frame);

// Once per `reported`: of the bones written last frame, how many the engine
// left holding the writes (put back to the animation's pose, named) and how
// many it had renewed.
void ReportKept(const char* what, UInt32 kept, UInt32 renewed, const bool* restored, NiAVObject* const* nodes,
                UInt32 count, bool& reported);

// Every leaf shape under `root` (and the root) with its bound against the
// camera, 48 lines at most ("Body bounds").
void WalkBodyBounds(const NiAVObject* root, const NiPoint3& camera, const char* mark);

}  // namespace obvr::game
