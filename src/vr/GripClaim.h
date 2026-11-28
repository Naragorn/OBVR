#pragma once

// Who gets a closing grip where the tablet's open gesture and a handover
// overlap (docs/inventory-tablet-spec.md 30.3, free-weapons-spec 5.16).
//
// The gesture closes both grips with the hands together at the inventory
// ring; the shield's handover (camera/hands/Handover.cpp) fires on the weapon
// hand closing near the other hand - the same move. In the run of 2026-11-28
// it switched the sides during the gesture (OBVR.log 9326-9329), so the
// tablet could land in the weapon hand. Decided 2026-11-28: where both claim
// the grips, the gesture has the first claim.
//
// - In the gesture's zone (vr::TabletScrollResult::zone: the tablet on, no
//   menu, both hands empty, the controllers together at the ring) a closing
//   the handover wants waits up to the gesture's window for the second grip
//   (TabletScrollLimits::pairSeconds).
// - The gesture begins in that time: it is the gesture's, the handover is
//   dropped, and no handover comes from these grips until both are open again.
// - The window passes without the gesture: the handover, late by the window.
// - Outside the zone - and with the tablet or its gesture off, which keeps the
//   hands out of it - the handover at once, as before.
// - No longer usable (a menu up, the hand mode off) while waiting: dropped, as
//   a closing under a menu is.
//
// Pure; covered by grip_claim_test.

#include "core/Types.h"

namespace obvr::vr {

enum class GripClaimer : UInt8 { Nobody, Gesture, HandoverNow, HandoverAfterWait };

struct GripClaimFrame {
	bool usable = false;          // the handover may act this frame (the hand mode on, no menu up)
	bool zone = false;            // the hands where the gesture can begin (TabletScrollResult::zone)
	bool gestureBegan = false;    // the gesture began this frame (TabletScrollResult::began)
	bool gripsOpen = false;       // both grips open (TabletScrollResult::gripsOpen)
	bool handoverWanted = false;  // a closing the handover would act on, this frame
	float dt = 0.0f;
};

struct GripClaimState {
	float waited = -1.0f;  // seconds a wanted handover has waited; < 0 none
	bool claimed = false;  // the gesture took the grips: no handover till both are open
};

inline GripClaimer StepGripClaim(GripClaimState& s, const GripClaimFrame& f, float windowSeconds) {
	if (f.gestureBegan) {
		s.claimed = true;
		s.waited = -1.0f;
		return GripClaimer::Gesture;
	}
	if (s.claimed) {
		if (!f.gripsOpen) {
			return GripClaimer::Nobody;
		}
		s.claimed = false;
	}
	if (!f.usable) {
		s.waited = -1.0f;
		return GripClaimer::Nobody;
	}
	if (s.waited >= 0.0f) {
		s.waited += f.dt > 0.0f ? f.dt : 0.0f;
		if (s.waited >= windowSeconds) {
			s.waited = -1.0f;
			return GripClaimer::HandoverAfterWait;
		}
		return GripClaimer::Nobody;  // one wait, whatever closes meanwhile
	}
	if (!f.handoverWanted) {
		return GripClaimer::Nobody;
	}
	if (f.zone) {
		s.waited = 0.0f;
		return GripClaimer::Nobody;
	}
	return GripClaimer::HandoverNow;
}

}  // namespace obvr::vr
