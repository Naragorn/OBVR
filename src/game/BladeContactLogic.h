#pragma once

#include <memory>
#include <vector>

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BodyPart.h"
#include "game/BladeContactFeel.h"
#include "game/BladeSlideRefusal.h"
#include "game/BladeStopperIds.h"
#include "game/BodyShapes.h"
#include "game/GrabPhysics.h"
#include "game/HandBodyLogic.h"
#include "game/NiMath.h"
#include "game/RadialField.h"
#include "game/SegmentGeometry.h"

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
// BIPED: the bones' bodies of a person (the dead's ragdolls among them).
inline constexpr UInt32 kContactLayerBiped = 8;

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
	UInt32 layer = 0;  // the body's Havok layer, when it is a body
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

// A blade's section across its line, in the grip's frame as its span is
// ([Hands] FittedWeaponShapes, game/WeaponShapeLogic.h): its edge's
// direction, its flat's normal and its half extents along each, read from
// the weapon's model. Where none was read, the contact margin stands in.
struct BladeSection {
	bool valid = false;
	NiPoint3 major{1.0f, 0.0f, 0.0f};
	NiPoint3 minor{0.0f, 0.0f, 1.0f};
	float halfWidth = 0.0f;
	float halfThickness = 0.0f;
	bool round = false;  // a ball's or a capsule's: halfWidth every way (a mace's head)
};

// Kept between the steel and a person it rests on, past the section's
// reach: the rounding a held pose has.
inline constexpr float kBladeSectionSpareUnits = 0.1f;

// How far the section reaches toward `n` (a unit direction, the world's)
// with the blade turned by `rot`: a flat box's reach - its half thickness
// flat on, its half width edge on - or a round one's, a disc across the
// axis: its radius across, nothing along the axis. A section is a slice
// across the blade, so neither reaches past the points swept along it (a
// mace's head was a ball until 2026-10-28 and held its narrow tip 6.6 units
// short of a wall met tip first).
inline float SectionReach(const BladeSection& s, const NiMatrix33& rot, const NiPoint3& n) {
	const float w = DotOf3(n, rot * s.major);
	const float t = DotOf3(n, rot * s.minor);
	if (s.round) {
		return s.halfWidth * math::Sqrt(w * w + t * t);
	}
	return (w < 0.0f ? -w : w) * s.halfWidth + (t < 0.0f ? -t : t) * s.halfThickness;
}

// How far short of a person the shape is held toward `n`: its steel's
// reach and the spare when its section was read (Nadi, 2026-10-27: the blade
// resting 1.5 units off everyone, the margin meant for walls), else the
// margin. Walls and stoppers keep the margin (a Havok ray begun on a face
// does not see it; the bones are pinned a frame behind).
inline float PersonMargin(const BladeSection* s, const NiMatrix33& rot, const NiPoint3& n, float margin) {
	return s != nullptr && s->valid ? SectionReach(*s, rot, n) + kBladeSectionSpareUnits : margin;
}

// The furthest the section reaches any way, with the spare: how far a ray
// must look past its end so a person within that reach is seen.
inline float SectionMostReach(const BladeSection* s, float margin) {
	if (s == nullptr || !s->valid) {
		return margin;
	}
	const float most = (s->round ? s->halfWidth : s->halfWidth + s->halfThickness) + kBladeSectionSpareUnits;
	return most > margin ? most : margin;
}

inline NiPoint3 Normalised(const NiPoint3& v, const NiPoint3& fallback) {
	const float l = math::Sqrt(v.LengthSquared());
	return l > 1e-6f ? v * (1.0f / l) : fallback;
}

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
// someone walked into. The capsules, their lists and the shallow band:
// game/BodyShapes.h.

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
	auto link = [&](UInt32 from, UInt32 to, float radius, BodyPart part) {
		if (have[from] && have[to] && out.Add(bones[from], bones[to], radius * k, actor, part)) {
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
		if (out.Add(bones[kBoneHead], bones[kBoneHead] + up * (kBladeHeadAbove * k), kBladeHeadRadius * k, actor,
		            BodyPart::Head)) {
			++added;
		}
	}
	link(kBoneNeck, kBoneHead, kBladeNeckRadius, BodyPart::Neck);
	link(kBoneSpine2, kBoneNeck, kBladeTorsoRadius, BodyPart::Torso);
	link(kBoneSpine, kBoneSpine2, kBladeTorsoRadius, BodyPart::Torso);
	link(kBonePelvis, kBoneSpine, kBladeTorsoRadius, BodyPart::Torso);
	link(kBoneLUpperArm, kBoneLForearm, kBladeUpperArmRadius, BodyPart::LeftArm);
	link(kBoneLForearm, kBoneLHand, kBladeForearmRadius, BodyPart::LeftArm);
	link(kBoneRUpperArm, kBoneRForearm, kBladeUpperArmRadius, BodyPart::RightArm);
	link(kBoneRForearm, kBoneRHand, kBladeForearmRadius, BodyPart::RightArm);
	link(kBoneLThigh, kBoneLCalf, kBladeThighRadius, BodyPart::LeftLeg);
	link(kBoneLCalf, kBoneLFoot, kBladeCalfRadius, BodyPart::LeftLeg);
	link(kBoneRThigh, kBoneRCalf, kBladeThighRadius, BodyPart::RightLeg);
	link(kBoneRCalf, kBoneRFoot, kBladeCalfRadius, BodyPart::RightLeg);
	return added;
}

// Whether a blade or a fist touches an actor: within `marginUnits` of one of
// their capsules in `bodies` - the strike by motion's hit by touch ([Hands]
// MotionHitsByTouch; the tester, 2026-10-15: "manchmal auch direkt durch den
// körper", with the engine's bound reading hits a metre off a big fighter).
// Answers 1 touched, 0 missed, -1 when the actor has no capsule in the list
// (not collected: too far off, or the list is another step's).
inline int CapsulesTouched(const BladeBodies& bodies, UInt32 actor, const NiPoint3& a, const NiPoint3& b,
                           float marginUnits) {
	int found = -1;
	const float margin = marginUnits > 0.0f ? marginUnits : 0.0f;
	for (UInt32 c = 0; c < bodies.count; ++c) {
		const BladeBodyCapsule& cap = bodies.cap[c];
		if (cap.actor != actor) {
			continue;
		}
		found = 0;
		if (BodySegmentGap(cap, a, b, margin) <= margin) {
			return 1;
		}
	}
	return found;
}

// Which of an actor's capsules the blade a-b touched most deeply - the one
// whose axis it passes nearest for the capsule's width (distance over
// radius), so a thin neck under the torso's top wins when the blade is in
// it. False when it touched none of them.
inline bool NearestTouchedPart(const BladeBodies& bodies, UInt32 actor, const NiPoint3& a, const NiPoint3& b,
                               float marginUnits, BodyPart& part) {
	const float margin = marginUnits > 0.0f ? marginUnits : 0.0f;
	float best = 0.0f;
	bool found = false;
	for (UInt32 c = 0; c < bodies.count; ++c) {
		const BladeBodyCapsule& cap = bodies.cap[c];
		if (cap.actor != actor || !(cap.radius > 0.0f)) {
			continue;
		}
		const float d = BodySegmentGap(cap, a, b, margin) + cap.radius;
		if (d > cap.radius + margin) {
			continue;
		}
		const float depth = d / cap.radius;
		if (!found || depth < best) {
			found = true;
			best = depth;
			part = cap.part;
		}
	}
	return found;
}

