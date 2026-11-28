#pragma once

// The shield bash's approach in the game (game/BashApproachLogic.h): called
// by CameraHook's StepShieldBash once a frame it runs, it keeps the shield's
// last frames (the controller's velocity, the drawn shield's middle, the
// time) and gives the bash's speed: the fastest towards the one whose column
// the shield's middle is in, over the last kBashApproachSeconds. One clock read
// and one ring write a frame; the walk only with someone in the column.

#include "game/NiMath.h"

namespace obvr::camera::hands {

// Adds this frame and returns the approach towards centre in m/s, or 0 with
// nobody in the column (actor null).
float ShieldBashApproach(const NiPoint3& velocity, const NiPoint3& at, const void* actor, const NiPoint3& centre);

// The approach of the frames kept so far towards centre, for the attempt's
// line (camera/hands/ShieldBashTrace.cpp): someone near, not in the column.
float ShieldBashApproachTowards(const NiPoint3& centre);

}  // namespace obvr::camera::hands
