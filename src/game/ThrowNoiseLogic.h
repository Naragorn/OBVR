#pragma once

// Where a thrown thing makes its noise, for Oblivion Walkers (Nadi,
// 2026-11-05; Walkers listens for 'WNOI' since its noise draws: the walkers
// go to the sound). An object let go from the hand with a throw's speed
// (GrabPhysics' TakeJustThrown) is followed, and its noise is placed once:
// at its first hard stop - flying at least kNoiseImpactFrom m/s, the next
// frame under half of it, the moment it hits a wall, the floor or someone -
// or else where it comes to rest. A toss never faster than kNoiseLeastPeak
// makes none, nor a flight still going after kNoiseFollowSeconds.
//
// Pure, over plain values; camera/CameraHook.cpp follows the thing and
// sends the message (game/WalkersLink.h). throw_noise_test covers every flow.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

inline constexpr float kNoiseImpactFrom = 2.0f;     // m/s before a hard stop
inline constexpr float kNoiseImpactShare = 0.5f;    // the speed after it, at most this share
inline constexpr float kNoiseLeastPeak = 1.0f;      // m/s: a slower toss is silent
inline constexpr float kNoiseRestSpeed = 0.5f;      // m/s: lying still
inline constexpr float kNoiseRestAfter = 0.25f;     // seconds before resting counts
inline constexpr float kNoiseFollowSeconds = 5.0f;  // the longest flight followed

// 'WNOI' (Walkers): a noise's world position (the player's world) and what
// made it.
inline constexpr UInt32 kWalkersNoiseMessage = 0x494F4E57;  // 'WNOI'
inline constexpr UInt32 kNoiseThrow = 1;

struct WalkersNoiseMessage {
	float x;
	float y;
	float z;
	UInt32 kind;
};

inline WalkersNoiseMessage NoiseMessageFor(const NiPoint3& at, UInt32 kind) {
	return WalkersNoiseMessage{at.x, at.y, at.z, kind};
}

// A weapon or torch let go from the hand (camera/hands/WeaponRelease.cpp) is
// sent off with the hand's velocity once its body is there; it is followed
// for its noise when that is a throw's speed (GrabPhysics'
// kThrowMinUnitsPerSecond), not when it falls - and from the send-off, not
// the let-go: before its body comes it hangs still at the hand, which would
// read as resting.
inline bool IsThrowSpeed(const NiPoint3& velocityUnits, float leastUnitsPerSecond) {
	return velocityUnits.LengthSquared() >= leastUnitsPerSecond * leastUnitsPerSecond;
}

// One thrown thing followed for its noise.
struct ThrowNoise {
	UInt32 ref = 0;
	float seconds = 0.0f;
	bool haveLast = false;
	NiPoint3 last{0.0f, 0.0f, 0.0f};
	float speed = 0.0f;  // m/s over the last frame
	float peak = 0.0f;   // the fastest so far
};

struct NoiseStep {
	bool watching = false;  // still followed after this frame
	bool emit = false;      // the noise is made this frame, at `at`
	NiPoint3 at{0.0f, 0.0f, 0.0f};
};

inline void StartThrowNoise(ThrowNoise& n, UInt32 ref) {
	n = ThrowNoise{};
	n.ref = ref;
}

// One frame: the thing is now at `position` (game units).
inline NoiseStep StepThrowNoise(ThrowNoise& n, const NiPoint3& position, float dt, float unitsPerMetre) {
	NoiseStep step;
	if (n.ref == 0) {
		return step;
	}
	const float before = n.speed;
	if (dt > 0.0f && unitsPerMetre > 0.0f) {
		n.seconds += dt;
		if (n.haveLast) {
			n.speed = math::Sqrt((position - n.last).LengthSquared()) / dt / unitsPerMetre;
			n.peak = n.speed > n.peak ? n.speed : n.peak;
		}
	}
	n.last = position;
	n.haveLast = true;
	const bool hardStop = before >= kNoiseImpactFrom && n.speed < before * kNoiseImpactShare;
	const bool resting = n.seconds > kNoiseRestAfter && n.speed < kNoiseRestSpeed;
	if (hardStop || (resting && n.peak >= kNoiseLeastPeak)) {
		step.emit = true;
		step.at = position;
		n = ThrowNoise{};
		return step;
	}
	if (resting || n.seconds > kNoiseFollowSeconds) {
		n = ThrowNoise{};
		return step;
	}
	step.watching = true;
	return step;
}

}  // namespace obvr::game
