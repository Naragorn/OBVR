#pragma once

// The trunk stood upright (Nadi, 2026-11-28: the body under the headset
// always leans to the left or the right, never straight down).
//
// The trunk is held at one pose, the standing idle's, taken once from a frame
// the player stood still (BodyIkLogic.h, "The trunk"; game/BodyIk.cpp
// StepTrunk). The idle is not upright: it breathes and sways from one leg to
// the other, so whichever frame was taken, its hips stand a few units to one
// side and the spine bends aside - and the take accepts a head up to 12 units
// off the root across the ground (HeadOffsetStands). Nothing after the take
// undoes that: the torso turns the body about the vertical only, the chest's
// correction takes out its twist only, and the placement hangs the root from
// the head with the held offset as it is (BodyPlacement.h). So the body hung
// aside from the headset for as long as the skeleton lived, to whichever side
// the frame taken happened to sway (derived from the code, 2026-11-28; the
// headset to confirm).
//
// So the pose is stood upright once, as it is taken: each segment of the chain
// from the pelvis to the head is turned about the body's forward axis until it
// has nothing across the body - its lean forward or back kept, the spine's own
// curve - and the hips are moved across, level, over the skeleton's root, which
// stands between the feet. A spine has no bend aside standing upright; what the
// idle sways is gone, what it bows forward stays.
//
// Each joint's turn is applied in the world, about the joint's own origin, and
// carries every joint above it: turning joint k by t_k changes the side angle
// of every segment from k up by t_k (turns about one axis add). For each
// segment's side angle a_k to come to zero, t_k = a_(k-1) - a_k.
//
// Plain values only, covered by body_upright_test; game/BodyUpright.cpp
// reads and writes the bones.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

inline constexpr UInt32 kUprightMostJoints = 8;
// A segment leaning aside further than this is no standing pose's: left as it
// is (a spine bends aside about 30 degrees at the most).
inline constexpr float kUprightMostRadians = 30.0f * math::kDegreesToRadians;
// A segment shorter than this across the body's side plane (seen from the
// front) says nothing about a lean aside: it is given its lower neighbour's.
inline constexpr float kUprightShortUnits = 0.5f;
// The hips further across from the root than this are no standing pose's
// (HeadOffsetStands' 12 units): not moved.
inline constexpr float kUprightMostAsideUnits = 12.0f;

namespace upright_detail {
inline bool Finite(float v) { return v == v && v - v == 0.0f; }
inline bool Finite(const NiPoint3& p) { return Finite(p.x) && Finite(p.y) && Finite(p.z); }
}  // namespace upright_detail

// The body's right, level, from its yaw (HeadingOf's sense: the yaw of the
// right axis), and its forward, the up crossed with it.
inline NiPoint3 UprightRight(float bodyYaw) { return NiPoint3{math::Cos(bodyYaw), math::Sin(bodyYaw), 0.0f}; }
inline NiPoint3 UprightForward(float bodyYaw) { return NiPoint3{-math::Sin(bodyYaw), math::Cos(bodyYaw), 0.0f}; }

// How far a segment leans aside, seen from the front: the angle from up
// towards the body's right. False when it is too short across to say.
inline bool SideAngle(const NiPoint3& segment, const NiPoint3& right, float& radians) {
	const float side = segment.x * right.x + segment.y * right.y + segment.z * right.z;
	const float up = segment.z;
	if (!(side * side + up * up >= kUprightShortUnits * kUprightShortUnits)) {
		return false;
	}
	radians = math::Atan2(side, up);
	return true;
}

struct TrunkUpright {
	bool valid = false;
	UInt32 turns = 0;                       // joints turned: one fewer than the joints given
	float turn[kUprightMostJoints] = {};    // each joint's turn about the body's forward, radians
	float leanRadians = 0.0f;               // the head off the pelvis aside before, seen from the front
	float hipsAsideUnits = 0.0f;            // the pelvis off the root towards the body's right before
	bool moveHips = false;                  // the pelvis to be moved back over the root
};

// The turns that stand `count` joints upright - the pelvis first, the head
// last, in the world - for a body facing `bodyYaw`, and how far the pelvis
// stands across from `root`. Not valid for fewer than two joints or more
// than kUprightMostJoints, numbers that are not numbers, or a segment leaning
// aside past kUprightMostRadians: such a pose is left as it is.
inline TrunkUpright UprightTrunk(const NiPoint3* joints, UInt32 count, const NiPoint3& root, float bodyYaw) {
	using upright_detail::Finite;
	TrunkUpright out;
	if (joints == nullptr || count < 2 || count > kUprightMostJoints || !Finite(root) || !Finite(bodyYaw)) {
		return out;
	}
	for (UInt32 i = 0; i < count; ++i) {
		if (!Finite(joints[i])) {
			return out;
		}
	}
	const NiPoint3 right = UprightRight(bodyYaw);
	float below = 0.0f;  // the side angle the segment below had before; none below the pelvis
	for (UInt32 k = 0; k + 1 < count; ++k) {
		float angle = below;  // too short across to say: its neighbour's, so it is not turned
		SideAngle(joints[k + 1] - joints[k], right, angle);
		if (angle > kUprightMostRadians || angle < -kUprightMostRadians) {
			return TrunkUpright{};
		}
		out.turn[k] = below - angle;
		below = angle;
	}
	out.turns = count - 1;
	SideAngle(joints[count - 1] - joints[0], right, out.leanRadians);
	const NiPoint3 hips = joints[0] - root;
	out.hipsAsideUnits = hips.x * right.x + hips.y * right.y;
	out.moveHips = out.hipsAsideUnits <= kUprightMostAsideUnits && out.hipsAsideUnits >= -kUprightMostAsideUnits;
	out.valid = true;
	return out;
}

}  // namespace obvr::game
