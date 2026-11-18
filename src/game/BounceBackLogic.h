#pragma once

// Thrown gear bounces back to the hand (Nadi, 2026-11-18: things the hands
// equip - the torch, the weapons, the staff, the bow - thrown at a foe, hit,
// bounce off and come back towards the player a little, guided, so they can
// be caught in the air; on by default; free-weapons-spec "Thrown gear
// bounces back").
//
// At the throw's hit (game/ThrowLogic.h, the host's StepThrowHits) the
// thing's body is given a lob back to the hand it left - an arc whose speed
// is a share of the hit's, its flight time from that speed and the distance,
// gravity taken into it - and for a short time after it is steered: each
// frame its velocity is turned a little towards the lob that would reach the
// hand from where it is now, in the time left, the change a second capped -
// the first holdSeconds set to that lob outright, past the hit's contact.
// Near the hand the steering lets go and the thing flies on into the reach of
// the hand's grab or pull, which catch it as any weapon in the air
// (free-weapons-spec 5.15). Only gear: a thrown cup or a potion does not come
// back.
//
// Pure, covered by bounce_back_test; the host reads and writes the body.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

struct BounceBackSettings {
	bool enabled = true;          // [Hands] BounceBack
	float returnShare = 0.5f;     // the lob's speed as a share of the hit's
	float minSpeed = 3.0f;        // m/s: the lob at least this fast
	float maxSpeed = 7.0f;        // m/s: at most
	float minFlightSeconds = 0.35f;
	float maxFlightSeconds = 1.1f;
	float guideSeconds = 1.2f;    // how long after the hit it is steered
	// For this long after the hit the lob is set outright each frame, not
	// steered: the hit's own contact keeps pushing the body on - measured
	// 2026-11-18, the sword sent back at 251 units/s read 841 the next frame,
	// on away from the hand (throw-bounce-back.txt 20261118-143347).
	float holdSeconds = 0.15f;
	float guideRate = 5.0f;       // a second: how fast its velocity turns to the lob (1 - e^(-rate dt) a frame)
	float guideMostMetresPerSecond2 = 40.0f;  // the steering's change a second, at most
	float letGoMetres = 0.45f;    // this near the hand the steering lets go
	// Havok's pull on a dropped thing in Oblivion, measured 2026-11-18 on
	// the torch's fall from rest (torch-throw-flick, its body 14 21 29 36 43
	// 50 units/s frame by frame at 72 Hz): about 7.4 m/s^2. The steering
	// corrects what this gets wrong.
	float gravityMetresPerSecond2 = 7.4f;
};

// What a thrown thing is, for whether it comes back.
enum class BounceThing : UInt8 { Other, Weapon, Torch };

inline bool BounceBackFor(const BounceBackSettings& s, BounceThing thing) {
	return s.enabled && thing != BounceThing::Other;
}

// The velocity that carries a thing from `from` to `to` in `seconds` under
// gravity (world units, z up): (to - from) / t, plus g t / 2 upwards.
inline NiPoint3 LobVelocity(const NiPoint3& from, const NiPoint3& to, float seconds, float gravityUnits) {
	const float t = seconds > 0.05f ? seconds : 0.05f;
	NiPoint3 v = (to - from) * (1.0f / t);
	v.z += 0.5f * gravityUnits * t;
	return v;
}