// ------------------------------------------------------------- the stoppers
//
// Shapes of OBVR's own that hold the blade or a hand whatever its speed
// (the tester, 2026-10-11: "schild kollidieren nicht mit schwert. hand nicht
// mit waffe"): the worn shield as a disc, the drawn blade as a capsule for
// the free hand, the foes' blades as capsules and their shields as discs.
// Pressed past the cap they are passed as a person is - through the ledger,
// by their id - not "through" a wall: a blade crossed by the shield arm still
// strikes. The ids: the own things fixed numbers no actor's address is (the
// addresses are 4-aligned), a foe's blade or shield its address with a low
// bit set, so the ledger tells them from the foe's body.
// (The ids and ActorMayBeABody: game/BladeStopperIds.h.)

// A solid disc: the points within `radius` of the axis through `centre`
// along the unit `normal`, and within `halfThick` of its plane.
struct BladeDisc {
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 1.0f};
	float radius = 0.0f;
	float halfThick = 0.0f;
	UInt32 id = 0;
};

inline constexpr UInt32 kBladeDiscsMax = 8;

struct BladeDiscs {
	UInt32 count = 0;
	BladeDisc disc[kBladeDiscsMax];

	bool Add(const NiPoint3& centre, const NiPoint3& normal, float radius, float halfThick, UInt32 id) {
		const float n2 = normal.LengthSquared();
		if (count >= kBladeDiscsMax || !(radius > 0.0f) || !(halfThick > 0.0f) || !(n2 > 0.81f && n2 < 1.21f)) {
			return false;
		}
		disc[count].centre = centre;
		disc[count].normal = normal * (1.0f / math::Sqrt(n2));
		disc[count].radius = radius;
		disc[count].halfThick = halfThick;
		disc[count].id = id;
		++count;
		return true;
	}
};

// The distance from p to the solid disc (0 inside it).
inline float PointDiscDistance(const NiPoint3& p, const BladeDisc& d) {
	const NiPoint3 off = p - d.centre;
	const float along = DotOf3(off, d.normal);
	const NiPoint3 radial = off - d.normal * along;
	const float r = math::Sqrt(radial.LengthSquared());
	const float cr = r > d.radius ? d.radius : r;
	const float ca = along > d.halfThick ? d.halfThick : (along < -d.halfThick ? -d.halfThick : along);
	const NiPoint3 q = d.centre + d.normal * ca + (r > 1e-6f ? radial * (cr / r) : NiPoint3{0.0f, 0.0f, 0.0f});
	return math::Sqrt((p - q).LengthSquared());
}

// The distance from the segment a-b to the disc: the nearest of nine points
// along it (a crossing of a shield 12 units thick cannot slip between them).
inline constexpr UInt32 kDiscSamples = 9;

inline float SegmentDiscDistance(const NiPoint3& a, const NiPoint3& b, const BladeDisc& d) {
	float best = -1.0f;
	for (UInt32 i = 0; i < kDiscSamples; ++i) {
		const float s = static_cast<float>(i) / static_cast<float>(kDiscSamples - 1);
		const float dist = PointDiscDistance(a + (b - a) * s, d);
		best = best < 0.0f || dist < best ? dist : best;
	}
	return best;
}

inline bool SegmentTouchesDisc(const NiPoint3& a, const NiPoint3& b, const BladeDisc& d) {
	return SegmentDiscDistance(a, b, d) <= 0.01f;
}

