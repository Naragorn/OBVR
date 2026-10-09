#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// The weapon's weight, felt: the drawn weapon hand trails the controller
// by the weapon's weight (docs/physical-combat-spec.md, section 6). The
// controller is where the player put the hand; the DRAWN pose follows it,
// and is never further from it than a cap, so a slow hand never reads as
// lag and a jump of the poses (a seated zero reset) is bounded. The weight
// is the item's own, the WEAP form's TESWeightForm (game::WeaponWeightOf):
// an iron dagger 3 sits on the hand, a warhammer 42 is left behind by a
// fast hand.
//
// Two ways to follow, by [Hands] WeaponSwingThrough:
//
//   * The swing-through (on by default): the weapon has momentum. It is
//     joined to the hand by a spring and a damper that act on its motion
//     RELATIVE to the hand, as Blade & Sorcery's joint does: a hand that
//     starts leaves the weapon behind, a hand at a steady speed has it on
//     the hand again, a hand that stops is overrun by it - the weapon
//     carries on past the hand and swings back, a quarter of the way (the
//     damping ratio 0.4). Each frame is the damped oscillator's closed
//     form (Ryan Juckett, "Damped Springs", 2012): exact for a hand that
//     moves evenly within the frame, stable however long the frame; the
//     hand's change of speed between frames is the push the weapon feels.
//   * The plain lag (off): first order, drawn += (wanted - drawn) * dt /
//     (tc + dt). It trails a moving hand by speed * tc and never
//     overshoots.
//
// Either way the pose is lagged in the tracking space - the room - not
// relative to the head: a head turned with the hand held still must not
// swing the weapon, and walking moves the game's camera, not the room's
// poses, so the weapon rides along without lag.
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
// The swing-through's damping ratio: under 1 the weapon overshoots, and by
// this much of the way - e^(-zeta pi / sqrt(1 - zeta^2)), 25 % at 0.4. Its
// spring's natural frequency is 1 / tc, so the heaviest weapon at 100 %
// swings at 8.3 rad/s (a 0.8 s period) and at 40 % at 21.
constexpr float kWeaponSwingThroughDamping = 0.4f;

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
	// The weapon's momentum carries it past the hand (the second order
	// above); off, the plain lag.
	bool swingThrough = false;
};

// The lag for a weapon of this weight at the slider's setting: the time
// constant from the weight and the strength, cut to 0.4 with both hands on
// the handle; the caps from the strength alone.
inline WeaponLagTuning WeaponLagFor(float weight, float percent, bool twoHanded, bool swingThrough) {
	const float strength = WeaponWeightStrength(percent);
	WeaponLagTuning t;
	t.timeConstantSeconds = kWeaponLagTimeConstantMax * WeightFactor(weight) * strength *
	                        (twoHanded ? kWeaponLagTwoHandFactor : 1.0f);
	t.capMetres = kWeaponLagCapMetres * strength;
	t.capRadians = kWeaponLagCapDegrees * math::kDegreesToRadians * strength;
	t.swingThrough = swingThrough;
	return t;
}

