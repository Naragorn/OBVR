#pragma once

#include "vr/MenuSurface.h"

namespace obvr::vr::menu {

enum class ScrollOrientation { Off, Horizontal, Vertical };

struct ScrollLimits {
	float closedDistance = 0.08f;
	float poseDistance = 0.18f;
	float fullDistance = 0.60f;
	float dwellSeconds = 0.35f;
	float axisCosine = 0.819152f; // cos(35 degrees)
	float forwardCosine = 0.906308f; // cos(25 degrees)
	float minimumForward = 0.15f;
	float maximumForward = 0.75f;
	float minimumHeight = -0.65f;
	float maximumHeight = 0.10f;
};

inline bool Valid(const ScrollLimits& t) {
	return Finite(t.closedDistance) && Finite(t.poseDistance) && Finite(t.fullDistance) &&
	       t.closedDistance > 0 && t.poseDistance > t.closedDistance &&
	       t.fullDistance > t.poseDistance && Finite(t.dwellSeconds) && t.dwellSeconds > 0 &&
	       Finite(t.axisCosine) && t.axisCosine > 0 && t.axisCosine <= 1 &&
	       Finite(t.forwardCosine) && t.forwardCosine > 0 && t.forwardCosine <= 1 &&
	       Finite(t.minimumForward) && Finite(t.maximumForward) && t.minimumForward > 0 &&
	       t.maximumForward > t.minimumForward && Finite(t.minimumHeight) &&
	       Finite(t.maximumHeight) && t.maximumHeight > t.minimumHeight;
}

struct Opening {
	bool valid = false;
	float amount = 0;
	bool fullyOpen = false;
};

inline Opening MapOpening(float distance, const ScrollLimits& t = {}) {
	if (!Valid(t) || !Finite(distance) || distance < 0) return {};
	// Check physical distance independently: float division can round a value
	// just below fullDistance up to 1. Interaction must still be refused there.
	return {true, Clamp01((distance - t.closedDistance) / (t.fullDistance - t.closedDistance)),
	        distance >= t.fullDistance};
}

struct GestureHand {
	bool tracked = false;
	NiPoint3 position{0, 0, 0};
	NiPoint3 forward{0, 0, -1};
	NiPoint3 up{0, 1, 0};
};

inline bool Valid(const GestureHand& h) {
	const float perpendicular = Dot(h.forward, h.up);
	return h.tracked && Finite(h.position) && Unit(h.forward) && Unit(h.up) &&
	       perpendicular >= -0.001f && perpendicular <= 0.001f;
}

struct GestureFrame {
	GestureHand left;
	GestureHand right;
	GestureHand head;
	bool eligible = false; // Full VR, in world, focused, no foreign menu.
	bool controlsNeutral = false; // Includes BOTH grips, triggers and sticks.
	float dt = 0;
};

inline bool ScrollPose(const GestureFrame& f, ScrollOrientation orientation,
                       const ScrollLimits& t = {}) {
	if ((orientation != ScrollOrientation::Horizontal && orientation != ScrollOrientation::Vertical) ||
	    !Valid(t) || !f.eligible || !Valid(f.head) || !Valid(f.left) || !Valid(f.right)) return false;
	const NiPoint3 separation = f.right.position - f.left.position;
	const float distance = math::Sqrt(separation.LengthSquared());
	if (!Finite(distance) || distance < t.closedDistance || distance > t.poseDistance) return false;
	const NiPoint3 axis = orientation == ScrollOrientation::Horizontal
	    ? Cross(f.head.forward, f.head.up) : NiPoint3{0, 1, 0};
	float aligned = Dot(separation, axis) / distance;
	if (orientation == ScrollOrientation::Vertical && aligned < 0) aligned = -aligned;
	if (aligned < t.axisCosine || Dot(f.left.forward, f.right.forward) < t.forwardCosine ||
	    Dot(f.left.up, f.right.up) < t.forwardCosine) return false;
	// Both hands, not just their midpoint, must be in the deliberate pose zone.
	for (int side = 0; side < 2; ++side) {
		const GestureHand* hand = side == 0 ? &f.left : &f.right;
		const NiPoint3 relative = hand->position - f.head.position;
		const float forward = Dot(relative, f.head.forward);
		const float height = relative.y;
		if (forward < t.minimumForward || forward > t.maximumForward ||
		    height < t.minimumHeight || height > t.maximumHeight ||
		    Dot(hand->forward, f.head.forward) < t.forwardCosine) return false;
	}
	return true;
}

struct ReadyState { float dwell = 0; bool ready = false; };
struct ReadyResult { bool ready = false; bool entered = false; };

// The ready detector never captures gameplay input or opens a native menu.
// A missing/long frame is not evidence of a deliberately held pose.
inline ReadyResult StepReady(ReadyState& state, const GestureFrame& f,
                             ScrollOrientation orientation, const ScrollLimits& t = {}) {
	if (!f.controlsNeutral || !ScrollPose(f, orientation, t) ||
	    !Finite(f.dt) || f.dt <= 0 || f.dt > 0.1f) {
		state = {};
		return {};
	}
	const bool wasReady = state.ready;
	state.dwell += f.dt;
	if (state.dwell >= t.dwellSeconds) {
		state.dwell = t.dwellSeconds;
		state.ready = true;
	}
	return {state.ready, state.ready && !wasReady};
}

} // namespace obvr::vr::menu