// Where the segment from -> to first enters the disc: the slab of its
// thickness met with the cylinder of its radius. `inside` when `from` is in
// it already (fraction 0). The normal: the face entered, or the rim's
// outward direction there.
inline bool SegmentEntersDisc(const NiPoint3& from, const NiPoint3& to, const BladeDisc& d, float& fraction,
                              NiPoint3& normal, bool& inside) {
	inside = false;
	if (!(d.radius > 0.0f) || !(d.halfThick > 0.0f)) {
		return false;
	}
	const NiPoint3 o = from - d.centre;
	const NiPoint3 dir = to - from;
	const float oa = DotOf3(o, d.normal);
	const float da = DotOf3(dir, d.normal);
	const NiPoint3 oRadial = o - d.normal * oa;
	const NiPoint3 dRadial = dir - d.normal * da;
	const float r2 = oRadial.LengthSquared();
	if (oa <= d.halfThick && oa >= -d.halfThick && r2 <= d.radius * d.radius) {
		inside = true;
		fraction = 0.0f;
		normal = d.normal;
		return true;
	}
	if (!(dir.LengthSquared() > 1e-12f)) {
		return false;
	}
	const float big = 1e30f;
	float slabIn = -big;
	float slabOut = big;
	if (da > 1e-9f || da < -1e-9f) {
		const float t1 = (-d.halfThick - oa) / da;
		const float t2 = (d.halfThick - oa) / da;
		slabIn = t1 < t2 ? t1 : t2;
		slabOut = t1 < t2 ? t2 : t1;
	} else if (oa > d.halfThick || oa < -d.halfThick) {
		return false;
	}
	float cylIn = -big;
	float cylOut = big;
	const float qa = dRadial.LengthSquared();
	const float qb = DotOf3(oRadial, dRadial);
	const float qc = r2 - d.radius * d.radius;
	if (qa > 1e-12f) {
		const float h = qb * qb - qa * qc;
		if (h < 0.0f) {
			return false;
		}
		const float s = math::Sqrt(h);
		cylIn = (-qb - s) / qa;
		cylOut = (-qb + s) / qa;
	} else if (qc > 0.0f) {
		return false;
	}
	const float enter = slabIn > cylIn ? slabIn : cylIn;
	const float leave = slabOut < cylOut ? slabOut : cylOut;
	if (enter > leave || enter < 0.0f || enter > 1.0f) {
		return false;
	}
	fraction = enter;
	if (slabIn >= cylIn) {
		normal = d.normal * (oa > 0.0f ? 1.0f : -1.0f);
	} else {
		const NiPoint3 p = o + dir * enter;
		const NiPoint3 radial = p - d.normal * DotOf3(p, d.normal);
		const float rl = math::Sqrt(radial.LengthSquared());
		normal = rl > 1e-6f ? radial * (1.0f / rl) : d.normal;
	}
	return true;
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
		StepSegments(&guard, &tip, 1, bodies, dtSeconds);
	}

	// The same for a shape of several segments (a shield's two spokes):
	// touching any of them is touching. The stoppers (capsules by their id)
	// and the discs count as the bodies do.
	void StepSegments(const NiPoint3* from, const NiPoint3* to, UInt32 segments, const BladeBodies* bodies,
	                  float dtSeconds, const BladeBodies* stoppers = nullptr, const BladeDiscs* discs = nullptr) {
		UInt32 kept = 0;
		for (UInt32 i = 0; i < count; ++i) {
			bool seen = false;
			bool touching = false;
			const BladeBodies* lists[2] = {bodies, stoppers};
			for (const BladeBodies* list : lists) {
				for (UInt32 c = 0; list != nullptr && c < list->count; ++c) {
					const BladeBodyCapsule& cap = list->cap[c];
					if (cap.actor != actor[i]) {
						continue;
					}
					seen = true;
					for (UInt32 k = 0; k < segments && !touching; ++k) {
						touching = BodySegmentGap(cap, from[k], to[k], 0.0f) <= 0.0f;
					}
				}
			}
			for (UInt32 c = 0; discs != nullptr && c < discs->count; ++c) {
				const BladeDisc& d = discs->disc[c];
				if (d.id != actor[i]) {
					continue;
				}
				seen = true;
				for (UInt32 k = 0; k < segments && !touching; ++k) {
					touching = SegmentTouchesDisc(from[k], to[k], d);
				}
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
	bool second = false;   // the shape's second span met it (a head), not its line
	// Only just inside a person when the sweep began (ShallowBand): how deep,
	// and the way out of their capsule - the step pushes it out (0 for none).
	float depthUnits = 0.0f;
	NiPoint3 depthNormal{0.0f, 0.0f, 0.0f};
};

// What the living are to this sweep: their capsules, those passed, and
// whether the blade swings (then it goes into them and passes them - unless
// they hold). And the stoppers: capsules and discs that hold the blade at
// any speed, passed only once it was pressed past the cap into them (the
// ledger, by their id).
struct BladeLiving {
	const BladeBodies* bodies = nullptr;
	BladePassLedger* passed = nullptr;
	bool swinging = false;
	const BladeBodies* stoppers = nullptr;
	const BladeDiscs* discs = nullptr;
	// The living hold the shape at any speed, as the stoppers do: a swing is
	// held at them, and only pressed on past the cap does it go in (the
	// tester, 2026-10-15: "mach es wie in blade and sorcery" - there the
	// body's colliders stop the blade and the fist; docs/weapon-collision-spec
	// section 17). Off, a swing goes into them and passes them.
	bool hold = false;
};

// One swept point's ray against the world: the first stopper on it, and the
// movable things it passes within its way kicked with its speed.
template <class World>
BladeRayHit WorldRayOfPoint(World& world, const NiPoint3& a, const NiPoint3& end, const NiPoint3& way, float reach,
                            float length, float dtSeconds, BladeKicks* kicks) {
	const NiPoint3 velocity = dtSeconds > 0.0f ? way * (1.0f / dtSeconds) : NiPoint3{0.0f, 0.0f, 0.0f};
	return FirstStopper(world, a, end, [&](const BladeRayHit& m) {
		if (kicks != nullptr && dtSeconds > 0.0f && m.fraction * reach <= length) {
			kicks->Add(m.body, velocity);
		}
	});
}

// The pose a sweep held, pushed back out a step (kBladeShallowPushUnits)
// when it lay only just inside someone (BladeSweep::depthUnits).
inline BladePose PushedOutOfShallow(BladePose pose, const BladeSweep& hit) {
	if (hit.depthUnits > 0.0f) {
		const float push = hit.depthUnits < kBladeShallowPushUnits ? hit.depthUnits : kBladeShallowPushUnits;
		pose.pos = pose.pos + hit.depthNormal * push;
	}
	return pose;
}

// A point's hit, for the sweep: its normal `n` made a unit turned against
// the way, and how far short of it the shape is held - at a person's grown
// capsule (hitGrown) right there; anything else the margin short along the
// way, or a head's reach toward it when larger (sectionAtWalls: a mace's
// head 7 round is no line).
inline float HeldShortOf(NiPoint3& n, const NiPoint3& dir, bool hitGrown, float margin, bool sectionAtWalls,
                         const BladeSection* section, const NiMatrix33& rot, bool outOfInside = false) {
	const float nLength = math::Sqrt(n.LengthSquared());
	n = nLength > 1e-6f ? n * (1.0f / nLength) : dir * -1.0f;
	// The way out of a capsule the point is already in is the surface's own
	// normal: turned against a point drawn back out, it pointed into them.
	if (!outOfInside && DotOf3(n, dir) > 0.0f) {
		n = n * -1.0f;
	}
	float held = hitGrown ? 0.0f : margin;
	if (!hitGrown && sectionAtWalls && section != nullptr && section->valid) {
		const float reachToward = SectionReach(*section, rot, n) + kBladeSectionSpareUnits;
		held = reachToward > held ? reachToward : held;
	}
	return held;
}

// How far a person's capsule is grown for the point `a` (SweepBlade): the
// shape's section's reach toward them, from the point toward their axis
// (PersonMargin); 0 with no section read.
inline float PersonGrowAt(const BladeBodyCapsule& cap, const NiPoint3& a, const NiPoint3& dir,
                          const BladeSection* section, const NiMatrix33& rot, float margin) {
	if (section == nullptr || !section->valid) {
		return 0.0f;
	}
	const NiPoint3 onAxis = cap.a + (cap.b - cap.a) * ShareOnSegment(a, cap.a, cap.b);
	const NiPoint3 toward = a - onAxis;
	const float towardUnits = math::Sqrt(toward.LengthSquared());
	return PersonMargin(section, rot, towardUnits > 1e-6f ? toward * (1.0f / towardUnits) : dir * -1.0f, margin);
}

// The blade's points from where they were to where they would be, the
// earliest that meets something fixed - or, held slowly, someone living.
// Each point's ray reaches the margin past its end, so a point is never
// left nearer a surface than the margin - a ray that began on a face could
// not see it. Movable things passed are kicked with the speed of the point
// that passed them (none without a time); the living a swing goes into
// (unless they hold), and any a point is already inside, are passed. The
// margin is the blade's own
// half-width unless the caller's shape has another (a hand's, a shield's);
// a span of no length (a ball's centre) is one point, swept once.
template <class World>
BladeSweep SweepBlade(World& world, const BladePose& from, const BladePose& to, const BodySpan& span, float dtSeconds,
                      BladeKicks* kicks, const BladeLiving& living = BladeLiving{},
                      float margin = kBladeContactMarginUnits, const BladeSection* section = nullptr,
                      bool sectionAtWalls = false) {
	BladeSweep best;
	const bool onePoint = (span.b - span.a).LengthSquared() < 1e-6f;
	for (UInt32 i = 0; i < (onePoint ? 1u : kBladeSweepPoints); ++i) {
		const float s = kBladeSweepAt[i];
		const NiPoint3 a = BladePointAt(from, span, s);
		const NiPoint3 b = BladePointAt(to, span, s);
		const NiPoint3 way = b - a;
		const float length = math::Sqrt(way.LengthSquared());
		// Slower than kBladeMinSweepUnits a frame the world is not swept (its
		// floor against jitter); people still are - a point creeping onto
		// them at the harness's 0.16 units a frame was never checked and sank
		// in (armour-touch-blade-mod.txt, 2026-11-07, "0.00 m/s into it").
		if (!(length > 1e-4f)) {
			continue;
		}
		const bool sweepWorld = length >= kBladeMinSweepUnits;
		const NiPoint3 dir = way * (1.0f / length);
		const float reach = length + SectionMostReach(section, margin);
		const NiPoint3 end = a + dir * reach;
		const BladeRayHit h = sweepWorld ? WorldRayOfPoint(world, a, end, way, reach, length, dtSeconds, kicks) : BladeRayHit{};
		float atUnits = h.hit ? h.fraction * reach : -1.0f;
		NiPoint3 normal = h.normal;
		NiPoint3 point = h.point;
		UInt32 body = h.body;
		UInt32 actor = 0;
		// A shape of OBVR's own met on this point's way: a person (a swing
		// goes into them and passes them) or a stopper (held at any speed);
		// one the point is inside already is passed either way. The
		// stoppers and the discs go first, so that a person behind one the
		// point is held at (their blade, your shield) is not reached - the
		// swing stops short of them.
		// Whether the hit taken was met at a person's capsule grown by the
		// shape's reach toward them: then it stops there, not the margin
		// short of it.
		bool hitGrown = false;
		// a point only just inside a person: how deep, and the way out
		float atDepth = 0.0f;
		NiPoint3 atDepthNormal{0.0f, 0.0f, 0.0f};
		// the hit taken is the way out of a capsule the point is already in
		// (the band): its normal stands as it is, whichever way the point goes
		bool atOut = false;
		auto take = [&](float f, const NiPoint3& n, bool inside, UInt32 id, bool passable, bool grown = false,
		                bool out = false) {
			const float at = f * reach;
			if (inside || (passable && living.swinging)) {
				const bool reached = inside || (at <= length && (atUnits < 0.0f || at <= atUnits));
				if (reached && living.passed != nullptr && living.passed->Add(id) && !inside && passable) {
					best.entered = true;
				}
				return;
			}
			if (atUnits < 0.0f || at < atUnits) {
				atUnits = at;
				normal = n;
				point = a + dir * at;
				body = 0;
				actor = id;
				hitGrown = grown;
				atOut = out;
			}
		};
		// A point already in a capsule, but only just (ShallowBand): held
		// where it is, the normal out of the capsule - not passed.
		// A person's capsule, with the shape's section read, is met grown by
		// the shape's reach toward them (PersonMargin, from the point toward
		// their axis): the reach is kept off their surface whichever way the
		// shape comes - backed off along its way instead, a hand sliding
		// round a torso came within 0.2 units of a capsule its palm reached
		// 3.4 into (mesh-gap-hand-soldier.txt, 2026-10-27), as a way
		// that never enters the bare capsule is never stopped.
		auto takeCapsule = [&](const BladeBodyCapsule& cap, bool passable, bool person) {
			const float grow = person ? PersonGrowAt(cap, a, dir, section, to.rot, margin) : 0.0f;
			float f = 1.0f;
			NiPoint3 n;
			bool inside = false;
			if (!BodyEnters(cap, a, end, grow, f, n, inside)) {
				return;
			}
			if (inside) {
				// within the reach of their surface, or only just in them
				// (ShallowBand): held where it is - not passed
				NiPoint3 out;
				const float depth = -BodyGapAt(cap, a, &out);
				if (depth <= BodyBandAt(cap, a)) {
					take(0.0f, out, false, cap.actor, passable, grow > 0.0f, true);
					if (atUnits == 0.0f && depth > atDepth) {
						atDepth = depth;
						atDepthNormal = out;
					}
					return;
				}
			}
			take(f, n, inside, cap.actor, passable, grow > 0.0f);
		};
		for (UInt32 c = 0; living.stoppers != nullptr && c < living.stoppers->count; ++c) {
			const BladeBodyCapsule& cap = living.stoppers->cap[c];
			if (living.passed != nullptr && living.passed->Has(cap.actor)) {
				continue;
			}
			takeCapsule(cap, false, false);
		}
		for (UInt32 c = 0; living.discs != nullptr && c < living.discs->count; ++c) {
			const BladeDisc& d = living.discs->disc[c];
			if (living.passed != nullptr && living.passed->Has(d.id)) {
				continue;
			}
			float f = 1.0f;
			NiPoint3 n;
			bool inside = false;
			if (SegmentEntersDisc(a, end, d, f, n, inside)) {
				take(f, n, inside, d.id, false);
			}
		}
		for (UInt32 c = 0; living.bodies != nullptr && c < living.bodies->count; ++c) {
			const BladeBodyCapsule& cap = living.bodies->cap[c];
			if (living.passed != nullptr && living.passed->Has(cap.actor)) {
				continue;
			}
			takeCapsule(cap, !living.hold, true);
		}
		if (!(atUnits >= 0.0f)) {
			continue;
		}
		NiPoint3 n = normal;
		const float held = HeldShortOf(n, dir, hitGrown, margin, sectionAtWalls, section, to.rot, atOut);
		// The ray looks the section's most reach past the way; what lies
		// further than the reach toward it is not met (a warhammer's flat head
		// pointed down "met" the floor 13 units over it, 2026-10-28).
		if (atUnits - held >= length) {
			continue;
		}
		const float fraction = atUnits < length ? atUnits / length : 1.0f;
		if (best.hit && fraction >= best.fraction) {
			continue;
		}
		best.hit = true;
		best.fraction = fraction;
		const float back = (atUnits - held) / length;
		best.safeFraction = back > 0.0f ? (back < 1.0f ? back : 1.0f) : 0.0f;
		best.along = s;
		best.point = point;
		best.normal = n;
		best.body = body;
		best.actor = actor;
		best.depthUnits = atUnits == 0.0f ? atDepth : 0.0f;
		best.depthNormal = atDepthNormal;
		const float into = -DotOf3(way, n);
		best.speedIn = dtSeconds > 0.0f && into > 0.0f ? into / dtSeconds : 0.0f;
	}
	return best;
}

// Someone not passed the blade at this pose lies across - held slowly, or
// at any speed when the living hold: an arm or a leg between two of the
// swept points, which the points' ways missed. 0 for none, or while it
// swings and they do not hold.
inline UInt32 PersonAcross(const BladePose& pose, const BodySpan& span, const BladeLiving& living) {
	const NiPoint3 guard = BladePointAt(pose, span, 0.0f);
	const NiPoint3 tip = BladePointAt(pose, span, 1.0f);
	// Only just inside (ShallowBand) is not across: the pose is taken and the
	// sweep holds the shape where it is.
	if (living.bodies != nullptr && (!living.swinging || living.hold)) {
		for (UInt32 c = 0; c < living.bodies->count; ++c) {
			const BladeBodyCapsule& cap = living.bodies->cap[c];
			if (living.passed != nullptr && living.passed->Has(cap.actor)) {
				continue;
			}
			if (BodyCrossedDeep(cap, guard, tip)) {
				return cap.actor;
			}
		}
	}
	// A stopper, swinging or not.
	for (UInt32 c = 0; living.stoppers != nullptr && c < living.stoppers->count; ++c) {
		const BladeBodyCapsule& cap = living.stoppers->cap[c];
		if (living.passed != nullptr && living.passed->Has(cap.actor)) {
			continue;
		}
		if (BodyCrossedDeep(cap, guard, tip)) {
			return cap.actor;
		}
	}
	for (UInt32 c = 0; living.discs != nullptr && c < living.discs->count; ++c) {
		const BladeDisc& d = living.discs->disc[c];
		if (living.passed != nullptr && living.passed->Has(d.id)) {
			continue;
		}
		if (SegmentTouchesDisc(guard, tip, d)) {
			return d.id;
		}
	}
	return 0;
}

// The living the blade at this pose lies in: passed (someone walked into a
// blade held still, or it was taken up in them). The stoppers it lies in
// the same way (the shield arm swung across the blade).
inline void PassThoseItIsIn(const BladePose& pose, const BodySpan& span, const BladeLiving& living) {
	if (living.passed == nullptr) {
		return;
	}
	const NiPoint3 guard = BladePointAt(pose, span, 0.0f);
	const NiPoint3 tip = BladePointAt(pose, span, 1.0f);
	// Deeper than the shallow band only: a shape just inside someone is held
	// at them (SweepBlade, PersonAcross), not passed.
	const BladeBodies* lists[2] = {living.bodies, living.stoppers};
	for (const BladeBodies* list : lists) {
		for (UInt32 c = 0; list != nullptr && c < list->count; ++c) {
			const BladeBodyCapsule& cap = list->cap[c];
			if (BodyCrossedDeep(cap, guard, tip)) {
				living.passed->Add(cap.actor);
			}
		}
	}
	for (UInt32 c = 0; living.discs != nullptr && c < living.discs->count; ++c) {
		const BladeDisc& d = living.discs->disc[c];
		if (SegmentTouchesDisc(guard, tip, d)) {
			living.passed->Add(d.id);
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
	float letGoUnits = 42.0f;     // WeaponLetGoMetres, in game units (0.60 m; 0.30 until 2026-10-10)
	float letGoRadians = 0.7853982f;  // WeaponLetGoDegrees (45)
	// The cap at a person, of its own ([Hands] BodyLetGoMetres): how far the
	// hand may go past where someone holds the shape before it goes into
	// them (passed, still striking). 0 never: held for good, as Blade &
	// Sorcery's "block" holds a stuck weapon, until the hand comes back (the
	// tester, 2026-10-15). The angle cap goes with it - none while a person
	// holds and this is 0. Walls and the stoppers keep the weapon's cap.
	float bodyLetGoUnits = 0.0f;
	// The turn cap at people, the slide's ([Hands] BodyLetGoDegrees; with
	// BladeContactFrame::slidesOffPeople): a blade a person has turned further
	// off the hand's turn than this slices out of them - into them, passed,
	// drawn where the hand is - as Blade & Sorcery's slash slides out through
	// them. 0 none: it lies along them until it comes off at its tip or the
	// hand comes back.
	float bodyLetGoRadians = 0.0f;
};

// The settings as the step uses them, from the INI's: a cap that is not a
// number is the default, and one out of reason is held to 0.05..2 m and
// 5..180 degrees (the caps at people to 0..2 m and 0..180 degrees, 0 for
// none); game units by the tracker's own scale (70 a metre when it has none).
inline BladeContactSettings BladeContactSettingsFor(bool enabled, float letGoMetres, float letGoDegrees,
                                                    float unitsPerMetre, float bodyLetGoMetres = 0.0f,
                                                    float bodyLetGoDegrees = 0.0f) {
	BladeContactSettings s;
	s.enabled = enabled;
	float metres = letGoMetres == letGoMetres ? letGoMetres : 0.60f;
	metres = metres < 0.05f ? 0.05f : (metres > 2.0f ? 2.0f : metres);
	float degrees = letGoDegrees == letGoDegrees ? letGoDegrees : 45.0f;
	degrees = degrees < 5.0f ? 5.0f : (degrees > 180.0f ? 180.0f : degrees);
	float body = bodyLetGoMetres == bodyLetGoMetres ? bodyLetGoMetres : 0.0f;
	body = body < 0.0f ? 0.0f : (body > 2.0f ? 2.0f : body);
	const float perMetre = unitsPerMetre > 0.0f ? unitsPerMetre : 70.0f;
	s.letGoUnits = metres * perMetre;
	s.letGoRadians = degrees * 0.017453293f;
	s.bodyLetGoUnits = body * perMetre;
	float bodyTurn = bodyLetGoDegrees == bodyLetGoDegrees ? bodyLetGoDegrees : 0.0f;
	bodyTurn = bodyTurn < 0.0f ? 0.0f : (bodyTurn > 180.0f ? 180.0f : bodyTurn);
	s.bodyLetGoRadians = bodyTurn * 0.017453293f;
	return s;
}

enum class BladeContactEvent : UInt8 {
	None,
	Touched,        // held this frame, free the frame before
	LetGo,          // pulled too far: it passes through
	Rearmed,        // through, and free again
	StartedInside,  // taken up inside something (the first frame, a jump of the camera)
	CarriedOff,     // a person's hold with the controller outside them: free where the controller is
	SlidOff,        // a person's hold the blade turned off: free at the hand's pose (slidesOffPeople)
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
	// A second span of the same shape, when it has one: a shield's other
	// spoke (HandContactLogic.h). Both are swept, and the first contact of
	// either holds the shape.
	BodySpan span2;
	// How far short of a surface the shape is held: the blade's half-width,
	// a hand's, half a shield's thickness.
	float marginUnits = kBladeContactMarginUnits;
	// The second span's own margin, when it has another (a mace's head: its
	// radius); below 0 the first's.
	float margin2Units = -1.0f;
	// Each span's section across it, read from the weapon's model ([Hands]
	// FittedWeaponShapes): a person is held its steel's reach short, not
	// the margin. Not read, the margin.
	BladeSection section;
	BladeSection section2;
	// The camera jumped (a teleport, a snap turn, a load): nothing is swept
	// across the jump.
	bool jumped = false;
	NiPoint3 eye{0.0f, 0.0f, 0.0f};
	float dtSeconds = 0.0f;
	// The living near the blade (none: they are not met), and whether the
	// blade swings - the strike's own swing - and goes into them.
	const BladeBodies* bodies = nullptr;
	bool swinging = false;
	// The living hold the shape at any speed (BladeLiving::hold): a swing is
	// held at the person it meets rather than going into them.
	bool bodiesHold = false;
	// A person holds the shape only while the wanted shape - the controller's
	// - is inside them, within the margin of one of their capsules: the hands'
	// rule. A body stops a hand; a hand walked or turned away from them with
	// the stick, past them or round them, comes free where the controller is
	// (a Quest tester, 2026-10-22: "'Sticky' hands ... When you try to move
	// away, your arm stretches out like a piece of spaghetti"). Off, their
	// hold is for good until the sweep back to the controller clears them:
	// the blade's, Blade & Sorcery's block (BodyLetGoMetres 0).
	bool holdWhileInside = false;
	// A person's hold lets the grip go with the hand and turns the blade off
	// them, on the side it met them, until it comes off at its tip - Blade &
	// Sorcery's blade sliding while it cuts (TurnOffPeople). Off, the whole
	// pose is held where it met them, the grip too.
	bool slidesOffPeople = false;
	// The stoppers (the shields, the blades): held at any speed.
	const BladeBodies* stoppers = nullptr;
	const BladeDiscs* discs = nullptr;
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
	SlideRefusal slideRefused = SlideRefusal::None;  // a person's hold that did not slide, and why
};

// ---------------------------------------------------------------- the shape
//
// What the step sweeps is one span - the blade, a hand - or two, a shield's
// spokes (HandContactLogic.h). Each of these asks every span the frame
// carries and answers for the shape as a whole.

inline UInt32 ShapeSpans(const BladeContactFrame& f, const BodySpan* out[2]) {
	UInt32 n = 0;
	if (f.span.valid) {
		out[n++] = &f.span;
	}
	if (f.span2.valid) {
		out[n++] = &f.span2;
	}
	return n;
}

// The contact of the span that must stop soonest; a swing into someone by
// either enters.
template <class World>
BladeSweep SweepShape(World& world, const BladePose& from, const BladePose& to, const BladeContactFrame& f,
                      BladeKicks* kicks, const BladeLiving& living) {
	const BodySpan* spans[2];
	const UInt32 n = ShapeSpans(f, spans);
	BladeSweep best;
	bool entered = false;
	for (UInt32 i = 0; i < n; ++i) {
		const bool second = spans[i] == &f.span2;
		const float margin = second && f.margin2Units >= 0.0f ? f.margin2Units : f.marginUnits;
		BladeSweep s = SweepBlade(world, from, to, *spans[i], f.dtSeconds, kicks, living, margin,
		                          second ? &f.section2 : &f.section, second);
		s.second = second;
		entered = entered || s.entered;
		// the span that must stop soonest holds the shape: its own reach
		// short of what it met, not the one that met it first (a mace's
		// line and head at the floor at once - the line's 1.5 margin let
		// the head 6.5 round into it, mace-wall.txt 2026-10-27)
		if (s.hit && (!best.hit || s.safeFraction < best.safeFraction ||
		              (s.safeFraction == best.safeFraction && s.fraction < best.fraction))) {
			best = s;
		}
	}
	best.entered = entered;
	return best;
}

template <class World>
bool ShapeInside(World& world, const BladePose& pose, const BladeContactFrame& f) {
	const BodySpan* spans[2];
	const UInt32 n = ShapeSpans(f, spans);
	for (UInt32 i = 0; i < n; ++i) {
		if (BladeInside(world, pose, *spans[i])) {
			return true;
		}
	}
	return false;
}

template <class World>
bool ShapeBehind(World& world, const BladePose& pose, const BladeContactFrame& f) {
	const BodySpan* spans[2];
	const UInt32 n = ShapeSpans(f, spans);
	for (UInt32 i = 0; i < n; ++i) {
		if (BladeBehind(world, f.eye, pose, *spans[i])) {
			return true;
		}
	}
	return false;
}

inline UInt32 ShapeAcross(const BladePose& pose, const BladeContactFrame& f, const BladeLiving& living) {
	const BodySpan* spans[2];
	const UInt32 n = ShapeSpans(f, spans);
	for (UInt32 i = 0; i < n; ++i) {
		const UInt32 across = PersonAcross(pose, *spans[i], living);
		if (across != 0) {
			return across;
		}
	}
	return 0;
}

inline void ShapePassThoseItIsIn(const BladePose& pose, const BladeContactFrame& f, const BladeLiving& living) {
	const BodySpan* spans[2];
	const UInt32 n = ShapeSpans(f, spans);
	for (UInt32 i = 0; i < n; ++i) {
		PassThoseItIsIn(pose, *spans[i], living);
	}
}

// Whether the shape at `pose` is inside the person `actor`: one of its swept
// points within the margin of one of their capsules - pressed into them, or
// resting on them. Nothing of theirs there, or nobody: outside.
inline bool ShapeInsidePerson(const BladePose& pose, const BladeContactFrame& f, UInt32 actor) {
	if (f.bodies == nullptr || actor == 0) {
		return false;
	}
	const BodySpan* spans[2];
	const UInt32 n = ShapeSpans(f, spans);
	for (UInt32 c = 0; c < f.bodies->count; ++c) {
		const BladeBodyCapsule& cap = f.bodies->cap[c];
		if (cap.actor != actor) {
			continue;
		}
		for (UInt32 i = 0; i < n; ++i) {
			const bool onePoint = (spans[i]->b - spans[i]->a).LengthSquared() < 1e-6f;
			for (UInt32 k = 0; k < (onePoint ? 1u : kBladeSweepPoints); ++k) {
				NiPoint3 normal;
				const NiPoint3 p = BladePointAt(pose, *spans[i], kBladeSweepAt[k]);
				if (BodyGapAt(cap, p, &normal) <= f.marginUnits) {
					return true;
				}
			}
		}
	}
	return false;
}

inline void StepShapePassed(BladePassLedger& passed, const BladePose& pose, const BladeContactFrame& f) {
	const BodySpan* spans[2];
	const UInt32 n = ShapeSpans(f, spans);
	NiPoint3 from[2];
	NiPoint3 to[2];
	for (UInt32 i = 0; i < n; ++i) {
		from[i] = BladePointAt(pose, *spans[i], 0.0f);
		to[i] = BladePointAt(pose, *spans[i], 1.0f);
	}
	passed.StepSegments(from, to, n, f.bodies, f.dtSeconds, f.stoppers, f.discs);
}

// ------------------------------------------------------------------ off people
//
// A person holds the blade and the hand goes on (slidesOffPeople; Nadi,
// 2026-10-26: "Schwert bleibt oft zu sehr an npcs hängen fast schon kleben.
// Soll eher wie b&s sein"). Blade & Sorcery's blade is a rigid body on a
// spring to the hand: a body it meets turns it, it slides along them, and a
// slash slides out of them ("penetrationAllowSlide ... allowing the blade to
// slice out", the SDK's Damager JSON). The step held the whole pose where it
// met them instead, the grip too: the turn of a swing presses the blade into
// them, the slide takes out only the grip's way into them, so the slid pose
// lay across them, was refused, and the pose of the frame before stayed - in
// his log of 2026-10-25 the drawn tip stood still in 396 of the 813 frames a
// person held it, the controller up to 63 units on.
//
// Here the grip goes where the hand is and the blade is turned about it, off
// each capsule it lies in, then on toward the hand's own turn as far as they
// let it - the way there checked pose by pose, so it goes round them, never
// through; once that turn is clear of them it came off them at its tip. A
// push along the blade (a thrust at them) cannot turn it off, nor can the
// grip itself inside them, nor a turn that would cross them: those are held
// as before.

inline constexpr UInt32 kTurnOffPasses = 8;
inline constexpr float kTurnOffMostRadiansAPass = 0.35f;
// The blade's point in them this near the grip: the grip itself is in them.
inline constexpr float kTurnOffLeastLeverUnits = 2.0f;
// The share of the way out that is across the blade, under which the push is
// along it - a thrust.
inline constexpr float kTurnOffLeastSideways = 0.15f;
// Turned a little past the margin, so the blade is not in them by rounding;
// and the rounding a pose off them may have.
inline constexpr float kTurnOffSpareUnits = 0.05f;
inline constexpr float kTurnOffSlackUnits = 0.02f;
// The poses a way is checked at (a longsword's tip goes 5 units a step on a
// turn of 55 degrees - less than the thinnest capsule across), and the
// halvings that find where it first meets them.
inline constexpr UInt32 kTurnOffSteps = 12;
inline constexpr UInt32 kTurnOffHalvings = 6;
// The grip's way while it slides, in steps this long at most (this many a
// frame), each turned off them from the last: it follows their round side,
// where one straight way cuts into it (6.4 units a frame on Linux, 2026-11-09).
inline constexpr float kSlideStepUnits = 2.5f;
inline constexpr UInt32 kSlideMostSteps = 16;

// How deep `clearUnits` round a capsule's axis reach into the blade at
// `pose`: 0 when the blade is clear of all of the living not passed.
inline float DeepestInPeople(const BladePose& pose, const BodySpan& span, const BladeLiving& living, float margin,
                             bool shallowFree, NiPoint3* at = nullptr, NiPoint3* out = nullptr,
                             const BladeSection* section = nullptr) {
	if (living.bodies == nullptr) {
		return 0.0f;
	}
	const NiPoint3 guard = BladePointAt(pose, span, 0.0f);
	const NiPoint3 tip = BladePointAt(pose, span, 1.0f);
	float worst = 0.0f;
	for (UInt32 c = 0; c < living.bodies->count; ++c) {
		const BladeBodyCapsule& cap = living.bodies->cap[c];
		if (living.passed != nullptr && living.passed->Has(cap.actor)) {
			continue;
		}
		NiPoint3 onBlade;
		NiPoint3 off;  // out of them there, zero on a round capsule's axis
		// exact only as far as it can matter: the band, or the steel's most reach
		const float asked = shallowFree ? 0.0f : SectionMostReach(section, margin);
		const float gap = BodySegmentGap(cap, guard, tip, asked, &onBlade, &off);
		// Clear of them by the margin (the steel's reach toward them when
		// its section was read); or, for a way, not deeper than the shallow
		// band (a turn grazing them, as PersonAcross takes it).
		const NiPoint3 toward = off.LengthSquared() > 0.0f ? off : NiPoint3{0.0f, 0.0f, 1.0f};
		const float clear = shallowFree ? -BodyBandAt(cap, onBlade) : PersonMargin(section, pose.rot, toward, margin);
		const float depth = clear - gap;
		if (depth > worst) {
			worst = depth;
			if (at != nullptr) {
				*at = onBlade;
			}
			if (out != nullptr) {
				*out = off;
			}
		}
	}
	return worst;
}

// The parts of a shape the turn off people keeps clear: a blade's line and,
// when it has one, its head (a mace's, an axe's - the second span with its
// own margin and section), each kept clear of them by its own reach (the
// iron mace's head went 7.6-9.9 units into the soldier's arm while only the
// line was turned off him, 2026-10-27).
struct ShapeParts {
	UInt32 count = 0;
	const BodySpan* span[2] = {};
	float margin[2] = {};
	const BladeSection* section[2] = {};

	void Add(const BodySpan& s, float m, const BladeSection* sec) {
		if (count < 2 && s.valid) {
			span[count] = &s;
			margin[count] = m;
			section[count] = sec;
			++count;
		}
	}
};

// The parts of one span (its margin, its section).
inline ShapeParts OneSpan(const BodySpan& span, float margin, const BladeSection* section = nullptr) {
	ShapeParts p;
	p.Add(span, margin, section);
	return p;
}

// The deepest any part reaches into the living at `pose` (DeepestInPeople),
// where, which way out, and along which part's line.
inline float DeepestInShape(const BladePose& pose, const ShapeParts& parts, const BladeLiving& living, bool shallowFree,
                            NiPoint3* at = nullptr, NiPoint3* out = nullptr, const BodySpan** which = nullptr) {
	float worst = 0.0f;
	for (UInt32 i = 0; i < parts.count; ++i) {
		NiPoint3 a{0.0f, 0.0f, 0.0f};
		NiPoint3 o{0.0f, 0.0f, 0.0f};
		const float d =
			DeepestInPeople(pose, *parts.span[i], living, parts.margin[i], shallowFree, &a, &o, parts.section[i]);
		if (d > worst) {
			worst = d;
			if (at != nullptr) {
				*at = a;
			}
			if (out != nullptr) {
				*out = o;
			}
			if (which != nullptr) {
				*which = parts.span[i];
			}
		}
	}
	return worst;
}

// The blade at `pose` - its grip where the hand is - turned about the grip
// out of the living's capsules, each part its margin or its reach short of
// them. False when it cannot be turned out of them.
inline bool TurnOffPeople(BladePose& pose, const ShapeParts& parts, const BladeLiving& living,
                          SlideRefusal* why = nullptr) {
	SlideRefusal unused = SlideRefusal::None;
	SlideRefusal& refused = why != nullptr ? *why : unused;
	for (UInt32 pass = 0; pass <= kTurnOffPasses; ++pass) {
		NiPoint3 at{0.0f, 0.0f, 0.0f};
		NiPoint3 out{0.0f, 0.0f, 0.0f};
		const BodySpan* deepest = parts.count > 0 ? parts.span[0] : nullptr;
		const float depth = DeepestInShape(pose, parts, living, false, &at, &out, &deepest);
		if (!(depth > kTurnOffSlackUnits)) {
			return true;
		}
		if (pass == kTurnOffPasses || !(out.LengthSquared() > 0.5f)) {
			refused = pass == kTurnOffPasses ? SlideRefusal::StillIn : SlideRefusal::NoWayOut;
			return false;
		}
		const NiPoint3 lever = at - pose.pos;
		const float leverUnits = math::Sqrt(lever.LengthSquared());
		const NiPoint3 along = Normalised(BladePointAt(pose, *deepest, 1.0f) - BladePointAt(pose, *deepest, 0.0f),
		                                  NiPoint3{0.0f, 0.0f, 1.0f});
		const NiPoint3 sideways = out - along * DotOf3(out, along);
		const float sidewaysShare = math::Sqrt(sideways.LengthSquared());
		if (!(leverUnits >= kTurnOffLeastLeverUnits) || !(sidewaysShare >= kTurnOffLeastSideways)) {
			refused = !(leverUnits >= kTurnOffLeastLeverUnits) ? SlideRefusal::GripIn : SlideRefusal::AlongBlade;
			return false;
		}
		const NiPoint3 axis = Normalised(CrossOf3(lever, sideways), NiPoint3{0.0f, 0.0f, 0.0f});
		if (!(axis.LengthSquared() > 0.5f)) {
			refused = SlideRefusal::NoWayOut;
			return false;
		}
		float angle = math::Atan2((depth + kTurnOffSpareUnits) / sidewaysShare, leverUnits);
		angle = angle > kTurnOffMostRadiansAPass ? kTurnOffMostRadiansAPass : angle;
		pose.rot = RotationAbout(axis, angle) * pose.rot;
	}
	refused = SlideRefusal::StillIn;
	return false;
}

// How far from `a` toward `b` the blade goes while clear of the living: the
// share, 1 all of the way. `shallowFree` lets it graze them (a way that only
// has to stay out of them), else it stays the margin short.
inline float ShareOffPeople(const BladePose& a, const BladePose& b, const ShapeParts& parts, const BladeLiving& living,
                            bool shallowFree) {
	float good = 0.0f;
	for (UInt32 k = 1; k <= kTurnOffSteps; ++k) {
		const float share = static_cast<float>(k) / static_cast<float>(kTurnOffSteps);
		if (DeepestInShape(PoseBetween(a, b, share), parts, living, shallowFree) > kTurnOffSlackUnits) {
			float bad = share;
			for (UInt32 i = 0; i < kTurnOffHalvings; ++i) {
				const float mid = (good + bad) * 0.5f;
				if (DeepestInShape(PoseBetween(a, b, mid), parts, living, shallowFree) > kTurnOffSlackUnits) {
					bad = mid;
				} else {
					good = mid;
				}
			}
			return good;
		}
		good = share;
	}
	return 1.0f;
}

// The pose a person's hold leaves the blade at when it slides off them: the
// grip at the target's, the blade turned out of them, then toward the
// target's turn as far as they let it (`free` when all of the way - it came
// off them). False when it cannot slide: a step of the grip's way cannot be
// turned out of them, the way there crosses them, or it goes through a
// wall, a stopper or a shield.
template <class World>
bool SlideOffPeople(World& world, const BladePose& start, const BladePose& target, const BladeContactFrame& f,
                    const BladeLiving& living, BladePose& out, bool& free, SlideRefusal* why = nullptr) {
	SlideRefusal unused = SlideRefusal::None;
	SlideRefusal& refused = why != nullptr ? *why : unused;
	BladePose p = start;
	// the line and a head, each by its own reach
	ShapeParts parts;
	parts.Add(f.span, f.marginUnits, &f.section);
	parts.Add(f.span2, f.margin2Units >= 0.0f ? f.margin2Units : f.marginUnits, &f.section2);
	// The grip to the target's in steps (kSlideStepUnits).
	const NiPoint3 move = target.pos - start.pos;
	UInt32 steps = static_cast<UInt32>(math::Sqrt(move.LengthSquared()) / kSlideStepUnits) + 1u;
	steps = steps > kSlideMostSteps ? kSlideMostSteps : steps;
	BladePose from = start;
	for (UInt32 k = 1; k <= steps; ++k) {
		p.pos = start.pos + move * (static_cast<float>(k) / static_cast<float>(steps));
		p.rot = from.rot;
		if (!TurnOffPeople(p, parts, living, &refused)) {
			return false;
		}
		if (ShareOffPeople(from, p, parts, living, true) < 1.0f) {
			refused = SlideRefusal::WayCrosses;
			return false;
		}
		from = p;
	}
	BladePose turned;
	turned.pos = p.pos;
	turned.rot = target.rot;
	const float share = ShareOffPeople(p, turned, parts, living, false);
	out = share < 1.0f ? PoseBetween(p, turned, share) : turned;
	free = !(share < 1.0f);
	const BladeLiving fixedOnly{nullptr, living.passed, living.swinging, living.stoppers, living.discs, living.hold};
	if (SweepShape(world, start, out, f, nullptr, fixedOnly).hit) {
		refused = SlideRefusal::Fixed;
		return false;
	}
	// Where it lands, as the step takes a pose: not inside the world, not
	// across someone (a post or a stopper between the swept points).
	if (ShapeInside(world, out, f)) {
		refused = SlideRefusal::Inside;
		return false;
	}
	if (ShapeAcross(out, f, living) != 0) {
		refused = SlideRefusal::Across;
		return false;
	}
	return true;
}

// Ending across a person no point of it met on the way (a frame of two
// steps): the slide as off a hit (strike-at-touch, 2026-11-11).
template <class World>
bool SlideOffAcross(World& world, const BladePose& start, const BladePose& target, const BladeContactFrame& f,
                    const BladeLiving& living, BladeContactVerdict& v, BladePose& out, bool& free) {
	const UInt32 across = f.slidesOffPeople ? ShapeAcross(target, f, living) : 0;
	if (across == 0 || IsStopperId(across) ||
	    !SlideOffPeople(world, start, target, f, living, out, free, &v.slideRefused)) {
		return false;
	}
	v.contact.hit = true;
	v.contact.actor = across;
	v.contact.body = 0;
	v.contact.point = BladePointAt(target, f.span, 0.5f);
	return true;
}

template <class World>
BladeContactVerdict StepBladeContact(BladeContactState& s, const BladeContactSettings& set,
                                     const BladeContactFrame& f, World& world) {
	BladeContactVerdict v;
	v.pose = f.wanted;
	if (!set.enabled || !f.active || !f.span.valid) {
		s = BladeContactState{};
		return v;
	}
	const BladeLiving living{f.bodies, &s.passed, f.swinging, f.stoppers, f.discs, f.bodiesHold};
	if (!s.have || f.jumped) {
		const bool inside = ShapeInside(world, f.wanted, f) || ShapeBehind(world, f.wanted, f);
		s.have = true;
		s.pose = f.wanted;
		s.through = inside;
		s.touching = false;
		ShapePassThoseItIsIn(f.wanted, f, living);
		v.through = inside;
		v.event = inside ? BladeContactEvent::StartedInside : BladeContactEvent::None;
		return v;
	}
	if (s.through) {
		s.pose = f.wanted;
		s.touching = false;
		ShapePassThoseItIsIn(f.wanted, f, living);
		if (!ShapeInside(world, f.wanted, f) && !ShapeBehind(world, f.wanted, f)) {
			s.through = false;
			v.event = BladeContactEvent::Rearmed;
		}
		v.through = s.through;
		StepShapePassed(s.passed, f.wanted, f);
		return v;
	}

	const BladePose start = s.pose;
	// Someone who walked into the blade where it rests is passed, not pushed
	// against.
	ShapePassThoseItIsIn(start, f, living);
	const bool slowed = f.followShare >= 0.0f && f.followShare < 1.0f;
	const BladePose target = slowed ? PoseBetween(start, f.wanted, f.followShare) : f.wanted;
	const BladeSweep first = SweepShape(world, start, target, f, &v.kicks, living);
	v.enteredBody = first.entered;
	// Where it goes, the first of these not across something along its
	// length: a post between two points, or the points' straight ways cutting
	// a corner the turning blade does not. Where it was is the last resort;
	// across something even there, it lets go.
	BladePose candidates[4];
	UInt32 count = 0;
	// A person's hold the blade slides off (slidesOffPeople): its pose first.
	bool held = false, slides = false, slidesFree = false;
	if (!first.hit) {
		held = slides = SlideOffAcross(world, start, target, f, living, v, candidates[count], slidesFree);
		count += slides ? 1u : 0u;
		candidates[count++] = target;
	} else {
		held = true;
		v.contact = first;
		if (f.slidesOffPeople && first.actor != 0 && !IsStopperId(first.actor)) {
			slides = SlideOffPeople(world, start, target, f, living, candidates[count], slidesFree, &v.slideRefused);
			count += slides ? 1u : 0u;
		}
		const BladePose stop = PushedOutOfShallow(PoseBetween(start, target, first.safeFraction), first);
		const BladePose slid = SlideTarget(stop, target, first.normal);
		const BladeSweep second = SweepShape(world, stop, slid, f, nullptr, living);
		v.enteredBody = v.enteredBody || second.entered;
		if (second.hit) {
			// What it slid into holds it now: the floor it scraped along led it
			// to a wall, or to someone lying on it.
			v.contact = second;
		}
		candidates[count++] = second.hit ? PoseBetween(stop, slid, second.safeFraction) : slid;
		candidates[count++] = stop;
	}
	candidates[count++] = start;
	bool placed = false;
	bool slidOff = false;
	BladePose pose = start;
	for (UInt32 i = 0; i < count && !placed; ++i) {
		if (ShapeInside(world, candidates[i], f)) {
			continue;
		}
		// Across someone held slowly (a limb between two points): not taken
		// either, and they are what holds it.
		const UInt32 across = ShapeAcross(candidates[i], f, living);
		if (across != 0) {
			if (!v.contact.hit) {
				v.contact.hit = true;
				v.contact.actor = across;
				v.contact.body = 0;
				v.contact.point = BladePointAt(candidates[i], f.span, 0.5f);
			}
			continue;
		}
		pose = candidates[i];
		placed = true;
		held = held || i > 0;
		slidOff = slides && slidesFree && i == 0;
	}

	const NiPoint3 gap = pose.pos - f.wanted.pos;
	v.gapUnits = math::Sqrt(gap.LengthSquared());
	v.gapRadians = AngleBetweenRotations(pose.rot, f.wanted.rot);
	if (slidOff) {
		// Off them at its tip: free where the hand is (or the hit-stop has it).
		s.pose = pose;
		s.touching = false;
		v.pose = pose;
		v.held = false;
		v.apart = slowed;
		v.event = BladeContactEvent::SlidOff;
		StepShapePassed(s.passed, pose, f);
		return v;
	}
	// The cap: a wall's or a stopper's is the weapon's; a person's is their
	// own (bodyLetGoUnits), and none at all when that is 0 - held until the
	// hand comes back.
	const bool byPerson = held && v.contact.hit && v.contact.actor != 0 && !IsStopperId(v.contact.actor);
	// Carried off: a person's hold that asks the controller to be inside them
	// (holdWhileInside), and it is not - walked or turned away from them, or
	// pushed clear through. Free where the controller is, nothing passed, so
	// the hand meets them again the moment it comes back.
	const bool carriedOff =
		placed && byPerson && f.holdWhileInside && !ShapeInsidePerson(f.wanted, f, v.contact.actor);
	const bool capped = !byPerson || set.bodyLetGoUnits > 0.0f;
	const float capUnits = byPerson ? set.bodyLetGoUnits : set.letGoUnits;
	// Sliding off them, turned further off the hand's turn than their turn
	// cap: it slices out of them (bodyLetGoRadians).
	const bool slicedOut = placed && held && byPerson && f.slidesOffPeople && set.bodyLetGoRadians > 0.0f &&
	                       v.gapRadians > set.bodyLetGoRadians;
	if (!placed || carriedOff || slicedOut ||
	    (held && capped && (v.gapUnits > capUnits || v.gapRadians > set.letGoRadians))) {
		// Let go. Pressed on into someone, the blade goes into them and they
		// are passed; anything else it went through, and it strikes nothing
		// until it is free.
		const bool intoPerson = placed && v.contact.hit && v.contact.actor != 0;
		if (intoPerson && !carriedOff) {
			s.passed.Add(v.contact.actor);
		}
		s.pose = f.wanted;
		s.through = !intoPerson;
		s.touching = false;
		v.pose = f.wanted;
		v.held = false;
		v.through = s.through;
		v.event = carriedOff ? BladeContactEvent::CarriedOff : BladeContactEvent::LetGo;
		StepShapePassed(s.passed, f.wanted, f);
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
	StepShapePassed(s.passed, pose, f);
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

}  // namespace obvr::game
