#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The weapon in the hand, and the hands themselves, push the objects in the
// room (the tester, 2026-09-28, after the held object did: "mach jetzt die
// kollisionen für waffe und hände").
//
// Not with bodies of their own. HIGGS gives each hand and the weapon a
// keyframed rigid body of its own (docs/holding-objects-spec.md, research of
// 2026-09-27); in Oblivion that needs the rigid body and shape constructors
// and hkWorld::addEntity, none of them found. What is found, read and in use
// already: the world's ray pick (game::PickWorldSegment), the pick's hit
// collidable turned into its body (below), the body's motion - its type, its
// velocity at motion+0xD0, and setLinearVelocity at the motion's vtable +0x54
// (game/GrabPhysics.h). So each frame a few short rays trace where the
// weapon or the hand is and where it went since the last frame; a movable
// body a ray meets is given at least the speed the weapon or hand has at
// that point, along its direction. That is what a keyframed pusher does to
// what it touches, without the body: a swing knocks a cup off the table, a
// hand moved slowly shoves a plate along it. What it cannot do: be pushed
// back - a hand does not stop at a wall - or hold something up.
//
// The pick's collidable (the grab's own reading, 0x004806E0): a byte at +0x18
// says what owns it - 1 an entity, a rigid body - and the int at +0x10 is the
// owner's offset from the collidable; a rigid body keeps its collidable at
// +0x14 (GrabPhysics.h), which the offset has to point back to.
inline constexpr UInt32 kCollidableOwnerOffset = 0x10;
inline constexpr UInt32 kCollidableTypeOffset = 0x18;
inline constexpr UInt8 kCollidableOwnerEntity = 1;
inline constexpr UInt32 kBodyCollidableOffset = 0x14;

// A pusher: the weapon's blade, or a hand, as a segment in the world (game
// units): a at the grip or the wrist, b at the tip or the fingers.
struct PushSegment {
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
};

inline NiPoint3 SegmentPoint(const PushSegment& s, float t) { return s.a + (s.b - s.a) * t; }

// One ray a frame traces, and the point of the pusher (0 at a, 1 at b) at its
// start and its end.
struct PushRay {
	NiPoint3 from{0.0f, 0.0f, 0.0f};
	NiPoint3 to{0.0f, 0.0f, 0.0f};
	float s0 = 0.0f;
	float s1 = 0.0f;
};

inline constexpr UInt32 kPushRays = 4;
// A point that moved less than this since the last frame sweeps nothing: a
// ray of no length asks the world nothing sensible.
inline constexpr float kPushMinSweepUnits = 0.5f;

// The rays for a pusher that was at `last` and is at `now`: along it as it is
// now (what it rests in), and the sweeps of its far end, its middle and its
// near end since the last frame (what a swing passed through). Answers how
// many were written.
inline UInt32 PushRaysFor(const PushSegment& last, const PushSegment& now,
                          PushRay (&out)[kPushRays]) {
	UInt32 count = 0;
	out[count++] = PushRay{now.a, now.b, 0.0f, 1.0f};
	const float points[3] = {1.0f, 0.5f, 0.0f};
	for (float s : points) {
		const NiPoint3 from = SegmentPoint(last, s);
		const NiPoint3 to = SegmentPoint(now, s);
		if ((to - from).LengthSquared() >= kPushMinSweepUnits * kPushMinSweepUnits) {
			out[count++] = PushRay{from, to, s, s};
		}
	}
	return count;
}

// Where along the pusher a ray's hit lies: its fraction along the ray, from
// the ray's start to its end.
inline float PushPointOnRay(const PushRay& ray, float fraction) {
	return ray.s0 + (ray.s1 - ray.s0) * fraction;
}

