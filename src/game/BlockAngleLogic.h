#pragma once

#include <limits>

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/ShieldOutlineLogic.h"
#include "game/ShieldPlateLogic.h"
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

// Torch: the raised torch, its shaft judged as a blade (Nadi, 2026-10-31).
enum class BlockWith : UInt8 { None, Shield, Weapon, Fists, Bow, Torch };

// The blocks by contact's settings: the shield's ([Hands]
// ShieldBlockByContact, BlockShieldReachUnits, BlockShieldInterposeUnits -
// the switch and the two numbers its blows are judged by,
// BlockAngleSettings::shieldReachUnits, shieldInterposeUnits) and the drawn
// weapon's ([Hands] WeaponBlockByContact, judged by the weapon's own numbers).
struct ContactBlockSettings {
	bool shield = true;
	bool weapon = true;
	float shieldReachUnits = 25.0f;
	float shieldInterposeUnits = 6.0f;
};

// What the frame has read for the blocks by contact: the shield's disc
// (game/ShieldShape) and the drawn melee weapon's blade line (the span the
// angle rule judges, camera g_blockBladeValid: a blade or blunt weapon, one-
// or two-handed, drawn and read, not passing through someone).
struct GuardReads {
	bool shieldDisc = false;
	bool weaponBlade = false;
	bool torchShaft = false;  // the held torch's shaft in the world this frame (camera g_torchWorldValid)
	bool staffLine = false;   // the drawn staff's line (game/StaffLine.h), the last frame's
};

// The drawn weapon as a guard by contact ([Hands] WeaponBlockByContact; Nadi,
// 2026-11-27: "the weapons' block the same way as the shield's"). `byContact`:
// the rule holds this frame - on, a melee weapon drawn and its blade read -
// so the weapon's guard pose presses no Block. `raised`: the old guard pose
// (vr::IsWeaponGuard), for a blow the blade cannot be judged against.
// `swinging`: the weapon hand in a swing - a blade passing a blow mid-swing
// is no block (real contact mid-swing is the parry's).
struct WeaponGuardPose {
	bool byContact = false;
	bool raised = false;
	bool swinging = false;
};

struct BlockAngleSettings {
	bool enabled = true;          // [Hands] BlockByAngle
	float bladeDegrees = 30.0f;   // [Hands] BlockBladeDegrees: the blades within this of a right angle
	float shieldDegrees = 70.0f;  // [Hands] BlockShieldDegrees: the shield's face within this of the blow
	float reachUnits = 40.0f;     // [Hands] BlockReachUnits: the blow within this of the guard; 0 asks no distance
	// Against something without a blade, with the player's chest read: the
	// blade in its way - within `interposeUnits` of the line from it to the
	// chest - and not along that line, within `bodyDegrees` of a right angle
	// (Nadi, Windows 2026-10-27: "kreature/zombies blocken geht jz iwie
	// kaum" - his guards stood 28 to 57 degrees off their line, which the
	// blade rule's 60 refused).
	float bodyDegrees = 70.0f;     // [Hands] BlockBodyDegrees
	float interposeUnits = 25.0f;  // [Hands] BlockInterposeUnits
	// The shield guarding by contact (ShieldGuardPose::byContact; Nadi,
	// 2026-11-27, "oddly too generous"): its own reach to their blade, and how
	// far past its outline the line from the blow to the chest may cross it
	// (ShieldByContactVerdict). With the shield by the pose, reachUnits and the
	// circle as before.
	float shieldReachUnits = ContactBlockSettings{}.shieldReachUnits;  // [Hands] BlockShieldReachUnits; 0 asks no distance
	float shieldInterposeUnits = ContactBlockSettings{}.shieldInterposeUnits;  // [Hands] BlockShieldInterposeUnits
};

// The shield as a guard by contact ([Hands] ShieldBlockByContact; Nadi,
// 2026-11-27, "as Blade & Sorcery": no block state, a shield blocks a blow
// when it is in its way). `byContact`: the rule holds this frame - on, a
// shield worn and its disc read - so nothing presses the game's Block
// control for the shield and every blow at the player is judged against the
// disc (ParryLogic.h, ForceBlockForShield, ShieldConeFor). `raised`: the left
// hand held as the old pose rule raised it (vr::IsBlockGesture), for a blow
// the disc cannot be judged against (an arrow, an attacker not placed).
struct ShieldGuardPose {
	bool byContact = false;
	bool raised = false;
};

