#pragma once

// Where a hand is drawn: pure geometry the hand features share.
//
// Moved out of camera/CameraHook.cpp (DrawnHandTracking) on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.7), unchanged. Pure.

#include "game/NiMath.h"
#include "vr/HandInput.h"
#include "vr/Stow.h"
#include "vr/HandMode.h"

namespace obvr::camera::hands {

// Where a hand is drawn, in tracking space: its controller plus the grip the
// hand sits at (vr::DrawnHandInTracking) - the shared grip and the hand's
// own. What the stow zone and the stow spot's placing measure since
// 2026-10-18; before, the controller, which a tester's grip of 0.23 m back
// put 0.40 m out while his drawn hand touched the chest.
inline NiPoint3 DrawnHandTracking(const vr::HandPose& hand, bool right, const vr::HandSettings& hands) {
	const NiPoint3 own = right ? NiPoint3{hands.rightHandGripX, hands.rightHandGripY, hands.rightHandGripZ}
	                           : NiPoint3{hands.leftHandGripX, hands.leftHandGripY, hands.leftHandGripZ};
	return vr::DrawnHandInTracking(
		hand.orientation, hand.position,
		NiPoint3{own.x, own.y + hands.handGripForwardMetres, own.z + hands.handGripUpMetres});
}

}  // namespace obvr::camera::hands
