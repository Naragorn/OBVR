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

struct BladeSweep {
	bool hit = false;
	float fraction = 1.0f;      // of the move, where the first point met something fixed
	float safeFraction = 1.0f;  // held short of it by the margin
	float along = 0.0f;         // the point of the blade that met it: 0 the guard, 1 the tip
	NiPoint3 point{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 1.0f};  // of the surface, against the point's way
	UInt32 body = 0;
	float speedIn = 0.0f;  // units a second into the surface
};

// The blade's points from where they were to where they would be, the
// earliest that meets something fixed. Each point's ray reaches the margin
// past its end, so a point is never left nearer a surface than the margin
// - a ray that began on a face could not see it. Movable things passed are
// kicked with the speed of the point that passed them (none without a time).
template <class World>
BladeSweep SweepBlade(World& world, const BladePose& from, const BladePose& to, const BodySpan& span, float dtSeconds,
                      BladeKicks* kicks) {
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
		const NiPoint3 velocity = dtSeconds > 0.0f ? way * (1.0f / dtSeconds) : NiPoint3{0.0f, 0.0f, 0.0f};
		const BladeRayHit h = FirstStopper(world, a, a + dir * reach, [&](const BladeRayHit& m) {
			if (kicks != nullptr && dtSeconds > 0.0f && m.fraction * reach <= length) {
				kicks->Add(m.body, velocity);
			}
		});
		if (!h.hit) {
			continue;
		}
		const float atUnits = h.fraction * reach;
		const float fraction = atUnits < length ? atUnits / length : 1.0f;
		if (best.hit && fraction >= best.fraction) {
			continue;
		}
		NiPoint3 n = h.normal;
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
		best.point = h.point;
		best.normal = n;
		best.body = h.body;
		const float into = -DotOf3(way, n);
		best.speedIn = dtSeconds > 0.0f && into > 0.0f ? into / dtSeconds : 0.0f;
	}
	return best;
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
};

struct BladeContactVerdict {
	BladePose pose;        // where the blade is drawn
	bool held = false;     // something fixed held it this frame
	bool through = false;  // it went through: it strikes nothing
	BladeContactEvent event = BladeContactEvent::None;
	BladeSweep contact;    // what held it, when a sweep found it
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
	if (!s.have || f.jumped) {
		const bool inside = BladeInside(world, f.wanted, f.span) || BladeBehind(world, f.eye, f.wanted, f.span);
		s.have = true;
		s.pose = f.wanted;
		s.through = inside;
		s.touching = false;
		v.through = inside;
		v.event = inside ? BladeContactEvent::StartedInside : BladeContactEvent::None;
		return v;
	}
	if (s.through) {
		s.pose = f.wanted;
		s.touching = false;
		if (!BladeInside(world, f.wanted, f.span) && !BladeBehind(world, f.eye, f.wanted, f.span)) {
			s.through = false;
			v.event = BladeContactEvent::Rearmed;
		}
		v.through = s.through;
		return v;
	}

	const BladePose start = s.pose;
	const BladeSweep first = SweepBlade(world, start, f.wanted, f.span, f.dtSeconds, &v.kicks);
	// Where it goes, the first of these not across something along its
	// length: a post between two points, or the points' straight ways cutting
	// a corner the turning blade does not. Where it was is the last resort;
	// across something even there, it lets go.
	BladePose candidates[3];
	UInt32 count = 0;
	bool held = false;
	if (!first.hit) {
		candidates[count++] = f.wanted;
	} else {
		held = true;
		v.contact = first;
		const BladePose stop = PoseBetween(start, f.wanted, first.safeFraction);
		const BladePose slid = SlideTarget(stop, f.wanted, first.normal);
		const BladeSweep second = SweepBlade(world, stop, slid, f.span, f.dtSeconds, nullptr);
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
		s.pose = f.wanted;
		s.through = true;
		s.touching = false;
		v.pose = f.wanted;
		v.held = false;
		v.through = true;
		v.event = BladeContactEvent::LetGo;
		return v;
	}
	v.pose = pose;
	v.held = held;
	if (held && !s.touching) {
		v.event = BladeContactEvent::Touched;
	}
	s.touching = held;
	s.pose = pose;
	return v;
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
