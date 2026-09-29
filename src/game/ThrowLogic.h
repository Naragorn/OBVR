#pragma once

// Thrown things that hit people (the tester, 2026-09-29: "ich nehme
// irgendwas in die hand auf mit grab und werfe es auf npcs. mit genug schwung
// fallen die zu boden wie der schubser. oder werden zumindest staggered").
//
// An object let go from the hand with speed is followed for a few seconds.
// When it passes through a living person's body (the upright column the
// shove uses, game::HandAtBody), fast enough it staggers them, faster still it
// knocks them down - the shove's own two effects, game/Shove.h - once per
// throw.
//
// Pure, covered by throw_test.

#include "core/Types.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

struct ThrowHitSettings {
	bool enabled = true;
	float staggerSpeed = 4.0f;   // m/s: staggered and pushed back
	float knockSpeed = 7.0f;     // m/s: knocked to the ground
	float followSeconds = 3.0f;  // how long a thrown thing is watched
};

// What a thing flying at this speed (m/s) does to whoever it meets.
inline ShoveKind ThrowHitFor(const ThrowHitSettings& s, float speed) {
	if (!s.enabled || !(speed >= s.staggerSpeed)) {
		return ShoveKind::None;
	}
	return speed >= s.knockSpeed ? ShoveKind::Hard : ShoveKind::Light;
}

// One thrown thing in flight.
struct ThrowFlight {
	UInt32 ref = 0;
	float seconds = 0.0f;
	bool haveLast = false;
	NiPoint3 last{0.0f, 0.0f, 0.0f};
	float speed = 0.0f;  // m/s over the last frame
};

inline void StartThrowFlight(ThrowFlight& f, UInt32 ref) {
	f = ThrowFlight{};
	f.ref = ref;
}

// One frame of flight: the thing is now at `position` (game units). Answers
// false when it is no longer watched - its time is up, or it has come to rest
// after the first quarter second.
inline bool StepThrowFlight(ThrowFlight& f, const ThrowHitSettings& s, const NiPoint3& position, float dt,
                            float unitsPerMetre) {
	if (f.ref == 0) {
		return false;
	}
	f.seconds += dt;
	if (f.haveLast && dt > 0.0f && unitsPerMetre > 0.0f) {
		f.speed = math::Sqrt((position - f.last).LengthSquared()) / dt / unitsPerMetre;
	}
	f.last = position;
	f.haveLast = true;
	const bool resting = f.seconds > 0.25f && f.speed < 0.5f;
	if (f.seconds > s.followSeconds || resting) {
		f = ThrowFlight{};
		return false;
	}
	return true;
}

}  // namespace obvr::game