inline float ClampF(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// The lob's flight time: the distance at the rebound's speed, held between
// the settings' bounds.
inline float BounceFlightSeconds(const BounceBackSettings& s, float distanceUnits, float hitMetresPerSecond,
                                 float unitsPerMetre) {
	const float hit = hitMetresPerSecond == hitMetresPerSecond && hitMetresPerSecond > 0.0f ? hitMetresPerSecond : 0.0f;
	const float speed = ClampF(hit * s.returnShare, s.minSpeed, s.maxSpeed) * unitsPerMetre;
	const float t = speed > 0.0f ? distanceUnits / speed : s.maxFlightSeconds;
	return ClampF(t, s.minFlightSeconds, s.maxFlightSeconds);
}

struct BounceFlight {
	UInt32 ref = 0;
	float seconds = 0.0f;        // since the hit
	float flightSeconds = 0.0f;  // the lob's time to the hand, from the hit
	float startMetres = 0.0f;    // the hand's distance at the hit
	float nearestMetres = 0.0f;  // the nearest it came to the hand
	UInt32 steered = 0;          // frames steered
};

enum class BounceEnd : UInt8 {
	None,      // still steered
	AtHand,    // near the hand: let fly on to be caught
	TimeUp,    // guideSeconds passed
	Gone,      // the thing went (caught, picked up, its 3D or body gone)
};

inline const char* BounceEndName(BounceEnd e) {
	switch (e) {
	case BounceEnd::AtHand:
		return "near the hand, let fly on to be caught";
	case BounceEnd::TimeUp:
		return "its time up";
	case BounceEnd::Gone:
		return "gone - caught, picked up or its body gone";
	default:
		return "steered";
	}
}

// At the hit: the lob from `at` to `hand`, the flight started. `velocity`
// answers the body's new velocity, world units a second.
inline bool BeginBounce(BounceFlight& f, const BounceBackSettings& s, UInt32 ref, const NiPoint3& at,
                        const NiPoint3& hand, float hitMetresPerSecond, float unitsPerMetre, NiPoint3& velocity) {
	f = BounceFlight{};
	if (!s.enabled || ref == 0 || !(unitsPerMetre > 0.0f)) {
		return false;
	}
	const float distance = math::Sqrt((hand - at).LengthSquared());
	f.ref = ref;
	f.flightSeconds = BounceFlightSeconds(s, distance, hitMetresPerSecond, unitsPerMetre);
	f.startMetres = distance / unitsPerMetre;
	f.nearestMetres = f.startMetres;
	velocity = LobVelocity(at, hand, f.flightSeconds, s.gravityMetresPerSecond2 * unitsPerMetre);
	return true;
}

struct BounceStep {
	BounceEnd end = BounceEnd::None;
	bool steer = false;          // set the body's velocity to `velocity` this frame
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
};

// One frame: the thing at `at` moving at `current` (world units a second),
// the hand at `hand`; `present` false when the thing or its body is gone.
inline BounceStep StepBounce(BounceFlight& f, const BounceBackSettings& s, bool present, const NiPoint3& at,
                             const NiPoint3& current, const NiPoint3& hand, float dt, float unitsPerMetre) {
	BounceStep out;
	if (f.ref == 0) {
		return out;
	}
	const float step = dt == dt && dt > 0.0f ? dt : 0.0f;
	f.seconds += step;
	if (!present || !(unitsPerMetre > 0.0f)) {
		out.end = BounceEnd::Gone;
		f.ref = 0;
		return out;
	}
	const float metres = math::Sqrt((hand - at).LengthSquared()) / unitsPerMetre;
	f.nearestMetres = metres < f.nearestMetres ? metres : f.nearestMetres;
	if (metres <= s.letGoMetres) {
		out.end = BounceEnd::AtHand;
		f.ref = 0;
		return out;
	}
	if (f.seconds > s.guideSeconds) {
		out.end = BounceEnd::TimeUp;
		f.ref = 0;
		return out;
	}
	if (step <= 0.0f) {
		return out;
	}
	// The time left on the lob; past it, a short time on so the lob stays
	// gentle.
	const float left = ClampF(f.flightSeconds - f.seconds, 0.15f, s.maxFlightSeconds);
	const NiPoint3 want = LobVelocity(at, hand, left, s.gravityMetresPerSecond2 * unitsPerMetre);
	out.steer = true;
	++f.steered;
	if (f.seconds <= s.holdSeconds) {
		out.velocity = want;
		return out;
	}
	const float share = 1.0f - math::Exp(-s.guideRate * step);
	NiPoint3 change = (want - current) * share;
	const float most = s.guideMostMetresPerSecond2 * unitsPerMetre * step;
	const float size = math::Sqrt(change.LengthSquared());
	if (size > most && size > 0.0f) {
		change = change * (most / size);
	}
	out.velocity = current + change;
	return out;
}

}  // namespace obvr::game
