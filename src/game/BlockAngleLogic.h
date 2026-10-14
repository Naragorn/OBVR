#pragma once

#include <limits>

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The block's angle rule (docs/next-up.md 1; the tester, 2026-10-14: the
// weapon "eng", the shield "wesentlich großzügiger"), and its reach since
// the same night (the tester, in the headset: "die höhe der kreuzenden
// waffen scheinen egal zu sein").
//
// The engine blocks a blow for whoever is in the Block action with the
// attacker in front (docs/combat-comfort-spec.md, "How vanilla decides a
// block"); OBVR puts the player there for a guard pose (vr/HandInput.h,
// IsWeaponGuard, IsBlockGesture). Nothing asks how the guard stands to the
// blow. Now, at the blow, with the engine's own yes in hand (game/BlockCone):
//
//   the weapon  the player's blade and the attacker's have to cross - the
//               angle between the two lines within `bladeDegrees` of a right
//               angle - and meet: the two blades, as segments, within
//               `reachUnits` of each other at the blow, so a blade held over
//               the head does not block a cut at the belly. Without a blade
//               on the attacker (a creature, the fists), the player's blade
//               across the way the attacker comes, the line from the blade's
//               middle to the attacker's centre, and their body (the
//               skeleton's bound, a ball) within the reach of the blade.
//   the shield  its face turned to the blow - the disc's normal (either
//               side; a disc has two) within `shieldDegrees` of the line to
//               the attacker's blade, or to the attacker's centre without
//               one - and their blade (their body without one) within
//               `reachUnits` of the disc, its radius and thickness counted.
//
// A reach of 0 asks no distance: the angle alone decides, as before. The
// rule only takes blocks away; a blow the engine lets through stays
// through. The fists' and the bow's guards, a block the engine has on its
// own (the keyboard's), a guard this cannot read (no blade span, no shield
// disc) and an attacker it cannot place are the engine's as before -
// BlockAngle::Engine. Pure; block_angle_test.

enum class BlockWith : UInt8 { None, Shield, Weapon, Fists, Bow };

struct BlockAngleSettings {
	bool enabled = true;          // [Hands] BlockByAngle
	float bladeDegrees = 30.0f;   // [Hands] BlockBladeDegrees: the blades within this of a right angle
	float shieldDegrees = 70.0f;  // [Hands] BlockShieldDegrees: the shield's face within this of the blow
	float reachUnits = 40.0f;     // [Hands] BlockReachUnits: the blow within this of the guard; 0 asks no distance
};

// The player's guard this frame, in the world (vr::HandMode names the pose;
// the blade's span and the shield's disc come from the contact steps).
struct BlockGuard {
	BlockWith with = BlockWith::None;
	bool bladeValid = false;
	NiPoint3 bladeA{0.0f, 0.0f, 0.0f};
	NiPoint3 bladeB{0.0f, 0.0f, 0.0f};
	bool shieldValid = false;
	NiPoint3 shieldCentre{0.0f, 0.0f, 0.0f};
	NiPoint3 shieldNormal{0.0f, 0.0f, 1.0f};
	float shieldRadius = 0.0f;     // the disc's, as the shield's shape reads it (game/ShieldShape)
	float shieldHalfThick = 0.0f;  // half the disc's thickness
};

// The blow: the attacker's blade in the world when they hold one, and where
// they are (their skeleton's bound: its centre and radius).
struct BlockBlow {
	bool bladeValid = false;
	NiPoint3 bladeA{0.0f, 0.0f, 0.0f};
	NiPoint3 bladeB{0.0f, 0.0f, 0.0f};
	bool centreValid = false;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	float centreRadius = 0.0f;
};

enum class BlockAngle : UInt8 {
	Engine,   // the rule has nothing to say: the engine's verdict stands
	Blocked,  // the guard stands right to the blow and meets it
	Through,  // it does not: the blow lands
};

struct BlockAngleVerdict {
	BlockAngle verdict = BlockAngle::Engine;
	float degrees = 0.0f;  // the angle measured: between the blades, or off the shield's face
	float units = 0.0f;    // the distance measured: between the blades, their blade to the disc, or their body to the guard
	bool across = false;   // the angle's part: the guard stands right to the blow
	bool met = false;      // the distance's part: the blow comes within the reach, or no reach is asked
	const char* why = "";
};