inline float DotOf(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

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

// The rotation vector of a turn: its axis times its angle, the short way
// round (q and -q are one turn); the zero vector for no turn.
inline NiPoint3 RotationVectorOf(const Quaternion& turn) {
	const float sign = turn.w < 0.0f ? -1.0f : 1.0f;
	const float x = turn.x * sign;
	const float y = turn.y * sign;
	const float z = turn.z * sign;
	const float w = turn.w * sign;
	const float sine = math::Sqrt(x * x + y * y + z * z);
	if (sine < 1e-6f) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	const float angle = 2.0f * math::Atan2(sine, w);
	return NiPoint3{x, y, z} * (angle / sine);
}

// The turn of a rotation vector: the inverse of the above.
inline Quaternion TurnOfRotationVector(const NiPoint3& r) {
	const float angle = math::Sqrt(r.LengthSquared());
	if (angle < 1e-6f) {
		return Quaternion::Identity();
	}
	const float half = angle * 0.5f;
	const float scale = math::Sin(half) / angle;
	return Quaternion{r.x * scale, r.y * scale, r.z * scale, math::Cos(half)};
}

// One frame of the damped oscillator x'' + 2 zeta omega x' + omega^2 x = 0,
// as the four coefficients of its closed form for zeta < 1 (Ryan Juckett,
// "Damped Springs", 2012): x1 = pp x0 + pv v0, v1 = vp x0 + vv v0. Exact,
// so a long frame cannot blow it up; no time, or a spring that cannot
// swing, leaves everything as it is.
struct SpringStep {
	float pp = 1.0f;
	float pv = 0.0f;
	float vp = 0.0f;
	float vv = 1.0f;
};

inline SpringStep UnderDampedStep(float omega, float zeta, float dt) {
	SpringStep k;
	const float decay = zeta * omega;
	const float omegaD = omega * math::Sqrt(1.0f - zeta * zeta);
	if (!(omegaD > 0.0f) || !(dt > 0.0f)) {
		return k;
	}
	const float e = math::Exp(-decay * dt);
	const float c = math::Cos(omegaD * dt);
	const float sOverD = math::Sin(omegaD * dt) / omegaD;
	k.pp = e * (c + decay * sOverD);
	k.pv = e * sOverD;
	k.vp = -e * omega * omega * sOverD;
	k.vv = e * (c - decay * sOverD);
	return k;
}

// The drawn pose, carried from frame to frame while a weapon lags.
struct WeaponLagState {
	bool held = false;  // a drawn pose is held from the last frame
	Quaternion orientation = Quaternion::Identity();
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	// The swing-through's momentum: the drawn pose's velocity in the room
	// (metres a second) and its spin (radians a second, a rotation vector),
	// and the hand's pose of the last frame, which this frame's motion of
	// the hand is measured from.
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
	NiPoint3 spin{0.0f, 0.0f, 0.0f};
	Quaternion lastWantedOrientation = Quaternion::Identity();
	NiPoint3 lastWantedPosition{0.0f, 0.0f, 0.0f};
};

struct WeaponLagVerdict {
	// The lag is on: the drawn pose is the state's, not the controller's.
	bool lagging = false;
	Quaternion orientation = Quaternion::Identity();
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	// How far the drawn pose is from the controller, for the log.
	float gapMetres = 0.0f;
	float gapRadians = 0.0f;
};

// One frame of the lag. `weaponInHand` is a swung weapon drawn in the hand;
// without one, or with a time constant of 0 (a dagger, the slider at 1 %),
// the drawn pose is the wanted one and the state is let go, so the next
// weapon is taken up where the hand is, without momentum - no jump at the
// draw. A frame without time (dt 0, or none that is a number) leaves the
// drawn pose where it is.
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
		s.velocity = NiPoint3{0.0f, 0.0f, 0.0f};
		s.spin = NiPoint3{0.0f, 0.0f, 0.0f};
		s.lastWantedOrientation = wantedOrientation;
		s.lastWantedPosition = wantedPosition;
	}
	const float dt = dtSeconds > 0.0f ? dtSeconds : 0.0f;
	// The hand's motion this frame, which the swing-through measures the
	// weapon's momentum against; nothing with the plain lag.
	NiPoint3 handVelocity{0.0f, 0.0f, 0.0f};
	NiPoint3 handSpin{0.0f, 0.0f, 0.0f};
	if (dt > 0.0f) {
		if (t.swingThrough) {
			const SpringStep k =
				UnderDampedStep(1.0f / t.timeConstantSeconds, kWeaponSwingThroughDamping, dt);
			const float perSecond = 1.0f / dt;
			// The drawn pose behind the hand as it WAS, and its velocity
			// relative to the hand's this frame, through the oscillator; then
			// put behind the hand as it is. The hand's frame moved evenly within
			// the frame, so nothing acts on the weapon in it but the joint; a
			// change of the hand's speed between frames is the push.
			handVelocity = (wantedPosition - s.lastWantedPosition) * perSecond;
			const NiPoint3 x0 = s.position - s.lastWantedPosition;
			const NiPoint3 v0 = s.velocity - handVelocity;
			s.position = wantedPosition + x0 * k.pp + v0 * k.pv;
			s.velocity = handVelocity + x0 * k.vp + v0 * k.vv;
			s.lastWantedPosition = wantedPosition;
			// The orientation the same way, with rotation vectors in the room's
			// frame: the turn from the hand's orientation to the drawn one.
			handSpin = RotationVectorOf(wantedOrientation * s.lastWantedOrientation.Conjugate()) * perSecond;
			const NiPoint3 r0 = RotationVectorOf(s.orientation * s.lastWantedOrientation.Conjugate());
			const NiPoint3 w0 = s.spin - handSpin;
			s.orientation = (TurnOfRotationVector(r0 * k.pp + w0 * k.pv) * wantedOrientation).Normalized();
			s.spin = handSpin + r0 * k.vp + w0 * k.vv;
			s.lastWantedOrientation = wantedOrientation;
		} else {
			const float share = dt / (t.timeConstantSeconds + dt);
			s.position = s.position + (wantedPosition - s.position) * share;
			s.orientation = SlerpTowards(s.orientation, wantedOrientation, share);
		}
	}

	// The cap: never further from the hand than this, in either measure. The
	// momentum that would carry the weapon further out is taken away, so it
	// rests at the cap rather than presses on it.
	const NiPoint3 behind = s.position - wantedPosition;
	float gap = math::Sqrt(behind.LengthSquared());
	if (gap > t.capMetres) {
		const NiPoint3 outward = behind * (1.0f / gap);
		s.position = wantedPosition + outward * t.capMetres;
		gap = t.capMetres;
		const float out = DotOf(s.velocity - handVelocity, outward);
		if (out > 0.0f) {
			s.velocity = s.velocity - outward * out;
		}
	}
	float angle = AngleBetween(s.orientation, wantedOrientation);
	if (angle > t.capRadians) {
		s.orientation = SlerpTowards(wantedOrientation, s.orientation, t.capRadians / angle);
		angle = t.capRadians;
		const NiPoint3 turn = RotationVectorOf(s.orientation * wantedOrientation.Conjugate());
		const float turnAngle = math::Sqrt(turn.LengthSquared());
		if (turnAngle > 0.0f) {
			const NiPoint3 outward = turn * (1.0f / turnAngle);
			const float out = DotOf(s.spin - handSpin, outward);
			if (out > 0.0f) {
				s.spin = s.spin - outward * out;
			}
		}
	}

	v.lagging = true;
	v.orientation = s.orientation;
	v.position = s.position;
	v.gapMetres = gap;
	v.gapRadians = angle;
	return v;
}

}  // namespace obvr::vr
