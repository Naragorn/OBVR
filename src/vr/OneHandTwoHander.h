#pragma once

// A two-hander held in one hand (Nadi, 2026-10-26: "2 händer, ausser staffs:
// mit nur einer hand getragen hat dann 40% (default) mehr "Gewicht" in vr. Mit
// beiden Händen gehalten: normales Gewicht wie bisher. Mit nur einer hand 40%
// (default) mehr Ausdauer kosten auf alles. und neu: Mit nur einer hand
// hochhalten über der Hüfte kostet jz bei 2 händern auch schon Ausdauer
// (default on). Runter gehalten unter der hüfte aber kostet auch mit nur einer
// hand nix. Mit beiden Händen kein Ausdauer verlust nur beim halten auch über
// der hüfte. den 2 händer auf der schulter ruhen lassen mit nur einer hand
// kostet auch keine ausdauer").
//
// A two-handed blade or blunt weapon - not a staff - drawn in the weapon hand
// without the other hand on its handle (vr/TwoHandLogic.h, TwoHandHolds):
//
//   * the weight: its lag behind the controller is heavier by [Hands]
//     OneHandTwoHanderWeight percent - the time constant and the caps alike,
//     as exhaustion makes it heavier (vr::WeaponLagHeavier). Both hands on
//     the handle: the weight as before.
//   * the fatigue fighting costs is [Hands] OneHandTwoHanderFatigue percent
//     higher: the game's attack and block (game/FatigueCost.h), OBVR's
//     swing, parry, shove and grab (OneHandScales). Nadi, 2026-10-26, after
//     the first build made every cost dearer: "nur auf angriff block, swung,
//     parade, schubsen, packen" - running, a jump, a spell, a bow and the
//     teleport cost as they did. Fatigue others take with blows and spells
//     is no cost and stays as it is.
//   * holding it up costs ([Hands] OneHandTwoHanderHoldCosts, on): with the
//     weapon hand over the hip, OneHandTwoHanderHoldFatigue fatigue burns a
//     second - the game's own figure for a bow held drawn by a novice
//     (fMarksmanFatigueBurnPerSecond 15, read in the attack update at
//     0x005FACFC; the CS wiki's "Fatigue Game Settings"). The game gives back
//     fFatigueReturnBase 10 a second, so the bar falls 5 a second. Held low,
//     under the hip, nothing burns; with both hands nothing burns at any
//     height; resting on a shoulder - the weapon lying over it, its tip
//     behind - nothing burns either.
//
// The hip and the shoulders are places in the body's frame (vr::BodyRelative:
// metres from the eyes along the head's heading, x right, y ahead, z up). The
// hip is [Hands] HipBelowEyesMetres under the eyes: 0.65, a standing adult's
// eyes 1.6 m and hip 0.95 m up (vr/Holster.h's numbers), made to fit by the
// T-pose as the weapons' places are (vr/BodyMeasure.h). The shoulders follow
// it at the same scale; their place is derived from body proportions
// (Drillis and Contini, the ratios vr/BodyMeasure.h cites: the eyes 0.936 of
// the stature, the shoulder's top 0.818, its breadth 0.259), not measured on
// a player.
//
// Pure, covered by one_hand_two_hander_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

struct OneHandTwoHanderSettings {
	float weightPercent = 40.0f;   // [Hands] OneHandTwoHanderWeight, 0 as before
	float fatiguePercent = 40.0f;  // [Hands] OneHandTwoHanderFatigue, 0 as before
	bool holdCosts = true;         // [Hands] OneHandTwoHanderHoldCosts
	float holdPerSecond = 15.0f;   // [Hands] OneHandTwoHanderHoldFatigue
	float hipBelowEyes = 0.65f;    // [Hands] HipBelowEyesMetres
};

