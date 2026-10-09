#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// The weapon's weight, felt: the drawn weapon hand trails the controller
// behind a lag whose time constant grows with the weapon's weight
// (docs/physical-combat-spec.md, section 6). The controller is where the
// player put the hand; the DRAWN pose follows it as a first-order lag - each
// frame it closes the share dt / (tc + dt) of the gap - and is never further
// from it than a cap, so a slow hand never reads as lag and a jump of the
// poses (a seated zero reset) is bounded. The weight is the item's own, the
// WEAP form's TESWeightForm (game::WeaponWeightOf): an iron dagger 3 sits on
// the hand, a warhammer 42 trails a fast swing by a hand's breadth and
// carries on after the hand stopped.
//
// The pose is lagged in the tracking space - the room - not relative to the
// head: a head turned with the hand held still must not swing the weapon,
// and walking moves the game's camera, not the room's poses, so the weapon
// rides along without lag.
//
// Pure: a step over plain values, so the response per weight, slider and
// grip is checked in weapon_weight_test.

// The code constants the slider scales (section 6: nothing else to tune).
constexpr float kWeaponLagTimeConstantMax = 0.12f;  // s: the heaviest weapon at 100 %
constexpr float kWeaponLagCapMetres = 0.25f;        // the gap at 100 %
constexpr float kWeaponLagCapDegrees = 35.0f;       // the heading's gap at 100 %
constexpr float kWeaponLagTwoHandFactor = 0.4f;     // both hands on the handle
// The weight that lags not at all (the iron dagger, 3) and the one that lags
// fully (a warhammer, 42, nearly); between them the lag grows in a straight
// line.
constexpr float kWeaponLagLightWeight = 3.0f;
constexpr float kWeaponLagHeavyWeight = 43.0f;

// 0 for a dagger and anything lighter, 1 for a warhammer and anything
// heavier. A weight that is not a number is no weight.
inline float WeightFactor(float weight) {
	if (!(weight > kWeaponLagLightWeight)) {
		return 0.0f;
	}
	const float factor = (weight - kWeaponLagLightWeight) / (kWeaponLagHeavyWeight - kWeaponLagLightWeight);
	return factor > 1.0f ? 1.0f : factor;
}

// The slider ([Hands] WeaponWeight, 1-100) as 0.01-1.00: anything under 1,
// or not a number, is 1; anything over 100 is 100.
inline float WeaponWeightStrength(float percent) {
	if (!(percent > 1.0f)) {
		return 0.01f;
	}
	return percent < 100.0f ? percent * 0.01f : 1.0f;
}

struct WeaponLagTuning {
	float timeConstantSeconds = 0.0f;  // 0: the drawn pose is the controller's
	float capMetres = 0.0f;
	float capRadians = 0.0f;
};

// The lag for a weapon of this weight at the slider's setting: the time
// constant from the weight and the strength, cut to 0.4 with both hands on
// the handle; the caps from the strength alone.
inline WeaponLagTuning WeaponLagFor(float weight, float percent, bool twoHanded) {
	const float strength = WeaponWeightStrength(percent);
	WeaponLagTuning t;
	t.timeConstantSeconds = kWeaponLagTimeConstantMax * WeightFactor(weight) * strength *
	                        (twoHanded ? kWeaponLagTwoHandFactor : 1.0f);
	t.capMetres = kWeaponLagCapMetres * strength;
	t.capRadians = kWeaponLagCapDegrees * math::kDegreesToRadians * strength;
	return t;
}