// What the player guards with this frame: the pose's guard while one holds
// the Block control, else the shield by contact, else the weapon by contact
// (judged after the shield's - BlockCone's ContactConeAt asks both), else
// nothing.
inline BlockWith GuardWithFor(bool poseBlocking, BlockWith poseWith, bool shieldByContact,
                              bool weaponByContact = false) {
	if (!poseBlocking && !shieldByContact && weaponByContact) {
		return BlockWith::Weapon;
	}
	if (poseBlocking) {
		return poseWith;
	}
	return shieldByContact ? BlockWith::Shield : BlockWith::None;
}

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
	ShieldGuardPose shieldPose;    // the shield by contact (ShieldGuardPose)
	WeaponGuardPose weaponPose;    // the weapon by contact (WeaponGuardPose)
	WeaponGuardPose torchPose;     // the torch by contact: the same fields, the left hand's raise and swing
	// The held torch's shaft in the world, judged as the blade when the torch
	// is the guard (BlockWith::Torch; game/BlockCone JudgeBlow).
	bool torchValid = false;
	NiPoint3 torchA{0.0f, 0.0f, 0.0f};
	NiPoint3 torchB{0.0f, 0.0f, 0.0f};
	// The drawn staff's line in the world (game/StaffLine.h): the weapon's
	// blade when no blade is read (GuardAs).
	bool staffValid = false;
	NiPoint3 staffA{0.0f, 0.0f, 0.0f};
	NiPoint3 staffB{0.0f, 0.0f, 0.0f};
	// The disc's in-plane axes in the world (its axis[0] and axis[1]) and its
	// outline along them, for the arrows (game/ArrowShieldLogic.h); the melee
	// rule above takes the circle of shieldRadius.
	NiPoint3 shieldAxisA{1.0f, 0.0f, 0.0f};
	NiPoint3 shieldAxisB{0.0f, 1.0f, 0.0f};
	const ShieldOutline* shieldOutline = nullptr;
	// The player's upper body, for a blow without a blade: where it comes
	// at - from the neck (`body`) down to the belly (`bodyLow`).
	bool bodyValid = false;
	NiPoint3 body{0.0f, 0.0f, 0.0f};
	NiPoint3 bodyLow{0.0f, 0.0f, 0.0f};
};

// The guard as judged with `with`: the torch's shaft, or the drawn staff's
// line when no blade is read, stands for the blade (game/BlockCone
// JudgeBlow); anything else as it is.
inline BlockGuard GuardAs(const BlockGuard& g, BlockWith with) {
	BlockGuard out = g;
	out.with = with;
	if (with == BlockWith::Torch) {
		out.bladeValid = g.torchValid;
		out.bladeA = g.torchA;
		out.bladeB = g.torchB;
	} else if (with == BlockWith::Weapon && !g.bladeValid && g.staffValid) {
		out.bladeValid = true;
		out.bladeA = g.staffA;
		out.bladeB = g.staffB;
	}
	return out;
}

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
	float interposeUnits = -1.0f;  // a blow without a blade: the blade off its line to the chest; -1 not asked
	const char* test = "";       // the shield by contact: what decided (ShieldByContactVerdict)
	float betweenUnits = -1.0f;  // the shield by contact: the line from the blow to the chest off its face; -1 not asked
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

// How far the blade stays off the ways a blow without a blade can take: the
// lines from the attacker's centre to the player's upper body, neck to belly
// (five of them, evenly), the nearest. A guard before the face is in the way
// of a claw at the chest as much as one before the chest.
inline float InterposeUnits(const NiPoint3& bladeA, const NiPoint3& bladeB, const NiPoint3& from, const NiPoint3& top,
                            const NiPoint3& bottom) {
	constexpr int kWays = 5;
	float best = -1.0f;
	for (int i = 0; i < kWays; ++i) {
		const float t = static_cast<float>(i) / static_cast<float>(kWays - 1);
		const NiPoint3 to = top + (bottom - top) * t;
		const float d = SegmentSegmentDistance(bladeA, bladeB, from, to);
		if (best < 0.0f || d < best) {
			best = d;
		}
	}
	return best;
}

// The guard's shield as a plate: its outline when read (and its in-plane
// axes have a length), else the circle of the longer spoke.
inline ShieldPlate PlateOf(const BlockGuard& g) {
	ShieldPlate p;
	p.centre = g.shieldCentre;
	p.normal = g.shieldNormal * (1.0f / math::Sqrt(g.shieldNormal.LengthSquared()));
	p.radius = g.shieldRadius;
	p.halfThick = g.shieldHalfThick > 0.0f ? g.shieldHalfThick : 0.0f;
	const float la = g.shieldAxisA.LengthSquared();
	const float lb = g.shieldAxisB.LengthSquared();
	if (g.shieldOutline != nullptr && g.shieldOutline->valid && la > 0.0f && lb > 0.0f) {
		p.outline = g.shieldOutline;
		p.axisA = g.shieldAxisA * (1.0f / math::Sqrt(la));
		p.axisB = g.shieldAxisB * (1.0f / math::Sqrt(lb));
	}
	return p;
}