// The hip's default distance under the eyes, which the shoulders' places are
// written for.
inline constexpr float kHipBelowEyesMetres = 0.65f;
// The hand within this of the hip keeps the hold it had: no flicker at the
// line.
inline constexpr float kHipBandMetres = 0.02f;
// The top of a shoulder, where a weapon rests on it, at the default hip: out
// from the middle (half the breadth, 0.13 of the stature, less the slope
// towards the neck the weapon lies on), behind the eyes (the shoulders stand
// under the ears) and under them (0.936 - 0.818 of the stature, 0.20 m, less
// the muscle the weapon lies on).
inline constexpr float kShoulderRestOutMetres = 0.15f;
inline constexpr float kShoulderRestAheadMetres = -0.08f;
inline constexpr float kShoulderRestUpMetres = -0.17f;
// The weapon this near a shoulder's top rests on it, and stays resting until
// it is further than the second.
inline constexpr float kShoulderRestMetres = 0.10f;
inline constexpr float kShoulderRestStayMetres = 0.13f;
// The weapon rests on the shoulder with its blade or its shaft, not with the
// fist round its handle: the touch at least this far up it from the grip.
inline constexpr float kShoulderRestFromGripMetres = 0.12f;
// The places' scale is held to the T-pose's range (vr/BodyMeasure.h).
inline constexpr float kHoldScaleLeast = 0.8f;
inline constexpr float kHoldScaleMost = 1.25f;

// A percent more as a factor: 40 is 1.4. Under 0 or not a number is 1; over
// 200 is 3.
inline float PercentMore(float percent) {
	if (!(percent > 0.0f)) {
		return 1.0f;
	}
	return 1.0f + (percent < 200.0f ? percent : 200.0f) * 0.01f;
}

// A two-handed blade (1) or blunt weapon (3) - not a staff (4), which is not
// swung by its weight, and nothing one-handed.
inline bool IsSwungTwoHanderType(SInt32 type) { return type == 1 || type == 3; }

// What a cost the player pays is for (game/FatigueCost.h): the game's two
// that are pointed at OBVR, and OBVR's own.
enum class FatigueCostKind : UInt8 {
	Attack,    // the game's (0x005E4010): an attack by the attack control
	Block,     // the game's (0x005F5B70): a blow blocked
	Swing,     // a swing that may strike by motion (game::ChargeSwingFatigue)
	Parry,     // a blow parried (game::TakeParryFatigue)
	Shove,     // the other hand's shove or punch (game/Shove.h)
	Grab,      // the other hand taking someone (game/GrappleLogic.h)
	Teleport,  // a teleport's jump
	Hold,      // holding a two-hander up (StepTwoHanderHold's burn)
	Count,
};

// Whether a two-hander in one hand makes this cost dearer: the costs of
// fighting with it - not the teleport, and not the hold's own burn, which is
// the one hand's cost already.
inline bool OneHandScales(FatigueCostKind kind) {
	switch (kind) {
	case FatigueCostKind::Attack:
	case FatigueCostKind::Block:
	case FatigueCostKind::Swing:
	case FatigueCostKind::Parry:
	case FatigueCostKind::Shove:
	case FatigueCostKind::Grab:
		return true;
	default:
		return false;
	}
}

inline const char* FatigueCostKindWords(FatigueCostKind kind) {
	switch (kind) {
	case FatigueCostKind::Attack:
		return "the game's attack";
	case FatigueCostKind::Block:
		return "the game's block";
	case FatigueCostKind::Swing:
		return "a swing";
	case FatigueCostKind::Parry:
		return "a parry";
	case FatigueCostKind::Shove:
		return "a shove";
	case FatigueCostKind::Grab:
		return "a grab";
	case FatigueCostKind::Teleport:
		return "a teleport";
	case FatigueCostKind::Hold:
		return "holding it up";
	default:
		return "nothing";
	}
}

// The fatigue change the player pays: a cost (a negative change) times the
// scale while it is over 1; anything else as it is.
inline float ScaledFatigueChange(float change, float scale) {
	if (!(change < 0.0f) || !(scale > 1.0f) || !(scale < 1.0e3f)) {
		return change;
	}
	return change * scale;
}

