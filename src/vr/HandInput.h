#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// One hand as OBVR sees it: the controller's pose in the same tracking space
// the head is read in, and what it is pressing. Pure data, so the decisions
// below can be checked without a headset.
struct HandPose {
	bool valid = false;
	Quaternion orientation = Quaternion::Identity();
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	UInt64 buttonsPressed = 0;
	float trigger = 0.0f;  // 0 released, 1 pulled through
	float thumbX = 0.0f;
	float thumbY = 0.0f;
};

// Whether a button bit is down in a pressed mask. Bit positions are
// openvr's EVRButtonId values.
inline bool ButtonDown(UInt64 pressedMask, UInt32 button) {
	return (pressedMask >> button) & 1ull;
}

// The turn from one heading to another, as the shortest signed angle in
// radians. Both headings are angles the way HeadingOf and Atan2 give them;
// the difference is wrapped so a hand held just left of a head looking just
// right of the seam comes out as a small turn and not most of a circle.
inline float ShortestTurn(float fromRadians, float toRadians) {
	float turn = toRadians - fromRadians;
	while (turn > math::kPi) {
		turn -= 2.0f * math::kPi;
	}
	while (turn < -math::kPi) {
		turn += 2.0f * math::kPi;
	}
	return turn;
}

}  // namespace obvr::vr