// The pusher's velocity at a point s of it, units a second.
inline NiPoint3 PusherVelocityAt(const PushSegment& last, const PushSegment& now, float s,
                                 float dtSeconds) {
	if (!(dtSeconds > 0.0f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return (SegmentPoint(now, s) - SegmentPoint(last, s)) * (1.0f / dtSeconds);
}

// A frame too long - a hitch, a load - makes no speed: the pusher would
// seem to fly and fling everything it passed.
inline constexpr float kPushMaxFrameSeconds = 0.1f;
// Slower than this the pusher leaves bodies alone: a hand resting against a
// bottle does not creep it along. 0.3 m/s, in Havok units (a tenth of a metre).
inline constexpr float kPushMinHavokPerSecond = 3.0f;
// And a body is never given more than this: 15 m/s.
inline constexpr float kPushMaxHavokPerSecond = 150.0f;

// The body's new velocity when a pusher moving at `pusherV` meets it (both
// Havok units a second): at least the pusher's speed along the pusher's
// direction - what it already has across that direction, or beyond it, it
// keeps. False when nothing changes: the pusher too slow, or the body already
// that fast that way.
inline bool PushedVelocity(const NiPoint3& bodyV, const NiPoint3& pusherV, NiPoint3& out) {
	const float speed = math::Sqrt(pusherV.LengthSquared());
	if (!(speed >= kPushMinHavokPerSecond)) {
		return false;
	}
	const NiPoint3 dir = pusherV * (1.0f / speed);
	const float along = bodyV.x * dir.x + bodyV.y * dir.y + bodyV.z * dir.z;
	const float want = speed < kPushMaxHavokPerSecond ? speed : kPushMaxHavokPerSecond;
	if (along >= want) {
		return false;
	}
	out = bodyV + dir * (want - along);
	return true;
}

// The weapon's blade from the grip along the hand's forward: the weapon's
// scene bound is round the whole weapon, so its diameter is about the
// weapon's length. A bound that cannot be right (nothing, or a house) gives
// a longsword's.
inline constexpr float kPushFallbackBladeUnits = 60.0f;

inline float BladeLengthFromBound(float boundRadius) {
	if (!(boundRadius > 5.0f && boundRadius < 120.0f)) {
		return kPushFallbackBladeUnits;
	}
	return 1.9f * boundRadius;
}

// A hand, from the wrist to the fingertips, along its forward: behind the
// grip and ahead of it, game units (70 a metre).
inline constexpr float kPushHandBackUnits = 4.0f;
inline constexpr float kPushHandAheadUnits = 9.0f;

inline PushSegment HandSegment(const NiPoint3& grip, const NiPoint3& forward) {
	return PushSegment{grip - forward * kPushHandBackUnits, grip + forward * kPushHandAheadUnits};
}

inline PushSegment BladeSegment(const NiPoint3& grip, const NiPoint3& forward, float length) {
	return PushSegment{grip, grip + forward * length};
}

// The pushers, each frame: the weapon hand (its blade while a melee weapon is
// drawn, else the hand) and the other hand.
enum class Pusher : UInt8 { WeaponHand, OtherHand, Count };

struct PushFrame {
	float dtSeconds = 0.0f;
	bool valid[static_cast<int>(Pusher::Count)] = {};
	bool blade[static_cast<int>(Pusher::Count)] = {};  // for the log
	PushSegment segment[static_cast<int>(Pusher::Count)];
};

// Once per frame while Full VR runs in the world; `enabled` false (a menu,
// the setting off) forgets where the pushers were, so the next frame does
// not sweep from long ago.
void StepWorldPush(bool enabled, const PushFrame& frame);

// What a pick met, for the blade's contact (game/BladeContactLogic.h): the
// rigid body its collidable belongs to, that body's motion type (the
// motion's vtable +0x08) and its layer (the low seven bits of its filter,
// body+0x30). False when the collidable is no rigid body's.
bool ReadPickBody(UInt32 collidable, UInt32& body, UInt32& motionType, UInt32& layer);

// Gives a movable body the blade passed at least the blade's speed there
// (units a second), as a pusher's ray does; answers whether it did.
bool KickBodyByBlade(UInt32 body, const NiPoint3& unitsPerSecond);

}  // namespace obvr::game