// A point of the world in the body's frame, from the cyclopean camera - the
// head (vr/LaserGeometry.h): its world frame is x right, y forward, z up in
// game units, the world's z the room's up. The heading is where the camera
// looks, on the level; metres from the eyes along it, x right, y ahead, z
// up. The same frame vr::BodyRelative makes from the tracking space.
inline NiPoint3 BodyFrameOf(const NiMatrix33& cameraRot, const NiPoint3& cameraPos, const NiPoint3& world,
                            float unitsPerMetre) {
	float fx = cameraRot.data[0][1];
	float fy = cameraRot.data[1][1];
	const float length = math::Sqrt(fx * fx + fy * fy);
	if (length < 1e-4f) {
		fx = 0.0f;
		fy = 1.0f;
	} else {
		fx /= length;
		fy /= length;
	}
	const NiPoint3 d = world - cameraPos;
	const float perMetre = unitsPerMetre > 0.0f ? 1.0f / unitsPerMetre : 0.0f;
	// Right is forward turned a quarter clockwise seen from above: (fy, -fx).
	return NiPoint3{(d.x * fy - d.y * fx) * perMetre, (d.x * fx + d.y * fy) * perMetre, d.z * perMetre};
}

// The places' scale from the hip: the T-pose writes the hip as it writes the
// others, its default times the scale.
inline float HoldScaleOf(float hipBelowEyes) {
	if (!(hipBelowEyes > 0.0f)) {
		return 1.0f;
	}
	const float scale = hipBelowEyes / kHipBelowEyesMetres;
	return scale < kHoldScaleLeast ? kHoldScaleLeast : (scale > kHoldScaleMost ? kHoldScaleMost : scale);
}

struct ShoulderRest {
	bool resting = false;
	bool right = false;     // on the right shoulder (the body's +x), else the left
	float distance = 1.0e9f;  // from the nearer shoulder's top to the weapon, metres
	float along = 0.0f;       // where on the weapon, metres from its grip
};

// Whether the weapon from `grip` to `tip` (the body's frame) rests on a
// shoulder: its blade or shaft - kShoulderRestFromGripMetres and more up it -
// within `radius` of a shoulder's top, its tip behind that shoulder. Of the
// two shoulders the nearer is said; a weapon shorter than the grip's share
// rests on none.
inline ShoulderRest ShoulderRestOf(const NiPoint3& grip, const NiPoint3& tip, float scale, float radius) {
	ShoulderRest r;
	const NiPoint3 line = tip - grip;
	const float length = math::Sqrt(line.LengthSquared());
	if (!(length > kShoulderRestFromGripMetres) || !(scale > 0.0f)) {
		return r;
	}
	const NiPoint3 dir = line * (1.0f / length);
	for (int side = 0; side < 2; ++side) {
		const bool right = side == 0;
		const NiPoint3 top{(right ? kShoulderRestOutMetres : -kShoulderRestOutMetres) * scale,
		                   kShoulderRestAheadMetres * scale, kShoulderRestUpMetres * scale};
		const NiPoint3 d = top - grip;
		float along = d.x * dir.x + d.y * dir.y + d.z * dir.z;
		along = along < kShoulderRestFromGripMetres ? kShoulderRestFromGripMetres : (along > length ? length : along);
		const NiPoint3 off = top - (grip + dir * along);
		const float distance = math::Sqrt(off.LengthSquared());
		if (!(distance < r.distance)) {
			continue;
		}
		r.distance = distance;
		r.right = right;
		r.along = along;
		r.resting = distance <= radius && tip.y < top.y;
	}
	return r;
}

enum class TwoHanderHold : UInt8 {
	None,       // no two-hander drawn in the hand
	BothHands,  // the other hand on its handle
	Low,        // in one hand, the hand under the hip
	Raised,     // in one hand, the hand over the hip
	Shoulder,   // in one hand, resting on a shoulder
};

inline const char* TwoHanderHoldWords(TwoHanderHold h) {
	switch (h) {
	case TwoHanderHold::BothHands:
		return "in both hands";
	case TwoHanderHold::Low:
		return "in one hand, held low under the hip";
	case TwoHanderHold::Raised:
		return "in one hand, raised over the hip";
	case TwoHanderHold::Shoulder:
		return "in one hand, resting on a shoulder";
	default:
		return "not drawn in the hand";
	}
}

struct TwoHanderHoldInput {
	// A two-handed blade or blunt weapon drawn in the weapon hand, in the
	// world, first person, no menu.
	bool twoHanderDrawn = false;
	bool bothHands = false;  // the other hand holds its handle
	// The weapon hand's controller, metres up from the eyes along the room's
	// up (the body's frame z; negative under them).
	bool handValid = false;
	float handUp = 0.0f;
	// The drawn weapon from its grip to its tip, in the body's frame.
	bool weaponValid = false;
	NiPoint3 grip{0.0f, 0.0f, 0.0f};
	NiPoint3 tip{0.0f, 0.0f, 0.0f};
	float dtSeconds = 0.0f;
};

