#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/GrabPhysics.h"
#include "game/HandBodyLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The drawn weapon stops at walls (docs/weapon-collision-spec.md, the
// tester, 2026-10-09: "waffen kollisionen mit allem ... fang an").
//
// The controller is where the player's hand is; the DRAWN weapon - the pose
// the weight spring gives (vr/WeaponWeight.h), which the bones, the Havok
// body and the strike all use - is swept each frame from where it was drawn
// last to where it would be drawn now, and held where it first meets
// something fixed: a wall, the floor, a table, a tree, a door. What is left
// of the frame's motion along the surface is swept once more, so a blade
// pressed to a wall scrapes along it. Clutter and the dead do not stop it -
// a ray goes on past them, and what it passed is handed back to be kicked
// (WorldPush's push), so a fast swing knocks a cup it would have tunnelled
// through between two physics steps.
//
// Further from the hand than a cap (0.30 m or 45 degrees by default), the
// weapon lets go and passes through, Blade & Sorcery's "too far" case; the
// tester chose it (2026-10-09, decision 1: "Variante A"). A blade that went
// through - let go, or found inside something after a teleport - strikes
// nothing until it is free again: neither inside anything along its length
// nor behind something seen from the eyes.
//
// Kinematic and OBVR's own, not a Havok joint: the living are only a
// character controller in Havok's world, and a dynamic blade tunnels as the
// keyframed body does (the spec, section 3.1). Pure: the world is asked
// through `World::Cast(from, to)`, so every flow is checked against planes
// and boxes in blade_contact_test.

// What a ray met, as the blade takes it.
enum class ContactKind : UInt8 {
	None,
	Fixed,    // stops the blade: a fixed or keyframed body (walls, the ground, doors)
	Movable,  // a body the blade pushes on through: clutter, a ragdoll
	Ignored,  // a body the blade does not notice: water, triggers, controllers, picks
};

// Oblivion's Havok layers (the table at 0x00B2EB40, hand-weapon-collision-
// spec.md) a blade passes through whatever their bodies' motion.
inline constexpr UInt32 kContactLayerWater = 11;
inline constexpr UInt32 kContactLayerTrigger = 12;
inline constexpr UInt32 kContactLayerNonCollidable = 15;
inline constexpr UInt32 kContactLayerCharController = 20;
inline constexpr UInt32 kContactLayerAvoidBox = 21;
inline constexpr UInt32 kContactLayerFirstPick = 24;  // CAMERAPICK .. DROPPINGPICK: 24-31
inline constexpr UInt32 kContactLayerLastPick = 31;

inline ContactKind ContactKindOf(bool isBody, UInt32 motionType, UInt32 layer) {
	if (!isBody) {
		return ContactKind::Ignored;
	}
	if (layer == kContactLayerWater || layer == kContactLayerTrigger || layer == kContactLayerNonCollidable ||
	    layer == kContactLayerCharController || layer == kContactLayerAvoidBox ||
	    (layer >= kContactLayerFirstPick && layer <= kContactLayerLastPick)) {
		return ContactKind::Ignored;
	}
	if (motionType == kMotionTypeKeyframed || motionType == kMotionTypeFixed) {
		return ContactKind::Fixed;
	}
	return MotionTypeDrivable(motionType) ? ContactKind::Movable : ContactKind::Ignored;
}

// One ray's answer, from the world.
struct BladeRayHit {
	bool hit = false;
	float fraction = 1.0f;  // along the ray asked, 0..1
	NiPoint3 point{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 1.0f};
	ContactKind kind = ContactKind::None;
	UInt32 body = 0;
};

// A ray goes past at most this many things that do not stop a blade, each
// time from just beyond the last.
inline constexpr UInt32 kBladeCastPasses = 4;
inline constexpr float kBladePastUnits = 0.25f;

// The first thing on from -> to that stops a blade; its fraction is along
// the whole segment. Each movable thing passed on the way is handed to
// `onMovable(hit)`.
template <class World, class OnMovable>
BladeRayHit FirstStopper(World& world, const NiPoint3& from, const NiPoint3& to, OnMovable&& onMovable) {
	const NiPoint3 ray = to - from;
	const float length = math::Sqrt(ray.LengthSquared());
	if (!(length > 1e-4f)) {
		return BladeRayHit{};
	}
	const NiPoint3 dir = ray * (1.0f / length);
	float startAt = 0.0f;
	for (UInt32 pass = 0; pass < kBladeCastPasses; ++pass) {
		BladeRayHit h = world.Cast(from + dir * startAt, to);
		if (!h.hit) {
			return BladeRayHit{};
		}
		const float at = startAt + h.fraction * (length - startAt);
		h.fraction = at / length;
		if (h.kind == ContactKind::Fixed) {
			return h;
		}
		if (h.kind == ContactKind::Movable) {
			onMovable(h);
		}
		startAt = at + kBladePastUnits;
		if (startAt >= length) {
			return BladeRayHit{};
		}
	}
	return BladeRayHit{};
}

template <class World>
BladeRayHit FirstStopper(World& world, const NiPoint3& from, const NiPoint3& to) {
	return FirstStopper(world, from, to, [](const BladeRayHit&) {});
}

