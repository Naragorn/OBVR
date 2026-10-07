#pragma once

#include "core/Types.h"

namespace obvr::game {

// The middle finger (the tester, 2026-10-07: "den mittelfinger zeigen auf
// NPCs lowert deren disposition by a value x"). From the controller's finger
// curls (SteamVR's skeleton, 0 open to 1 curled, thumb to little finger): the
// middle finger out, the index, ring and little fingers curled. Held at an
// NPC under that hand's laser (game::ActorUnderRay) for kInsultHoldSeconds,
// their disposition drops by [Hands] MiddleFingerDisposition - once, until
// the gesture is let go and the same NPC is not insulted again within
// kInsultCooldownSeconds. The thumb is free: a thumb out or in is the same
// gesture to anyone looking.
inline constexpr float kInsultMiddleOpen = 0.35f;
inline constexpr float kInsultOthersCurled = 0.60f;
inline constexpr float kInsultHoldSeconds = 0.4f;
inline constexpr float kInsultCooldownSeconds = 8.0f;
inline constexpr float kInsultReachUnits = 300.0f;   // 4 m
inline constexpr float kInsultConeCos = 0.966f;      // 15 degrees

inline bool InsultGesture(bool curlValid, const float curl[5]) {
	return curlValid && curl[2] <= kInsultMiddleOpen && curl[1] >= kInsultOthersCurled &&
	       curl[3] >= kInsultOthersCurled && curl[4] >= kInsultOthersCurled;
}

struct InsultState {
	float heldSeconds = 0.0f;     // the gesture at an NPC, so far
	bool fired = false;           // this gesture already counted
	const void* lastActor = nullptr;
	float cooldownLeft = 0.0f;    // for lastActor
};

// One frame for one hand. `actor` is who the hand points at (nullptr for
// no one). True on the frame the insult lands on `actor`.
inline bool StepInsult(InsultState& s, bool gesture, const void* actor, float dt) {
	if (s.cooldownLeft > 0.0f) {
		s.cooldownLeft -= dt;
		if (s.cooldownLeft <= 0.0f) {
			s.cooldownLeft = 0.0f;
			s.lastActor = nullptr;
		}
	}
	if (!gesture || actor == nullptr) {
		s.heldSeconds = 0.0f;
		if (!gesture) {
			s.fired = false;
		}
		return false;
	}
	if (s.fired) {
		return false;
	}
	s.heldSeconds += dt;
	if (s.heldSeconds < kInsultHoldSeconds) {
		return false;
	}
	if (actor == s.lastActor && s.cooldownLeft > 0.0f) {
		return false;
	}
	s.fired = true;
	s.lastActor = actor;
	s.cooldownLeft = kInsultCooldownSeconds;
	return true;
}

}  // namespace obvr::game