inline float BlockDot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline NiPoint3 BlockCross(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// The angle between two lines, 0 (along) to 90 (across), degrees: the way
// round either points does not count. Not a number when either has no length.
inline float LineAngleDegrees(const NiPoint3& a, const NiPoint3& b) {
	const float dot = BlockDot(a, b);
	const float cross = math::Sqrt(BlockCross(a, b).LengthSquared());
	if (!(a.LengthSquared() > 0.0f) || !(b.LengthSquared() > 0.0f)) {
		return std::numeric_limits<float>::quiet_NaN();  // MSVC refuses a constant 0/0 (C2124)
	}
	return math::Atan2(cross, dot < 0.0f ? -dot : dot) * math::kRadiansToDegrees;
}

// A slack from the settings: 0 to 90 degrees, the default for a number that
// is none.
inline float BlockSlackDegrees(float setting, float fallback) {
	if (!(setting == setting)) {
		return fallback;
	}
	return setting < 0.0f ? 0.0f : setting > 90.0f ? 90.0f : setting;
}

// The reach from the settings: 0 asks no distance, a negative one the same,
// the default for a number that is none.
inline float BlockReachUnits(float setting, float fallback) {
	if (!(setting == setting)) {
		return fallback;
	}
	return setting < 0.0f ? 0.0f : setting;
}

// How far a body - a ball of `radius` about `centre` - stays from the
// segment a-b; 0 once it reaches in.
inline float BallSegmentGap(const NiPoint3& centre, float radius, const NiPoint3& a, const NiPoint3& b) {
	const float gap = PointSegmentDistance(centre, a, b) - radius;
	return gap > 0.0f ? gap : 0.0f;
}

// And from a disc.
inline float BallDiscGap(const NiPoint3& centre, float radius, const BladeDisc& d) {
	const float gap = PointDiscDistance(centre, d) - radius;
	return gap > 0.0f ? gap : 0.0f;
}

inline BlockAngleVerdict BlockAngleFor(const BlockAngleSettings& s, const BlockGuard& g, const BlockBlow& b) {
	BlockAngleVerdict v;
	if (!s.enabled) {
		v.why = "the rule is off";
		return v;
	}
	const float reach = BlockReachUnits(s.reachUnits, BlockAngleSettings{}.reachUnits);
	const NiPoint3 theirBlade = b.bladeB - b.bladeA;
	const bool theirs = b.bladeValid && theirBlade.LengthSquared() > 0.0f;
	if (g.with == BlockWith::Weapon) {
		const NiPoint3 ours = g.bladeB - g.bladeA;
		if (!g.bladeValid || !(ours.LengthSquared() > 0.0f)) {
			v.why = "our blade not read";
			return v;
		}
		const float slack = BlockSlackDegrees(s.bladeDegrees, BlockAngleSettings{}.bladeDegrees);
		if (theirs) {
			v.degrees = LineAngleDegrees(ours, theirBlade);
			v.units = SegmentSegmentDistance(g.bladeA, g.bladeB, b.bladeA, b.bladeB);
			v.why = "the blades";
		} else if (b.centreValid) {
			const NiPoint3 way = b.centre - (g.bladeA + g.bladeB) * 0.5f;
			v.degrees = LineAngleDegrees(ours, way);
			v.why = "the blade across their way";
			if (!(v.degrees == v.degrees)) {
				v.why = "they stand in the blade";
				return v;
			}
			v.units = BallSegmentGap(b.centre, b.centreRadius, g.bladeA, g.bladeB);
		} else {
			v.why = "neither their blade nor where they are";
			return v;
		}
		v.across = v.degrees >= 90.0f - slack;
		v.met = reach <= 0.0f || v.units <= reach;
		v.verdict = v.across && v.met ? BlockAngle::Blocked : BlockAngle::Through;
		return v;
	}
	if (g.with == BlockWith::Shield) {
		if (!g.shieldValid || !(g.shieldNormal.LengthSquared() > 0.0f)) {
			v.why = "the shield's disc not read";
			return v;
		}
		if (!(g.shieldRadius > 0.0f)) {
			v.why = "the shield's disc without a size";
			return v;
		}
		BladeDisc disc;
		disc.centre = g.shieldCentre;
		disc.normal = g.shieldNormal * (1.0f / math::Sqrt(g.shieldNormal.LengthSquared()));
		disc.radius = g.shieldRadius;
		disc.halfThick = g.shieldHalfThick > 0.0f ? g.shieldHalfThick : 0.0f;
		NiPoint3 to{0.0f, 0.0f, 0.0f};
		if (theirs) {
			to = (b.bladeA + b.bladeB) * 0.5f - g.shieldCentre;
			v.units = SegmentDiscDistance(b.bladeA, b.bladeB, disc);
			v.why = "the face to their blade";
		} else if (b.centreValid) {
			to = b.centre - g.shieldCentre;
			v.units = BallDiscGap(b.centre, b.centreRadius, disc);
			v.why = "the face to them";
		} else {
			v.why = "neither their blade nor where they are";
			return v;
		}
		v.degrees = LineAngleDegrees(g.shieldNormal, to);
		if (!(v.degrees == v.degrees)) {
			v.why = "they stand at the shield's centre";
			return v;
		}
		const float slack = BlockSlackDegrees(s.shieldDegrees, BlockAngleSettings{}.shieldDegrees);
		v.across = v.degrees <= slack;
		v.met = reach <= 0.0f || v.units <= reach;
		v.verdict = v.across && v.met ? BlockAngle::Blocked : BlockAngle::Through;
		return v;
	}
	v.why = g.with == BlockWith::Fists ? "the fists, as the game has it"
	        : g.with == BlockWith::Bow ? "the bow, as the game has it"
	                                   : "no guard of ours";
	return v;
}

}  // namespace obvr::game