struct TwoHanderHoldState {
	TwoHanderHold hold = TwoHanderHold::None;
	float seconds = 0.0f;  // in this hold
	float burned = 0.0f;   // fatigue burned in this hold
};

struct TwoHanderHoldVerdict {
	TwoHanderHold hold = TwoHanderHold::None;
	TwoHanderHold was = TwoHanderHold::None;
	bool changed = false;
	// In one hand: the costs of fighting (OneHandScales) times costScale,
	// the lag times weightFactor (both 1 otherwise).
	bool oneHanded = false;
	float costScale = 1.0f;
	float weightFactor = 1.0f;
	// The fatigue to burn this frame for holding it, paid as it is (not
	// scaled again).
	float burn = 0.0f;
	// The hand over (+) or under (-) the hip, metres; the shoulder's reading.
	float overHip = 0.0f;
	ShoulderRest shoulder;
	// The hold just ended: how long it lasted and what it burned.
	float endedSeconds = 0.0f;
	float endedBurned = 0.0f;
};

// One frame: which hold, and what it costs.
inline TwoHanderHoldVerdict StepTwoHanderHold(TwoHanderHoldState& s, const TwoHanderHoldInput& in,
                                              const OneHandTwoHanderSettings& set) {
	TwoHanderHoldVerdict v;
	v.was = s.hold;
	const float scale = HoldScaleOf(set.hipBelowEyes);
	const float hip = -(set.hipBelowEyes > 0.0f ? set.hipBelowEyes : kHipBelowEyesMetres);
	v.overHip = in.handValid ? in.handUp - hip : 0.0f;
	TwoHanderHold hold = TwoHanderHold::None;
	if (in.twoHanderDrawn) {
		if (in.bothHands) {
			hold = TwoHanderHold::BothHands;
		} else {
			if (in.weaponValid) {
				const float radius = s.hold == TwoHanderHold::Shoulder ? kShoulderRestStayMetres : kShoulderRestMetres;
				v.shoulder = ShoulderRestOf(in.grip, in.tip, scale, radius * scale);
			}
			if (v.shoulder.resting) {
				hold = TwoHanderHold::Shoulder;
			} else if (!in.handValid) {
				// No hand to read: the last height stands.
				hold = s.hold == TwoHanderHold::Raised ? TwoHanderHold::Raised : TwoHanderHold::Low;
			} else if (s.hold == TwoHanderHold::Raised) {
				hold = v.overHip < -kHipBandMetres ? TwoHanderHold::Low : TwoHanderHold::Raised;
			} else if (s.hold == TwoHanderHold::Low) {
				hold = v.overHip > kHipBandMetres ? TwoHanderHold::Raised : TwoHanderHold::Low;
			} else {
				hold = v.overHip > 0.0f ? TwoHanderHold::Raised : TwoHanderHold::Low;
			}
		}
	}
	const float dt = in.dtSeconds > 0.0f && in.dtSeconds < 0.25f ? in.dtSeconds : 0.0f;
	if (hold != s.hold) {
		v.changed = true;
		v.endedSeconds = s.seconds;
		v.endedBurned = s.burned;
		s.hold = hold;
		s.seconds = 0.0f;
		s.burned = 0.0f;
	}
	s.seconds += dt;
	v.hold = hold;
	v.oneHanded = hold == TwoHanderHold::Low || hold == TwoHanderHold::Raised || hold == TwoHanderHold::Shoulder;
	if (v.oneHanded) {
		v.costScale = PercentMore(set.fatiguePercent);
		v.weightFactor = PercentMore(set.weightPercent);
	}
	if (hold == TwoHanderHold::Raised && set.holdCosts && set.holdPerSecond > 0.0f && set.holdPerSecond < 1.0e4f) {
		v.burn = set.holdPerSecond * dt;
		s.burned += v.burn;
	}
	return v;
}

}  // namespace obvr::vr