// A blow at the shield guarding by contact (ShieldGuardPose::byContact; Nadi,
// 2026-11-27: blocking felt "oddly too generous"). Its face turned to the
// blow, as by the pose (within shieldDegrees of the line to their blade's
// middle, or to their centre without a blade), and in its way:
// - a blade: within the shield's own reach (shieldReachUnits) of the plate -
//   the outline given its thickness, not the circle of the longer spoke -
//   or, failing that, the plate between the blade's middle and the nearest
//   point of the chest (neck to belly), within shieldInterposeUnits of its
//   face: the engine settles a blow before the blades meet (10 to 37 units
//   apart for guards in the way, the tally in free-weapons-spec), so a
//   shield squarely in the way may still be well short of the blade then;
// - without a blade (a creature, the fists): the plate between their centre
//   and the chest, within shieldInterposeUnits - their body's ball was
//   always within the reach, so the face alone decided; without the chest
//   read, their ball within the shield's reach of the plate.
// No outline read: the same tests with the circle.
inline BlockAngleVerdict ShieldByContactVerdict(const BlockAngleSettings& s, const BlockGuard& g, const BlockBlow& b) {
	BlockAngleVerdict v;
	const ShieldPlate plate = PlateOf(g);
	const bool byOutline = plate.ByOutline();
	const NiPoint3 theirBlade = b.bladeB - b.bladeA;
	const bool theirs = b.bladeValid && theirBlade.LengthSquared() > 0.0f;
	NiPoint3 from{0.0f, 0.0f, 0.0f};
	if (theirs) {
		from = (b.bladeA + b.bladeB) * 0.5f;
		v.why = "the face to their blade";
	} else if (b.centreValid) {
		from = b.centre;
		v.why = "the face to them";
	} else {
		v.why = "neither their blade nor where they are";
		return v;
	}
	v.degrees = LineAngleDegrees(g.shieldNormal, from - g.shieldCentre);
	if (!(v.degrees == v.degrees)) {
		v.why = "they stand at the shield's centre";
		return v;
	}
	v.across = v.degrees <= BlockSlackDegrees(s.shieldDegrees, BlockAngleSettings{}.shieldDegrees);
	const float reach = BlockReachUnits(s.shieldReachUnits, BlockAngleSettings{}.shieldReachUnits);
	const float margin = s.shieldInterposeUnits == s.shieldInterposeUnits && s.shieldInterposeUnits >= 0.0f
	                         ? s.shieldInterposeUnits
	                         : BlockAngleSettings{}.shieldInterposeUnits;
	const NiPoint3 chest = NearestOnSegment(from, g.body, g.bodyLow);
	if (theirs) {
		v.units = SegmentPlateDistance(b.bladeA, b.bladeB, plate);
		v.met = reach <= 0.0f || v.units <= reach;
		v.test = byOutline ? "the outline's distance" : "the circle's distance";
		if (!v.met && g.bodyValid) {
			v.met = PlateBetween(from, chest, plate, margin, v.betweenUnits);
			v.test = byOutline ? "the outline's distance, then between, by the outline"
			                   : "the circle's distance, then between, by the circle";
		}
	} else if (g.bodyValid) {
		v.met = PlateBetween(from, chest, plate, margin, v.betweenUnits);
		v.units = v.betweenUnits;
		v.test = byOutline ? "between, by the outline" : "between, by the circle";
	} else {
		const float gap = PointPlateDistance(b.centre, plate) - b.centreRadius;
		v.units = gap > 0.0f ? gap : 0.0f;
		v.met = reach <= 0.0f || v.units <= reach;
		v.test = byOutline ? "their body's distance to the outline" : "their body's distance to the circle";
	}
	v.verdict = v.across && v.met ? BlockAngle::Blocked : BlockAngle::Through;
	return v;
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
	if (g.with == BlockWith::Weapon || g.with == BlockWith::Torch) {
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
		} else if (b.centreValid && g.bodyValid && (b.centre - g.body).LengthSquared() > 0.0f) {
			// In their way: their line to the chest, the blade on it and not
			// along it.
			v.degrees = LineAngleDegrees(ours, (g.body + g.bodyLow) * 0.5f - b.centre);
			v.why = "the blade in their way";
			v.units = BallSegmentGap(b.centre, b.centreRadius, g.bladeA, g.bladeB);
			v.interposeUnits = InterposeUnits(g.bladeA, g.bladeB, b.centre, g.body, g.bodyLow);
			const float bodySlack = BlockSlackDegrees(s.bodyDegrees, BlockAngleSettings{}.bodyDegrees);
			const float interpose = s.interposeUnits == s.interposeUnits && s.interposeUnits >= 0.0f
			                            ? s.interposeUnits
			                            : BlockAngleSettings{}.interposeUnits;
			v.across = v.degrees >= 90.0f - bodySlack && v.interposeUnits <= interpose;
			v.met = reach <= 0.0f || v.units <= reach;
			v.verdict = v.across && v.met ? BlockAngle::Blocked : BlockAngle::Through;
			return v;
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
		if (g.shieldPose.byContact) {
			return ShieldByContactVerdict(s, g, b);
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
