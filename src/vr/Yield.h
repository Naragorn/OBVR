#pragma once

#include "core/Types.h"

namespace obvr::vr {

// Yielding by gesture (the tester, 2026-10-07: "wenn ich die waffe
// einstecke und mit offenen händen mit der handfläche zu den gegnern zeige
// und diese leicht von aussen nach innen wippe und das wiederhole ein paar
// mal, zählt das als ein yield").
//
// Vanilla's yield is block and activate together, facing the attacker
// (docs/controls-spec.md). The gesture: the weapon away, both hands open, an
// NPC in combat ahead, and both hands rocking sideways - out and in, out
// and in. Each hand's sideways position (outward from the body positive) is
// watched for reversals of direction after at least kYieldStrokeMetres of
// travel; kYieldReversals of them within kYieldWindowSeconds, counted over
// both hands, are the yield. Then the caller holds block and presses
// activate with the pick on that NPC, and the engine's yield runs - the NPC
// may still refuse, as in vanilla. Not again within kYieldCooldownSeconds.
inline constexpr float kYieldStrokeMetres = 0.05f;
inline constexpr UInt32 kYieldReversals = 4;       // two out-and-in cycles across both hands
inline constexpr float kYieldWindowSeconds = 2.5f;
inline constexpr float kYieldCooldownSeconds = 3.0f;
inline constexpr float kYieldOpenCurl = 0.5f;      // a hand is open below this curl

struct YieldHandTrack {
	bool tracking = false;
	float last = 0.0f;    // the sideways position seen last
	int direction = 0;    // +1 outward, -1 inward, 0 none yet
	float travel = 0.0f;  // since the last reversal
};

struct YieldState {
	YieldHandTrack hand[2];  // [0] right, [1] left
	UInt32 reversals = 0;
	float windowSeconds = 0.0f;
	float cooldownLeft = 0.0f;
};

// One frame. `allowed`: the weapon away, both hands open and tracked, an NPC
// in combat ahead. `lateral[2]` each hand's sideways position, metres,
// outward positive. True on the frame the gesture is complete.
inline bool StepYield(YieldState& s, bool allowed, const float lateral[2], float dt) {
	if (s.cooldownLeft > 0.0f) {
		s.cooldownLeft -= dt;
		if (s.cooldownLeft < 0.0f) {
			s.cooldownLeft = 0.0f;
		}
	}
	if (!allowed) {
		s.hand[0] = YieldHandTrack{};
		s.hand[1] = YieldHandTrack{};
		s.reversals = 0;
		s.windowSeconds = 0.0f;
		return false;
	}
	for (int side = 0; side < 2; ++side) {
		YieldHandTrack& h = s.hand[side];
		if (!h.tracking) {
			h.tracking = true;
			h.last = lateral[side];
			continue;
		}
		const float delta = lateral[side] - h.last;
		h.last = lateral[side];
		const int direction = delta > 0.0f ? 1 : (delta < 0.0f ? -1 : 0);
		if (direction == 0) {
			continue;
		}
		if (h.direction == 0) {
			h.direction = direction;
			h.travel = delta < 0.0f ? -delta : delta;
			continue;
		}
		if (direction == h.direction) {
			h.travel += delta < 0.0f ? -delta : delta;
			continue;
		}
		// A reversal: it counts when the stroke before it was long enough.
		if (h.travel >= kYieldStrokeMetres) {
			++s.reversals;
		}
		h.direction = direction;
		h.travel = delta < 0.0f ? -delta : delta;
	}
	if (s.reversals > 0) {
		s.windowSeconds += dt;
		if (s.windowSeconds > kYieldWindowSeconds) {
			s.reversals = 0;
			s.windowSeconds = 0.0f;
		}
	}
	if (s.reversals >= kYieldReversals && s.cooldownLeft <= 0.0f) {
		s.reversals = 0;
		s.windowSeconds = 0.0f;
		s.cooldownLeft = kYieldCooldownSeconds;
		return true;
	}
	return false;
}

// Whether the gesture is allowed this frame: the weapon away, both hands
// tracked and open, and someone in combat ahead.
inline bool YieldAllowed(bool weaponAway, bool rightTracked, bool leftTracked, float rightCurl, float leftCurl,
                         bool enemyAhead) {
	return weaponAway && rightTracked && leftTracked && rightCurl < kYieldOpenCurl && leftCurl < kYieldOpenCurl &&
	       enemyAhead;
}

}  // namespace obvr::vr
