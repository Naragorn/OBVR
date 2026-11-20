#pragma once

// The tap's tip from the drawn index finger (docs/inventory-tablet-spec.md
// task 6a): with the hand seen on the tablet's glass, the finger that taps
// should be the one the player sees, not a point 8 cm ahead of the
// controller ([Hands] PokeTipForward). Each frame the drawn index finger's
// last two joints ("Bip01 R Finger11" and "Finger12", the same for L) are
// read after the fingers are posed; the tip lies past the last joint by
// kIndexTipPastLastJoint of the link before it, and is kept in its
// controller's own frame (x right, y up, -z forward, metres), so the hand
// mode can put it on the controller's pose of its own frame. Only while the
// drawn fingers follow the controller's (game::FingerPose::Tracked): a hand
// closed round a weapon or posed by the animation has no finger out to see.
// Pure; covered by tablet_touch_test and menu_place_test.

#include "vr/HandInput.h"

namespace obvr::vr {

// The last link as a share of the one before: a guess from the human finger
// (the distal phalanx about 0.7-0.8 of the middle one); the headset decides.
inline constexpr float kIndexTipPastLastJoint = 0.8f;
// Farther from its controller than this the tip is a misread, not a finger.
inline constexpr float kIndexTipReachMetres = 0.3f;
// The share of each new reading taken: the bones are read at the end of the
// frame's posing, the controller's pose used with them a frame on.
inline constexpr float kIndexTipEase = 0.3f;

struct IndexTip {
	bool valid = false;
	NiPoint3 local{0.0f, 0.0f, 0.0f};  // metres, the controller's frame
};

inline NiPoint3 IndexTipBeyond(const NiPoint3& middleJoint, const NiPoint3& lastJoint) {
	return lastJoint + (lastJoint - middleJoint) * kIndexTipPastLastJoint;
}

inline void StepIndexTip(IndexTip& tip, bool fingerFollows, const NiPoint3& local) {
	if (!fingerFollows || local.LengthSquared() > kIndexTipReachMetres * kIndexTipReachMetres) {
		tip = IndexTip{};
		return;
	}
	if (!tip.valid) {
		tip.valid = true;
		tip.local = local;
		return;
	}
	tip.local = tip.local + (local - tip.local) * kIndexTipEase;
}

// The tip that taps, in tracking space: the drawn finger's when it is known,
// else the controller's tip carried forward along the beam.
inline NiPoint3 TapTip(const HandPose& hand, const NiPoint3& pointing, float tipForward, const IndexTip& finger) {
	return finger.valid ? hand.position + Rotate(hand.orientation, finger.local)
	                    : hand.position + pointing * tipForward;
}

}  // namespace obvr::vr
