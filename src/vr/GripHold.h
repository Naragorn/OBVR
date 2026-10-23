#pragma once

// A hand's hold: the fingers closed round the controller's grip, read from
// the grip's pull - an Index's capacitive sensing, a Touch's grip trigger
// (HandPose::gripPull, the action path's grip_pull) - with a hysteresis:
// closed from GripCloseAt up, open again from GripOpenAt down, a hand in
// between keeps what it was. The holsters draw and sheathe on it since
// 2026-10-23 (the tester: a realistic grip is to count, not the binding's
// force click, which wants a squeeze). Measured on the tester's two Index
// controllers (docs/free-weapons-spec.md 5.4): at rest in the strap the pull
// reads 0.0 to 0.1, with the fingers closed round the grip 1.0 steady at a
// force of 0.0 to 0.2 and no click, the click only from a force of 0.3 - so
// 0.8 and 0.4 sit far from both and across the transition. Where no pull
// reads (the legacy input, a binding without the action, the setting off)
// the click holds instead, edge for edge. Pure, covered by grip_hold_test.

#include "core/Types.h"
#include "vr/ControllerActions.h"
#include "vr/HandInput.h"

namespace obvr::vr {

inline constexpr float kGripCloseDefault = 0.8f;
inline constexpr float kGripOpenGap = 0.2f;

struct GripHoldSettings {
	bool byPull = true;    // [Hands] GripByPull: 0 puts the holsters back on the click
	float closeAt = 0.8f;  // [Hands] GripCloseAt: the pull from which the hand is closed
	float openAt = 0.4f;   // [Hands] GripOpenAt: the pull down to which it is open again
};

// The close limit as used: the setting when it is a number in (0, 1], else
// the default.
inline float GripCloseLimit(const GripHoldSettings& s) {
	const float close = s.closeAt;
	return close == close && close > 0.0f && close <= 1.0f ? close : kGripCloseDefault;
}

// The open limit as used: the setting when it is a number below the close
// limit, else kGripOpenGap below it; never under 0.
inline float GripOpenLimit(const GripHoldSettings& s) {
	const float close = GripCloseLimit(s);
	float open = s.openAt;
	if (!(open == open) || open >= close) {
		open = close - kGripOpenGap;
	}
	return open > 0.0f ? open : 0.0f;
}

struct GripHoldState {
	bool held = false;
};

struct GripHoldInput {
	bool valid = false;      // the hand is tracked
	bool click = false;      // the grip's click (GripDown)
	bool pullKnown = false;  // the pull reads (GripPullKnown)
	float pull = 0.0f;       // HandPose::gripPull
};

inline bool StepGripHold(GripHoldState& s, const GripHoldInput& in, const GripHoldSettings& settings) {
	if (!in.valid) {
		s.held = false;
		return false;
	}
	if (!settings.byPull || !in.pullKnown || !(in.pull == in.pull)) {
		s.held = in.click;
		return s.held;
	}
	if (s.held) {
		s.held = !(in.pull <= GripOpenLimit(settings));
	} else {
		s.held = in.pull >= GripCloseLimit(settings);
	}
	return s.held;
}

// Whether a hand's pull reads at all: the action path with the pull's action
// active. The legacy path reads no pull; a binding without the action, or a
// controller without the signal, leaves it inactive.
inline bool GripPullKnown(const HandPose& hand) {
	return hand.valid && hand.actionInput && (hand.actionActiveMask & (1u << input::GripPull)) != 0;
}

}  // namespace obvr::vr
