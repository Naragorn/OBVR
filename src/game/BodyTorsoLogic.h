#pragma once

// The torso after the head (moved out of game/BodyIkLogic.h, 2026-11-10).
//
// The body faces where the player does. Nothing tracks the chest, so it is
// taken from the head: the head may turn `freeRadians` either way before it
// drags the torso along, and a turn held is followed, the torso closing on
// the head with a time constant of `followSeconds`. Kept in the headset's own
// tracking space - its yaw from the recenter - so a snap turn, which turns the
// world and not the head, and the game turning the player under a still view
// (the walk direction) leave the torso where it is relative to the head. A
// jump of the head's yaw no head can make in one frame (a recenter, a
// tracking glitch) starts the torso afresh on the head.
//
// Standing, a look aside no longer turns the body at once (Nadi in the
// headset, 2026-11-10: turning the head to look left or right should not move
// the body straight away). Until 2026-11-10 the torso closed on the head
// every frame, so a look of 20 degrees turned the body after it within a
// second. Now, while the player stands (no walk, no step through the room),
// the torso is followed only once the head has been more than
// kTorsoStandHoldRadians off it for kTorsoStandHoldSeconds; a glance, or a
// look that comes back, leaves the body where it is. Past `freeRadians` the
// head still drags it at once. Walking, it follows as before.
//
// On a horse the body faces the horse: the head turns alone (TorsoTurnWanted;
// Nadi, 2026-11-10: looking aside on horseback turned the body on the saddle
// and it went through the horse).
//
// Pure, covered by body_ik_test.

#include "core/MathFns.h"

namespace obvr::game {

inline constexpr float kTorsoJumpRadians = 30.0f * math::kDegreesToRadians;
inline constexpr float kTorsoStandHoldRadians = 15.0f * math::kDegreesToRadians;
inline constexpr float kTorsoStandHoldSeconds = 1.0f;
inline constexpr float kTorsoStandSettledRadians = 2.0f * math::kDegreesToRadians;

struct TorsoYawSettings {
	float freeRadians = 40.0f * math::kDegreesToRadians;
	float followSeconds = 1.0f;
};

// [Body] TorsoFollowDegrees and TorsoFollowSeconds, held to sense: 0-120
// degrees, 0 (no lag) to 10 s; out of range or not a number, the defaults.
inline TorsoYawSettings TorsoYawFrom(float freeDegrees, float followSeconds) {
	TorsoYawSettings s;
	if (freeDegrees >= 0.0f && freeDegrees <= 120.0f) {
		s.freeRadians = freeDegrees * math::kDegreesToRadians;
	}
	if (followSeconds >= 0.0f && followSeconds <= 10.0f) {
		s.followSeconds = followSeconds;
	}
	return s;
}

struct TorsoYawState {
	bool valid = false;
	float torso = 0.0f;     // the torso's yaw in tracking space
	float lastHead = 0.0f;  // the head's, last frame
	float heldOff = 0.0f;   // seconds the head has stood more than the hold off the torso, standing
	bool following = false; // standing, the torso closing on the head until settled
};

// Whether the body is turned after the head at all: the torso part on, a
// head read, the skeleton's parent readable - and not on a horse, where the
// body faces the horse.
inline bool TorsoTurnWanted(bool torsoOn, bool headValid, bool parentReadable, bool riding) {
	return torsoOn && headValid && parentReadable && !riding;
}

// Whether the torso closes on the head this frame: walking (or stepping
// through the room) always; standing, once the head has stood more than the
// hold off it for the hold's time - counted in `s.heldOff`, which a head back
// within the hold, or a walk, starts again.
inline bool TorsoFollowsNow(TorsoYawState& s, float behind, float dtSeconds, bool moving) {
	if (moving) {
		s.heldOff = 0.0f;
		s.following = false;
		return true;
	}
	const float off = behind < 0.0f ? -behind : behind;
	if (s.following) {
		s.following = off > kTorsoStandSettledRadians;
		return s.following;
	}
	if (off <= kTorsoStandHoldRadians) {
		s.heldOff = 0.0f;
		return false;
	}
	s.heldOff += dtSeconds > 0.0f && dtSeconds < 1.0f ? dtSeconds : 0.0f;
	if (s.heldOff >= kTorsoStandHoldSeconds) {
		s.heldOff = 0.0f;
		s.following = true;
	}
	return s.following;
}

// One frame: the head's yaw in tracking space (radians), the frame's time and
// whether the player moves. Answers the torso's yaw relative to the head's:
// what is added to the head's yaw in the world to face the body.
inline float StepTorsoYaw(TorsoYawState& s, float headYaw, float dtSeconds, const TorsoYawSettings& set,
                          bool moving = true) {
	if (!(headYaw == headYaw) || headYaw > 1.0e6f || headYaw < -1.0e6f) {
		return s.valid ? math::WrapAngle(s.torso - s.lastHead) : 0.0f;
	}
	const float jump = math::WrapAngle(headYaw - s.lastHead);
	if (!s.valid || jump > kTorsoJumpRadians || jump < -kTorsoJumpRadians) {
		s.valid = true;
		s.torso = headYaw;
		s.heldOff = 0.0f;
		s.following = false;
	}
	s.lastHead = headYaw;
	const float behind = math::WrapAngle(headYaw - s.torso);
	if (behind > set.freeRadians) {
		s.torso = math::WrapAngle(headYaw - set.freeRadians);
	} else if (behind < -set.freeRadians) {
		s.torso = math::WrapAngle(headYaw + set.freeRadians);
	}
	if (dtSeconds > 0.0f && dtSeconds < 1.0f && TorsoFollowsNow(s, behind, dtSeconds, moving)) {
		const float share = set.followSeconds > 0.0f ? 1.0f - math::Exp(-dtSeconds / set.followSeconds) : 1.0f;
		s.torso = math::WrapAngle(s.torso + math::WrapAngle(headYaw - s.torso) * share);
	}
	return math::WrapAngle(s.torso - headYaw);
}

}  // namespace obvr::game