// ------------------------------------------------------------------ poses

// The drawn weapon hand in the world: the grip, turned. The blade is a span
// in the grip's own frame (HandBodyLogic.h, BladeSpanFromNode).
struct BladePose {
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
};

inline NiPoint3 BladePointAt(const BladePose& p, const BodySpan& span, float s) {
	return p.pos + p.rot * (span.a + (span.b - span.a) * s);
}

inline float DotOf3(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// The turn from `from` to `to` (world axes: to = turn * from) as a unit axis
// and an angle 0..pi, the short way round. False for no turn.
inline bool TurnBetween(const NiMatrix33& from, const NiMatrix33& to, NiPoint3& axis, float& angle) {
	NiMatrix33 turn;
	for (UInt32 i = 0; i < 3; ++i) {
		for (UInt32 j = 0; j < 3; ++j) {
			float sum = 0.0f;
			for (UInt32 k = 0; k < 3; ++k) {
				sum += to.data[i][k] * from.data[j][k];
			}
			turn.data[i][j] = sum;
		}
	}
	float q[4];
	QuaternionFromRotation(turn, q);
	if (q[3] < 0.0f) {
		q[0] = -q[0];
		q[1] = -q[1];
		q[2] = -q[2];
		q[3] = -q[3];
	}
	const float sine = math::Sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
	if (!(sine > 1e-7f)) {
		axis = NiPoint3{0.0f, 0.0f, 1.0f};
		angle = 0.0f;
		return false;
	}
	axis = NiPoint3{q[0] / sine, q[1] / sine, q[2] / sine};
	angle = 2.0f * math::Atan2(sine, q[3]);
	return true;
}

inline float AngleBetweenRotations(const NiMatrix33& a, const NiMatrix33& b) {
	NiPoint3 axis;
	float angle = 0.0f;
	TurnBetween(a, b, axis, angle);
	return angle;
}

// A turn of `angle` about the unit `axis` (Rodrigues).
inline NiMatrix33 RotationAbout(const NiPoint3& k, float angle) {
	const float c = math::Cos(angle);
	const float s = math::Sin(angle);
	const float t = 1.0f - c;
	NiMatrix33 m;
	m.data[0][0] = c + k.x * k.x * t;
	m.data[0][1] = k.x * k.y * t - k.z * s;
	m.data[0][2] = k.x * k.z * t + k.y * s;
	m.data[1][0] = k.y * k.x * t + k.z * s;
	m.data[1][1] = c + k.y * k.y * t;
	m.data[1][2] = k.y * k.z * t - k.x * s;
	m.data[2][0] = k.z * k.x * t - k.y * s;
	m.data[2][1] = k.z * k.y * t + k.x * s;
	m.data[2][2] = c + k.z * k.z * t;
	return m;
}

// The pose this share of the way from a to b: the grip along the line, the
// turn about one axis.
inline BladePose PoseBetween(const BladePose& a, const BladePose& b, float share) {
	BladePose p;
	p.pos = a.pos + (b.pos - a.pos) * share;
	NiPoint3 axis;
	float angle = 0.0f;
	p.rot = TurnBetween(a.rot, b.rot, axis, angle) ? RotationAbout(axis, angle * share) * a.rot : a.rot;
	return p;
}

// ------------------------------------------------------------------ the sweep

// The points of the blade swept: the guard, the quarters, the tip. Between
// two of them (a longsword's are 17 units apart) a thin post can be passed
// by a turning blade; the ray along the blade at the end catches one the
// blade would end up across.
inline constexpr UInt32 kBladeSweepPoints = 5;
inline constexpr float kBladeSweepAt[kBladeSweepPoints] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
// A point that moved less than this sweeps nothing (WorldPush's floor).
inline constexpr float kBladeMinSweepUnits = 0.5f;
// The blade is held this far short of a surface along its path: about its
// own half-width, and the frame the bones are pinned a frame later.
inline constexpr float kBladeContactMarginUnits = 1.5f;

// What a blade passed that moves, and the speed to give it (units a second).
struct BladeKick {
	UInt32 body = 0;
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
};

inline constexpr UInt32 kBladeKicksMax = 8;

struct BladeKicks {
	UInt32 count = 0;
	BladeKick kick[kBladeKicksMax];

	// Once per body: the fastest point that passed it.
	void Add(UInt32 body, const NiPoint3& velocity) {
		for (UInt32 i = 0; i < count; ++i) {
			if (kick[i].body == body) {
				if (velocity.LengthSquared() > kick[i].velocity.LengthSquared()) {
					kick[i].velocity = velocity;
				}
				return;
			}
		}
		if (count < kBladeKicksMax) {
			kick[count].body = body;
			kick[count].velocity = velocity;
			++count;
		}
	}
};

// ------------------------------------------------------------------ the living
//
// A living person is only a character controller in Havok's world (their
// bones are not in it while animated, hand-weapon-collision-spec.md), so
// the blade meets them as capsules of OBVR's own on their drawn bones
// (game::CollectBladeBodies). The tester (2026-10-09, decision 2): "Langsam
// an einen NPC gehalten liegt die Klinge auf" - held there slowly, the blade
// rests on them; a swing goes through, hits (the strike by motion) and
// passes on. As in PLANCK (nexusmods.com/skyrimspecialedition/mods/66025):
// once the blade has gone into someone, they are passed until it has been
// out of them for 0.22 s (its hitCooldownTimeStoppedColliding) - a swing
// that ends inside a body does not get stuck in it, and nor does a blade
// someone walked into.

// The closest point of segment a-b to p, as a share 0..1 along it.
inline float ShareOnSegment(const NiPoint3& p, const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 ab = b - a;
	const float abab = DotOf3(ab, ab);
	if (!(abab > 1e-12f)) {
		return 0.0f;
	}
	const float t = DotOf3(p - a, ab) / abab;
	return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

inline float PointSegmentDistance(const NiPoint3& p, const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 q = a + (b - a) * ShareOnSegment(p, a, b);
	return math::Sqrt((p - q).LengthSquared());
}

// The distance between two segments (Ericson, Real-Time Collision
// Detection, 5.1.9: the closest points, clamped to both).
inline float SegmentSegmentDistance(const NiPoint3& p1, const NiPoint3& q1, const NiPoint3& p2, const NiPoint3& q2) {
	const NiPoint3 d1 = q1 - p1;
	const NiPoint3 d2 = q2 - p2;
	const NiPoint3 r = p1 - p2;
	const float a = DotOf3(d1, d1);
	const float e = DotOf3(d2, d2);
	const float f = DotOf3(d2, r);
	float s = 0.0f;
	float t = 0.0f;
	if (!(a > 1e-12f) && !(e > 1e-12f)) {
		return math::Sqrt(r.LengthSquared());
	}
	if (!(a > 1e-12f)) {
		t = f / e;
		t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	} else {
		const float c = DotOf3(d1, r);
		if (!(e > 1e-12f)) {
			s = -c / a;
			s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
		} else {
			const float b = DotOf3(d1, d2);
			const float denom = a * e - b * b;
			s = denom > 1e-12f ? (b * f - c * e) / denom : 0.0f;
			s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
			t = (b * s + f) / e;
			if (t < 0.0f) {
				t = 0.0f;
				s = -c / a;
				s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
			} else if (t > 1.0f) {
				t = 1.0f;
				s = (b - c) / a;
				s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
			}
		}
	}
	const NiPoint3 gap = (p1 + d1 * s) - (p2 + d2 * t);
	return math::Sqrt(gap.LengthSquared());
}

// Where the segment from -> to first enters the capsule a-b of radius r: its
// fraction and the capsule's outward normal there. `inside` when `from` is
// already in it (then fraction 0). False when it never enters.
inline bool SegmentEntersCapsule(const NiPoint3& from, const NiPoint3& to, const NiPoint3& a, const NiPoint3& b,
                                 float r, float& fraction, NiPoint3& normal, bool& inside) {
	inside = false;
	if (!(r > 0.0f)) {
		return false;
	}
	if (PointSegmentDistance(from, a, b) <= r) {
		inside = true;
		fraction = 0.0f;
		normal = NiPoint3{0.0f, 0.0f, 1.0f};
		return true;
	}
	const NiPoint3 d = to - from;
	const float length = math::Sqrt(d.LengthSquared());
	if (!(length > 1e-6f)) {
		return false;
	}
	const NiPoint3 rd = d * (1.0f / length);
	float best = -1.0f;
	// The side: |(o + t rd - a) x (b - a)| = r |b - a|, between the ends.
	const NiPoint3 ba = b - a;
	const NiPoint3 oa = from - a;
	const float baba = DotOf3(ba, ba);
	if (baba > 1e-12f) {
		const float bard = DotOf3(ba, rd);
		const float baoa = DotOf3(ba, oa);
		const float qa = baba - bard * bard;
		const float qb = baba * DotOf3(rd, oa) - baoa * bard;
		const float qc = baba * DotOf3(oa, oa) - baoa * baoa - r * r * baba;
		const float h = qb * qb - qa * qc;
		if (qa > 1e-9f && h >= 0.0f) {
			const float t = (-qb - math::Sqrt(h)) / qa;
			const float y = baoa + t * bard;
			if (t >= 0.0f && y > 0.0f && y < baba) {
				best = t;
			}
		}
	}
	// The two round ends.
	const NiPoint3 ends[2] = {a, b};
	for (const NiPoint3& c : ends) {
		const NiPoint3 oc = from - c;
		const float hb = DotOf3(rd, oc);
		const float hc = DotOf3(oc, oc) - r * r;
		const float h = hb * hb - hc;
		if (h > 0.0f) {
			const float t = -hb - math::Sqrt(h);
			if (t >= 0.0f && (best < 0.0f || t < best)) {
				best = t;
			}
		}
	}
	if (!(best >= 0.0f) || best > length) {
		return false;
	}
	fraction = best / length;
	const NiPoint3 p = from + rd * best;
	const NiPoint3 q = a + ba * ShareOnSegment(p, a, b);
	const NiPoint3 out = p - q;
	const float outLength = math::Sqrt(out.LengthSquared());
	normal = outLength > 1e-6f ? out * (1.0f / outLength) : rd * -1.0f;
	return true;
}

// A capsule on the bones of someone living: which actor it belongs to.
struct BladeBodyCapsule {
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
	UInt32 actor = 0;
};

inline constexpr UInt32 kBladeBodyCapsulesMax = 128;

struct BladeBodies {
	UInt32 count = 0;
	BladeBodyCapsule cap[kBladeBodyCapsulesMax];

	bool Add(const NiPoint3& a, const NiPoint3& b, float radius, UInt32 actor) {
		if (count >= kBladeBodyCapsulesMax || !(radius > 0.0f)) {
			return false;
		}
		cap[count].a = a;
		cap[count].b = b;
		cap[count].radius = radius;
		cap[count].actor = actor;
		++count;
		return true;
	}
};

// The bones a person's capsules are built on, in this order (the Biped
// names Oblivion's skeletons carry; which a skeleton has is logged).
enum BladeBone : UInt32 {
	kBoneHead, kBoneNeck, kBoneSpine2, kBoneSpine1, kBoneSpine, kBonePelvis,
	kBoneLUpperArm, kBoneLForearm, kBoneLHand, kBoneRUpperArm, kBoneRForearm, kBoneRHand,
	kBoneLThigh, kBoneLCalf, kBoneLFoot, kBoneRThigh, kBoneRCalf, kBoneRFoot,
	kBladeBoneCount
};

inline const char* BladeBoneName(UInt32 bone) {
	static const char* const kNames[kBladeBoneCount] = {
		"Bip01 Head",     "Bip01 Neck",       "Bip01 Spine2",    "Bip01 Spine1",     "Bip01 Spine",
		"Bip01 Pelvis",   "Bip01 L UpperArm", "Bip01 L Forearm", "Bip01 L Hand",     "Bip01 R UpperArm",
		"Bip01 R Forearm", "Bip01 R Hand",    "Bip01 L Thigh",   "Bip01 L Calf",     "Bip01 L Foot",
		"Bip01 R Thigh",  "Bip01 R Calf",     "Bip01 R Foot",
	};
	return bone < kBladeBoneCount ? kNames[bone] : "";
}

// The flesh round each bone, game units at scale 1 (70 a metre): proposed
// for a human - a head 10 cm round, a torso 14, an upper arm 6, a forearm
// 5, a thigh 9, a calf 6 - not measured on the meshes. The head reaches
// this far past its bone, away from the neck (the head bone sits at the
// skull's base).
inline constexpr float kBladeHeadRadius = 7.0f;
inline constexpr float kBladeHeadAbove = 9.0f;
inline constexpr float kBladeNeckRadius = 4.5f;
inline constexpr float kBladeTorsoRadius = 10.0f;
inline constexpr float kBladeUpperArmRadius = 4.5f;
inline constexpr float kBladeForearmRadius = 3.5f;
inline constexpr float kBladeThighRadius = 6.5f;
inline constexpr float kBladeCalfRadius = 4.5f;
// Fewer bones than this read and the body is the bound's column instead.
inline constexpr UInt32 kBladeBonesNeeded = 6;

// A person's capsules from their bones (`have` says which were read), the
// radii times their scale; answers how many were added. With fewer than
// kBladeBonesNeeded bones, none: the caller takes the column.
inline UInt32 BodyCapsulesFromBones(const NiPoint3* bones, const bool* have, float scale, UInt32 actor,
                                    BladeBodies& out) {
	UInt32 read = 0;
	for (UInt32 i = 0; i < kBladeBoneCount; ++i) {
		read += have[i] ? 1u : 0u;
	}
	if (read < kBladeBonesNeeded) {
		return 0;
	}
	const float k = scale > 0.0f && scale < 20.0f ? scale : 1.0f;
	UInt32 added = 0;
	auto link = [&](UInt32 from, UInt32 to, float radius) {
		if (have[from] && have[to] && out.Add(bones[from], bones[to], radius * k, actor)) {
			++added;
		}
	};
	if (have[kBoneHead]) {
		NiPoint3 up{0.0f, 0.0f, 1.0f};
		if (have[kBoneNeck]) {
			const NiPoint3 d = bones[kBoneHead] - bones[kBoneNeck];
			const float l = math::Sqrt(d.LengthSquared());
			if (l > 1e-3f) {
				up = d * (1.0f / l);
			}
		}
		if (out.Add(bones[kBoneHead], bones[kBoneHead] + up * (kBladeHeadAbove * k), kBladeHeadRadius * k, actor)) {
			++added;
		}
	}
	link(kBoneNeck, kBoneHead, kBladeNeckRadius);
	link(kBoneSpine2, kBoneNeck, kBladeTorsoRadius);
	link(kBoneSpine, kBoneSpine2, kBladeTorsoRadius);
	link(kBonePelvis, kBoneSpine, kBladeTorsoRadius);
	link(kBoneLUpperArm, kBoneLForearm, kBladeUpperArmRadius);
	link(kBoneLForearm, kBoneLHand, kBladeForearmRadius);
	link(kBoneRUpperArm, kBoneRForearm, kBladeUpperArmRadius);
	link(kBoneRForearm, kBoneRHand, kBladeForearmRadius);
	link(kBoneLThigh, kBoneLCalf, kBladeThighRadius);
	link(kBoneLCalf, kBoneLFoot, kBladeCalfRadius);
	link(kBoneRThigh, kBoneRCalf, kBladeThighRadius);
	link(kBoneRCalf, kBoneRFoot, kBladeCalfRadius);
	return added;
}

// A body without the bones: an upright column of its bound - the shove's
// reach (game::HandAtBody: half the radius wide, the radius up and down),
// a little narrower.
inline bool BodyColumnFromBound(const NiPoint3& centre, float radius, UInt32 actor, BladeBodies& out) {
	if (!(radius > 0.0f && radius < 4096.0f)) {
		return false;
	}
	const NiPoint3 up{0.0f, 0.0f, radius * 0.8f};
	return out.Add(centre - up, centre + up, radius * 0.45f, actor);
}

// Those the blade has gone into: passed until it has been out of them for
// a while.
inline constexpr float kBladePassClearSeconds = 0.22f;
inline constexpr UInt32 kBladePassedMax = 8;

struct BladePassLedger {
	UInt32 count = 0;
	UInt32 actor[kBladePassedMax] = {};
	float clear[kBladePassedMax] = {};

	bool Has(UInt32 a) const {
		for (UInt32 i = 0; i < count; ++i) {
			if (actor[i] == a) {
				return true;
			}
		}
		return false;
	}

	// Answers whether it is new.
	bool Add(UInt32 a) {
		for (UInt32 i = 0; i < count; ++i) {
			if (actor[i] == a) {
				clear[i] = 0.0f;
				return false;
			}
		}
		if (count >= kBladePassedMax) {
			return false;
		}
		actor[count] = a;
		clear[count] = 0.0f;
		++count;
		return true;
	}

	// The blade where it is now: each one passed it is out of counts the
	// time, and is let go of after kBladePassClearSeconds; one in no
	// capsule of this frame (gone, dead, too far) at once.
	void Step(const NiPoint3& guard, const NiPoint3& tip, const BladeBodies* bodies, float dtSeconds) {
		UInt32 kept = 0;
		for (UInt32 i = 0; i < count; ++i) {
			bool seen = false;
			bool touching = false;
			for (UInt32 c = 0; bodies != nullptr && c < bodies->count; ++c) {
				const BladeBodyCapsule& cap = bodies->cap[c];
				if (cap.actor != actor[i]) {
					continue;
				}
				seen = true;
				touching = touching || SegmentSegmentDistance(guard, tip, cap.a, cap.b) <= cap.radius;
			}
			float t = touching ? 0.0f : clear[i] + (dtSeconds > 0.0f ? dtSeconds : 0.0f);
			if (!seen || t >= kBladePassClearSeconds) {
				continue;
			}
			actor[kept] = actor[i];
			clear[kept] = t;
			++kept;
		}
		count = kept;
	}
};

struct BladeSweep {
	bool hit = false;
	float fraction = 1.0f;      // of the move, where the first point met something fixed
	float safeFraction = 1.0f;  // held short of it by the margin
	float along = 0.0f;         // the point of the blade that met it: 0 the guard, 1 the tip
	NiPoint3 point{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 1.0f};  // of the surface, against the point's way
	UInt32 body = 0;
	float speedIn = 0.0f;  // units a second into the surface
	UInt32 actor = 0;      // a person it rests on (body 0), or 0
	bool entered = false;  // a swing went into someone not passed yet
};

// What the living are to this sweep: their capsules, those passed, and
// whether the blade swings (then it goes into them and passes them).
struct BladeLiving {
	const BladeBodies* bodies = nullptr;
	BladePassLedger* passed = nullptr;
	bool swinging = false;
};

// The blade's points from where they were to where they would be, the
// earliest that meets something fixed - or, held slowly, someone living.
// Each point's ray reaches the margin past its end, so a point is never
// left nearer a surface than the margin - a ray that began on a face could
// not see it. Movable things passed are kicked with the speed of the point
// that passed them (none without a time); the living a swing goes into, and
// any a point is already inside, are passed.
template <class World>
BladeSweep SweepBlade(World& world, const BladePose& from, const BladePose& to, const BodySpan& span, float dtSeconds,
                      BladeKicks* kicks, const BladeLiving& living = BladeLiving{}) {
	BladeSweep best;
	for (UInt32 i = 0; i < kBladeSweepPoints; ++i) {
		const float s = kBladeSweepAt[i];
		const NiPoint3 a = BladePointAt(from, span, s);
		const NiPoint3 b = BladePointAt(to, span, s);
		const NiPoint3 way = b - a;
		const float length = math::Sqrt(way.LengthSquared());
		if (!(length >= kBladeMinSweepUnits)) {
			continue;
		}
		const NiPoint3 dir = way * (1.0f / length);
		const float reach = length + kBladeContactMarginUnits;
		const NiPoint3 end = a + dir * reach;
		const NiPoint3 velocity = dtSeconds > 0.0f ? way * (1.0f / dtSeconds) : NiPoint3{0.0f, 0.0f, 0.0f};
		const BladeRayHit h = FirstStopper(world, a, end, [&](const BladeRayHit& m) {
			if (kicks != nullptr && dtSeconds > 0.0f && m.fraction * reach <= length) {
				kicks->Add(m.body, velocity);
			}
		});
		float atUnits = h.hit ? h.fraction * reach : -1.0f;
		NiPoint3 normal = h.normal;
		NiPoint3 point = h.point;
		UInt32 body = h.body;
		UInt32 actor = 0;
		for (UInt32 c = 0; living.bodies != nullptr && c < living.bodies->count; ++c) {
			const BladeBodyCapsule& cap = living.bodies->cap[c];
			if (living.passed != nullptr && living.passed->Has(cap.actor)) {
				continue;
			}
			float f = 1.0f;
			NiPoint3 n;
			bool inside = false;
			if (!SegmentEntersCapsule(a, end, cap.a, cap.b, cap.radius, f, n, inside)) {
				continue;
			}
			const float at = f * reach;
			if (inside || living.swinging) {
				// Inside already, or a swing into them within this move: passed.
				if ((inside || at <= length) && living.passed != nullptr && living.passed->Add(cap.actor) &&
				    !inside) {
					best.entered = true;
				}
				continue;
			}
			if (atUnits < 0.0f || at < atUnits) {
				atUnits = at;
				normal = n;
				point = a + dir * at;
				body = 0;
				actor = cap.actor;
			}
		}
		if (!(atUnits >= 0.0f)) {
			continue;
		}
		const float fraction = atUnits < length ? atUnits / length : 1.0f;
		if (best.hit && fraction >= best.fraction) {
			continue;
		}
		NiPoint3 n = normal;
		const float nLength = math::Sqrt(n.LengthSquared());
		n = nLength > 1e-6f ? n * (1.0f / nLength) : dir * -1.0f;
		if (DotOf3(n, dir) > 0.0f) {
			n = n * -1.0f;
		}
		best.hit = true;
		best.fraction = fraction;
		const float back = (atUnits - kBladeContactMarginUnits) / length;
		best.safeFraction = back > 0.0f ? (back < 1.0f ? back : 1.0f) : 0.0f;
		best.along = s;
		best.point = point;
		best.normal = n;
		best.body = body;
		best.actor = actor;
		const float into = -DotOf3(way, n);
		best.speedIn = dtSeconds > 0.0f && into > 0.0f ? into / dtSeconds : 0.0f;
	}
	return best;
}

// The living the blade at this pose lies in: passed (someone walked into a
// blade held still, or it was taken up in them).
inline void PassThoseItIsIn(const BladePose& pose, const BodySpan& span, const BladeLiving& living) {
	if (living.bodies == nullptr || living.passed == nullptr) {
		return;
	}
	const NiPoint3 guard = BladePointAt(pose, span, 0.0f);
	const NiPoint3 tip = BladePointAt(pose, span, 1.0f);
	for (UInt32 c = 0; c < living.bodies->count; ++c) {
		const BladeBodyCapsule& cap = living.bodies->cap[c];
		if (SegmentSegmentDistance(guard, tip, cap.a, cap.b) <= cap.radius) {
			living.passed->Add(cap.actor);
		}
	}
}

// A blade at this pose lies across something fixed, along its length.
template <class World>
bool BladeInside(World& world, const BladePose& pose, const BodySpan& span) {
	return FirstStopper(world, BladePointAt(pose, span, 0.0f), BladePointAt(pose, span, 1.0f)).hit;
}

// Something fixed between the eyes and the blade's guard: a blade wholly
// through a wall, which the ray along it cannot see from inside.
template <class World>
bool BladeBehind(World& world, const NiPoint3& eye, const BladePose& pose, const BodySpan& span) {
	return FirstStopper(world, eye, BladePointAt(pose, span, 0.0f)).hit;
}

// Where the rest of the move goes once the blade is held: the grip's way
// without its part into the surface, and the hand's own turn.
inline BladePose SlideTarget(const BladePose& at, const BladePose& wanted, const NiPoint3& normal) {
	NiPoint3 rest = wanted.pos - at.pos;
	const float into = DotOf3(rest, normal);
	if (into < 0.0f) {
		rest = rest - normal * into;
	}
	BladePose p;
	p.rot = wanted.rot;
	p.pos = at.pos + rest;
	return p;
}

// ------------------------------------------------------------------ the step

struct BladeContactSettings {
	bool enabled = true;          // [Hands] WeaponStopsAtWalls
	float letGoUnits = 21.0f;     // WeaponLetGoMetres, in game units (0.30 m)
	float letGoRadians = 0.7853982f;  // WeaponLetGoDegrees (45)
};

// The settings as the step uses them, from the INI's: a cap that is not a
// number is the default, and one out of reason is held to 0.05..2 m and
// 5..180 degrees; game units by the tracker's own scale (70 a metre when it
// has none).
inline BladeContactSettings BladeContactSettingsFor(bool enabled, float letGoMetres, float letGoDegrees,
                                                    float unitsPerMetre) {
	BladeContactSettings s;
	s.enabled = enabled;
	float metres = letGoMetres == letGoMetres ? letGoMetres : 0.30f;
	metres = metres < 0.05f ? 0.05f : (metres > 2.0f ? 2.0f : metres);
	float degrees = letGoDegrees == letGoDegrees ? letGoDegrees : 45.0f;
	degrees = degrees < 5.0f ? 5.0f : (degrees > 180.0f ? 180.0f : degrees);
	const float perMetre = unitsPerMetre > 0.0f ? unitsPerMetre : 70.0f;
	s.letGoUnits = metres * perMetre;
	s.letGoRadians = degrees * 0.017453293f;
	return s;
}

enum class BladeContactEvent : UInt8 {
	None,
	Touched,        // held this frame, free the frame before
	LetGo,          // pulled too far: it passes through
	Rearmed,        // through, and free again
	StartedInside,  // taken up inside something (the first frame, a jump of the camera)
};

struct BladeContactState {
	bool have = false;
	BladePose pose;         // where the blade was drawn last
	bool through = false;   // passing through: no stop, no strike
	bool touching = false;  // held last frame
	BladePassLedger passed; // the living it has gone into
};

struct BladeContactFrame {
	bool active = false;  // a melee weapon drawn, in the world, the hand tracked
	BladePose wanted;     // where the weight spring draws it
	BodySpan span;        // the blade in the grip's frame
	// The camera jumped (a teleport, a snap turn, a load): nothing is swept
	// across the jump.
	bool jumped = false;
	NiPoint3 eye{0.0f, 0.0f, 0.0f};
	float dtSeconds = 0.0f;
	// The living near the blade (none: they are not met), and whether the
	// blade swings - the strike's own swing - and goes into them.
	const BladeBodies* bodies = nullptr;
	bool swinging = false;
	// The hit-stop (HitStopShare): the share of its way the blade goes this
	// frame; 1 all of it.
	float followShare = 1.0f;
};

struct BladeContactVerdict {
	BladePose pose;        // where the blade is drawn
	bool held = false;     // something held it this frame
	bool apart = false;    // drawn elsewhere than wanted: held, or slowed by a hit-stop
	bool through = false;  // it went through a wall: it strikes nothing
	BladeContactEvent event = BladeContactEvent::None;
	BladeSweep contact;    // what held it, when a sweep found it (a person: contact.actor)
	bool enteredBody = false;  // a swing went into someone not passed yet
	float gapUnits = 0.0f;     // from where the spring would draw it
	float gapRadians = 0.0f;
	BladeKicks kicks;
};

template <class World>
BladeContactVerdict StepBladeContact(BladeContactState& s, const BladeContactSettings& set,
                                     const BladeContactFrame& f, World& world) {
	BladeContactVerdict v;
	v.pose = f.wanted;
	if (!set.enabled || !f.active || !f.span.valid) {
		s = BladeContactState{};
		return v;
	}
	const BladeLiving living{f.bodies, &s.passed, f.swinging};
	if (!s.have || f.jumped) {
		const bool inside = BladeInside(world, f.wanted, f.span) || BladeBehind(world, f.eye, f.wanted, f.span);
		s.have = true;
		s.pose = f.wanted;
		s.through = inside;
		s.touching = false;
		PassThoseItIsIn(f.wanted, f.span, living);
		v.through = inside;
		v.event = inside ? BladeContactEvent::StartedInside : BladeContactEvent::None;
		return v;
	}
	if (s.through) {
		s.pose = f.wanted;
		s.touching = false;
		PassThoseItIsIn(f.wanted, f.span, living);
		if (!BladeInside(world, f.wanted, f.span) && !BladeBehind(world, f.eye, f.wanted, f.span)) {
			s.through = false;
			v.event = BladeContactEvent::Rearmed;
		}
		v.through = s.through;
		s.passed.Step(BladePointAt(f.wanted, f.span, 0.0f), BladePointAt(f.wanted, f.span, 1.0f), f.bodies,
		              f.dtSeconds);
		return v;
	}

	const BladePose start = s.pose;
	// Someone who walked into the blade where it rests is passed, not pushed
	// against.
	PassThoseItIsIn(start, f.span, living);
	const bool slowed = f.followShare >= 0.0f && f.followShare < 1.0f;
	const BladePose target = slowed ? PoseBetween(start, f.wanted, f.followShare) : f.wanted;
	const BladeSweep first = SweepBlade(world, start, target, f.span, f.dtSeconds, &v.kicks, living);
	v.enteredBody = first.entered;
	// Where it goes, the first of these not across something along its
	// length: a post between two points, or the points' straight ways cutting
	// a corner the turning blade does not. Where it was is the last resort;
	// across something even there, it lets go.
	BladePose candidates[3];
	UInt32 count = 0;
	bool held = false;
	if (!first.hit) {
		candidates[count++] = target;
	} else {
		held = true;
		v.contact = first;
		const BladePose stop = PoseBetween(start, target, first.safeFraction);
		const BladePose slid = SlideTarget(stop, target, first.normal);
		const BladeSweep second = SweepBlade(world, stop, slid, f.span, f.dtSeconds, nullptr, living);
		v.enteredBody = v.enteredBody || second.entered;
		candidates[count++] = second.hit ? PoseBetween(stop, slid, second.safeFraction) : slid;
		candidates[count++] = stop;
	}
	candidates[count++] = start;
	bool placed = false;
	BladePose pose = start;
	for (UInt32 i = 0; i < count && !placed; ++i) {
		if (!BladeInside(world, candidates[i], f.span)) {
			pose = candidates[i];
			placed = true;
			held = held || i > 0;
		}
	}

	const NiPoint3 gap = pose.pos - f.wanted.pos;
	v.gapUnits = math::Sqrt(gap.LengthSquared());
	v.gapRadians = AngleBetweenRotations(pose.rot, f.wanted.rot);
	if (!placed || (held && (v.gapUnits > set.letGoUnits || v.gapRadians > set.letGoRadians))) {
		// Let go. Pressed on into someone, the blade goes into them and they
		// are passed; anything else it went through, and it strikes nothing
		// until it is free.
		const bool intoPerson = placed && v.contact.hit && v.contact.actor != 0;
		if (intoPerson) {
			s.passed.Add(v.contact.actor);
		}
		s.pose = f.wanted;
		s.through = !intoPerson;
		s.touching = false;
		v.pose = f.wanted;
		v.held = false;
		v.through = s.through;
		v.event = BladeContactEvent::LetGo;
		s.passed.Step(BladePointAt(f.wanted, f.span, 0.0f), BladePointAt(f.wanted, f.span, 1.0f), f.bodies,
		              f.dtSeconds);
		return v;
	}
	v.pose = pose;
	v.held = held;
	v.apart = held || slowed;
	if (held && !s.touching) {
		v.event = BladeContactEvent::Touched;
	}
	s.touching = held;
	s.pose = pose;
	s.passed.Step(BladePointAt(pose, f.span, 0.0f), BladePointAt(pose, f.span, 1.0f), f.bodies, f.dtSeconds);
	return v;
}

// The hit-stop (the tester, 2026-10-09, decision 4: "ja"): once a swing has
// gone into someone, the blade goes only part of its way for a few frames -
// a quarter for three, then a half and three quarters - and then on with
// the hand again. The view is never held. Frames counted from the one the
// swing went in; past them, all of the way.
inline constexpr UInt32 kHitStopFrames = 5;

inline float HitStopShare(UInt32 framesSince) {
	static const float kShares[kHitStopFrames] = {0.25f, 0.25f, 0.25f, 0.5f, 0.75f};
	return framesSince < kHitStopFrames ? kShares[framesSince] : 1.0f;
}

// ------------------------------------------------------------------ around it

// The camera jumped between two frames - a teleport, a snap turn, a load -
// rather than walked or turned: further than this, or turned more.
inline constexpr float kBladeJumpUnits = 40.0f;
inline constexpr float kBladeJumpRadians = 0.2617994f;  // 15 degrees

inline bool CameraJumped(const NiMatrix33& lastRot, const NiPoint3& lastPos, const NiMatrix33& rot,
                         const NiPoint3& pos) {
	const NiPoint3 moved = pos - lastPos;
	if (!(moved.LengthSquared() <= kBladeJumpUnits * kBladeJumpUnits)) {
		return true;
	}
	return !(AngleBetweenRotations(lastRot, rot) <= kBladeJumpRadians);
}

// The blade's span as the step uses it: the first one read, then a new one
// only once the drawn blade has stood apart from it for a while (the
// Havok body's own rule, StepSpanWatch) - the bones are pinned a frame
// behind, so a fast hand alone reads a moved span.
struct BladeSpanCache {
	BodySpan span;
	UInt32 movedFrames = 0;
	UInt32 weapon = 0;  // the weapon it was read for: another starts anew
};

inline const BodySpan& StepBladeSpanCache(BladeSpanCache& c, UInt32 weapon, const BodySpan& now) {
	if (weapon != c.weapon) {
		c = BladeSpanCache{};
		c.weapon = weapon;
	}
	if (!c.span.valid) {
		if (now.valid) {
			c.span = now;
		}
		return c.span;
	}
	if (StepSpanWatch(c.movedFrames, SpanMoved(c.span, now))) {
		c.span = now;
	}
	return c.span;
}

// The touch felt in the hand: a pulse by the blade's speed into the
// surface, from a light knock to the full strength at 3 m/s.
inline constexpr float kBladeTouchFullUnitsPerSecond = 210.0f;
inline constexpr float kBladeTouchMinAmplitude = 0.15f;

inline float BladeTouchAmplitude(float speedInUnitsPerSecond) {
	if (!(speedInUnitsPerSecond > 0.0f)) {
		return kBladeTouchMinAmplitude;
	}
	const float a = speedInUnitsPerSecond / kBladeTouchFullUnitsPerSecond;
	return a < kBladeTouchMinAmplitude ? kBladeTouchMinAmplitude : (a > 1.0f ? 1.0f : a);
}

// A knock loud enough for a sound: 1.5 m/s into the surface, and not again
// within a quarter of a second.
inline constexpr float kBladeSoundUnitsPerSecond = 105.0f;
inline constexpr float kBladeSoundGapSeconds = 0.25f;

inline bool BladeKnockSounds(float speedInUnitsPerSecond, float secondsSinceLast) {
	return speedInUnitsPerSecond >= kBladeSoundUnitsPerSecond && secondsSinceLast >= kBladeSoundGapSeconds;
}

}  // namespace obvr::game