inline float QuaternionDot(const Quaternion& a, const Quaternion& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

// The angle between two orientations, 0 to pi. The turn from a to b as a
// quaternion: its vector's length is the sine of the half-angle and its w
// the cosine, either sign (q and -q are one orientation), so atan2 of the
// two is exact near 0 as well - where the dot's cosine alone loses it to
// 1 - dot*dot. Out of atan2 and a square root: the cross build has no acos
// (core/MathFns.h).
inline float AngleBetween(const Quaternion& a, const Quaternion& b) {
	const Quaternion turn = a.Conjugate() * b;
	const float sine = math::Sqrt(turn.x * turn.x + turn.y * turn.y + turn.z * turn.z);
	const float cosine = turn.w < 0.0f ? -turn.w : turn.w;
	return 2.0f * math::Atan2(sine, cosine);
}

// The orientation this share of the way from `from` to `to`, along the short
// arc (a slerp); a straight blend for the last degree, where the arc's sine
// vanishes.
inline Quaternion SlerpTowards(const Quaternion& from, const Quaternion& to, float share) {
	// `to` on the same side of the sphere as `from`, so the arc is the short
	// way round.
	Quaternion b = to;
	float dot = QuaternionDot(from, to);
	if (dot < 0.0f) {
		b = Quaternion{-to.x, -to.y, -to.z, -to.w};
		dot = -dot;
	}
	if (!(share > 0.0f)) {
		return from;
	}
	if (share >= 1.0f) {
		return b;
	}
	if (dot > 0.9995f) {
		return Quaternion{from.x + (b.x - from.x) * share, from.y + (b.y - from.y) * share,
		                  from.z + (b.z - from.z) * share, from.w + (b.w - from.w) * share}
			.Normalized();
	}
	const float sine = math::Sqrt(1.0f - dot * dot);
	const float half = math::Atan2(sine, dot);
	const float ofFrom = math::Sin((1.0f - share) * half) / sine;
	const float ofTo = math::Sin(share * half) / sine;
	return Quaternion{from.x * ofFrom + b.x * ofTo, from.y * ofFrom + b.y * ofTo, from.z * ofFrom + b.z * ofTo,
	                  from.w * ofFrom + b.w * ofTo}
		.Normalized();
}

// The drawn pose, carried from frame to frame while a weapon lags.
struct WeaponLagState {
	bool held = false;  // a drawn pose is held from the last frame
	Quaternion orientation = Quaternion::Identity();
	NiPoint3 position{0.0f, 0.0f, 0.0f};
};

struct WeaponLagVerdict {
	// The lag is on: the drawn pose is the state's, not the controller's.
	bool lagging = false;
	Quaternion orientation = Quaternion::Identity();
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	// How far the drawn pose is behind the controller, for the log.
	float gapMetres = 0.0f;
	float gapRadians = 0.0f;
};

// One frame of the lag. `weaponInHand` is a swung weapon drawn in the hand;
// without one, or with a time constant of 0 (a dagger, the slider at 1 %),
// the drawn pose is the wanted one and the state is let go, so the next
// weapon is taken up where the hand is - no jump at the draw. A frame
// without time (dt 0, or none that is a number) leaves the drawn pose where
// it is.
inline WeaponLagVerdict StepWeaponLag(WeaponLagState& s, bool weaponInHand, const WeaponLagTuning& t,
                                      const Quaternion& wantedOrientation, const NiPoint3& wantedPosition,
                                      float dtSeconds) {
	WeaponLagVerdict v;
	v.orientation = wantedOrientation;
	v.position = wantedPosition;
	if (!weaponInHand || !(t.timeConstantSeconds > 0.0f)) {
		s = WeaponLagState{};
		return v;
	}
	if (!s.held) {
		s.held = true;
		s.orientation = wantedOrientation;
		s.position = wantedPosition;
	}
	const float dt = dtSeconds > 0.0f ? dtSeconds : 0.0f;
	const float share = dt / (t.timeConstantSeconds + dt);
	s.position = s.position + (wantedPosition - s.position) * share;
	s.orientation = SlerpTowards(s.orientation, wantedOrientation, share);

	// The cap: never further behind than this, in either measure.
	const NiPoint3 behind = s.position - wantedPosition;
	float gap = math::Sqrt(behind.LengthSquared());
	if (gap > t.capMetres) {
		s.position = wantedPosition + behind * (t.capMetres / gap);
		gap = t.capMetres;
	}
	float angle = AngleBetween(s.orientation, wantedOrientation);
	if (angle > t.capRadians) {
		s.orientation = SlerpTowards(wantedOrientation, s.orientation, t.capRadians / angle);
		angle = t.capRadians;
	}

	v.lagging = true;
	v.orientation = s.orientation;
	v.position = s.position;
	v.gapMetres = gap;
	v.gapRadians = angle;
	return v;
}

}  // namespace obvr::vr
