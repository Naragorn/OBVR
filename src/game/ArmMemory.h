#pragma once

// What each arm of the body carries from frame to frame (game/BodyIk.cpp,
// SolveArm): the elbow's eased direction, which way round the torso it went,
// and the two rolls continued the short way (UnwrapAngle). Forgotten when the
// arm stops following, and when the mirror switches - the weapon handed to the
// other hand (free-weapons-spec 5.16, step 3): an arm that eased its elbow
// from the other side crossed the body over two frames
// (hand-side-switch-probe 2026-11-22, 73 and 72 degrees running).
//
// Pure, covered by body_ik_test.

#include "game/NiMath.h"

namespace obvr::game {

struct ArmMemory {
	bool valid = false;
	NiPoint3 pole{0.0f, 0.0f, -1.0f};
	float sideSign = 0.0f;
	float upperRoll = 0.0f;
	float foreRoll = 0.0f;  // the hand's roll on the forearm, continued (HoldForeRollMemory)
	bool mirrored = false;  // the mirror it was taken under
};

// After a frame's solve: what the next frame carries.
inline ArmMemory ArmMemoryAfter(bool mirrored, const NiPoint3& pole, float sideSign, float upperRoll, float foreRoll) {
	ArmMemory m;
	m.valid = true;
	m.mirrored = mirrored;
	m.pole = pole;
	m.sideSign = sideSign;
	m.upperRoll = upperRoll;
	m.foreRoll = foreRoll;
	return m;
}

// Before a frame's solve: nothing carried across a switched mirror.
inline void ForgetArmAcrossMirror(ArmMemory& m, bool mirrored) {
	if (m.valid && m.mirrored != mirrored) {
		m = ArmMemory{};
	}
}

}  // namespace obvr::game
