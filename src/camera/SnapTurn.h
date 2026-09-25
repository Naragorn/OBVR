#pragma once

#include "camera/LookControl.h"
#include "core/MathFns.h"

namespace obvr::camera {

// Snap turning: the right stick pushed past its dead zone turns the PLAYER by a
// fixed angle, once per push.
//
// The player, not the camera. The view is built on the player's heading every
// frame, so a turn has to go into rotZ to stay: turning only the camera heading
// is undone by the next frame, which rebuilds it from the game's. It is also the
// only way walking and aiming turn with the view - both go along rotZ.
//
// The step comes back in Oblivion's own convention, radians clockwise seen from
// above, so a positive step turns right and is ADDED to rotZ.
struct SnapTurnState {
	bool armed = true;       // the stick has been back near the centre since the last snap
	float remaining = 0.0f;  // of an eased snap, still to turn, signed
};

struct SnapTurnStep {
	float yawRadians = 0.0f;  // to add to rotZ this frame
	bool fired = false;       // a snap started this frame - for the vignette
	bool ownsStick = false;   // the stick's continuous turn is to be dropped
};

// The stick has to come back inside half the dead zone before the next snap:
// a thumb resting just at the edge would otherwise fire on every wobble.
constexpr float kSnapRearmShare = 0.5f;

// A frame longer than this is a hitch or a load, not time to ease through.
constexpr float kSnapLongestFrame = 0.1f;

inline float SnapClamp(float value, float low, float high, float fallback) {
	if (!(value == value)) {
		return fallback;
	}
	return value < low ? low : (value > high ? high : value);
}

// allowed: in the world, no menu, the player there to turn. Anything else
// cancels an eased snap and waits for the stick to come back before the next.
inline SnapTurnStep StepSnapTurn(SnapTurnState& state, float stickX, const LookSettings& s,
                                 float deltaSeconds, bool allowed) {
	SnapTurnStep out;
	if (!s.snapTurning) {
		state = SnapTurnState{};
		return out;
	}
	out.ownsStick = true;

	const float x = stickX == stickX ? stickX : 0.0f;
	const float deadZone = SnapClamp(s.snapTurnDeadZone, 0.05f, 0.95f, 0.3f);
	const float magnitude = x < 0.0f ? -x : x;

	if (!allowed) {
		state.remaining = 0.0f;
		state.armed = magnitude < deadZone * kSnapRearmShare;
		return out;
	}

	if (magnitude < deadZone * kSnapRearmShare) {
		state.armed = true;
	} else if (state.armed && magnitude >= deadZone) {
		state.armed = false;
		out.fired = true;
		const float angle = SnapClamp(s.snapTurnAngle, 1.0f, 180.0f, 45.0f) * math::kDegreesToRadians;
		const float turn = x > 0.0f ? angle : -angle;
		if (s.snapTurnInstant) {
			out.yawRadians = turn;
			return out;
		}
		// A second push while one is still easing adds to it rather than
		// throwing the rest away.
		state.remaining += turn;
	}

	if (state.remaining != 0.0f) {
		const bool dtValid = deltaSeconds > 0.0f && deltaSeconds <= kSnapLongestFrame;
		const float speed = SnapClamp(s.snapTurnSpeed, 0.5f, 100.0f, 18.0f);
		const float most = dtValid ? speed * deltaSeconds : 0.0f;
		const float left = state.remaining < 0.0f ? -state.remaining : state.remaining;
		const float step = left <= most ? left : most;
		const float signedStep = state.remaining > 0.0f ? step : -step;
		out.yawRadians += signedStep;
		state.remaining -= signedStep;
		if (left <= most) {
			state.remaining = 0.0f;
		}
	}
	return out;
}

}  // namespace obvr::camera
