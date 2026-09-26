#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The held object's physics around a grab (docs/holding-objects-spec.md):
// it passes through the player's own body while held, and it leaves the
// hand with the hand's speed when let go. Read in Oblivion.exe 1.2.0.416:
//
// - The grabbed body: rb = [[player+0x574]+8]+0x18 (the update's own chain at
//   0x0066D97C); its bhkRigidBody wrapper at [rb+0x0C], its hkMotion at
//   [rb+0x50].
// - Its collision filter at rb+0x30 (the collidable at rb+0x14, filter at
//   +0x1C): layer in the low 6 bits, 0x4000 "no collision", the system group
//   in the high 16 bits. The engine's rule (0x008A7F70): two bodies of the
//   same group do not collide - unless both are linked (0x8000) or both
//   bipeds (layer 8). The player's controller is group 9 (0x0066AA7F sets
//   it), read from the controller's filter (0x0065ABE0, its word at +2) as
//   the grab ray does at 0x0066DAAC.
// - 0x0089F4D0, thiscall(bhkRigidBody*, UInt32 group), ret 4: replaces the
//   group and refreshes the world's view of the body
//   (hkWorld::updateCollisionFilterOnEntity, 0x0089B630, through the
//   wrapper's vtable +0x80). So the held body takes the player's group and
//   stops colliding with the player's capsule - the object "stopped at
//   something" before it reached the body (2026-09-26) - while it keeps
//   colliding with the world; on release the old group goes back.
// - The throw: vanilla's own release (0x0066A670) leaves the body with the
//   spring's damped speed, a soft drop. The engine's Telekinesis throw
//   (0x006A7830) releases, activates the body (0x008A6410,
//   thiscall(hkRigidBody*)) and pushes it through the motion's vtable. OBVR
//   does the same once the release is seen: activate, then
//   hkMotion::setLinearVelocity (motion vtable +0x54, thiscall(motion,
//   const hkVector4*), ret 4, the vector 16-byte aligned) to the palm's
//   speed. Havok units are game units times 0.142877 (the scale at
//   0x00A39088).
inline constexpr UInt32 kSetBodyGroup = 0x0089F4D0;
inline constexpr UInt32 kActivateBody = 0x008A6410;
inline constexpr UInt32 kControllerFilterOf = 0x0065ABE0;
inline constexpr UInt32 kBodyWrapperOffset = 0x0C;
inline constexpr UInt32 kBodyFilterOffset = 0x30;
inline constexpr UInt32 kBodyMotionOffset = 0x50;
inline constexpr UInt32 kMotionSetLinearVelocitySlot = 0x54;
inline constexpr UInt32 kPlayerCollisionGroupFallback = 9;
inline constexpr float kHavokPerUnit = 0.142877f;

inline UInt32 FilterGroup(UInt32 filter) { return filter >> 16; }

// Placing the body where the object was seen, in the in-hand mode: the
// object sat fixed in the palm while the spring pulled the physics body
// only near it, so letting go would jump it to wherever the body had got to.
// bhkRigidBody's vtable +0xA0 (0x008A2FB0) is SetTranslationAndRotation
// (thiscall, const hkVector4* pos, const hkQuaternion* rot, ret 8, both
// 16-byte aligned); the engine's own node-to-Havok push (0x0089EAE0) calls
// it inside the Havok critical section at 0x00BA7B00 (enter 0x0043F2E0, leave
// 0x0043F300, thiscall on that address), with the node's world translation
// times the Havok scale and its world rotation as a quaternion (x, y, z, w).
inline constexpr UInt32 kBodySetTranslationAndRotationSlot = 0xA0;
inline constexpr UInt32 kHavokLock = 0x00BA7B00;
inline constexpr UInt32 kHavokLockEnter = 0x0043F2E0;
inline constexpr UInt32 kHavokLockLeave = 0x0043F300;

// A rotation matrix (NiMatrix33, data[row][column]) as a unit quaternion
// x, y, z, w - the same rotation, Havok's order (NiQuaternion::FromRotation
// 0x007150F0 by the standard formulas, reordered). Shepperd's choice of the
// largest diagonal term keeps it exact near 180 degrees.
inline void QuaternionFromRotation(const NiMatrix33& m, float q[4]) {
	const float m00 = m.data[0][0], m11 = m.data[1][1], m22 = m.data[2][2];
	const float trace = m00 + m11 + m22;
	float x, y, z, w;
	if (trace > 0.0f) {
		const float s = math::Sqrt(trace + 1.0f) * 2.0f;
		w = 0.25f * s;
		x = (m.data[2][1] - m.data[1][2]) / s;
		y = (m.data[0][2] - m.data[2][0]) / s;
		z = (m.data[1][0] - m.data[0][1]) / s;
	} else if (m00 > m11 && m00 > m22) {
		const float s = math::Sqrt(1.0f + m00 - m11 - m22) * 2.0f;
		w = (m.data[2][1] - m.data[1][2]) / s;
		x = 0.25f * s;
		y = (m.data[0][1] + m.data[1][0]) / s;
		z = (m.data[0][2] + m.data[2][0]) / s;
	} else if (m11 > m22) {
		const float s = math::Sqrt(1.0f + m11 - m00 - m22) * 2.0f;
		w = (m.data[0][2] - m.data[2][0]) / s;
		x = (m.data[0][1] + m.data[1][0]) / s;
		y = 0.25f * s;
		z = (m.data[1][2] + m.data[2][1]) / s;
	} else {
		const float s = math::Sqrt(1.0f + m22 - m00 - m11) * 2.0f;
		w = (m.data[1][0] - m.data[0][1]) / s;
		x = (m.data[0][2] + m.data[2][0]) / s;
		y = (m.data[1][2] + m.data[2][1]) / s;
		z = 0.25f * s;
	}
	const float length = math::Sqrt(x * x + y * y + z * z + w * w);
	const float inv = length > 0.0f ? 1.0f / length : 0.0f;
	q[0] = x * inv;
	q[1] = y * inv;
	q[2] = z * inv;
	q[3] = w * inv;
}

// How fast the held point has to move for letting go to be a throw, units a
// second: half a metre a second. Slower is a set-down: the body is left
// still where it was placed.
constexpr float kThrowMinUnitsPerSecond = 35.0f;

// The throw's speed is SteamVR's own velocity of the controller - filtered
// in the tracking itself - carried to the held point (v + w x r), not a
// difference of positions: five frames of positions made short, quick moves
// shoot objects away (2026-09-26). As in the SteamVR Interaction System
// (Throwable.cs, ReleaseStyle.AdvancedEstimation and
// scaleReleaseVelocityCurve,
// github.com/ValveSoftware/steamvr_unity_plugin): the fastest of the last
// frames is taken, so a hand already slowing when the grip opens still
// throws with its peak; and the speed is eased in, a tenth at rest rising
// to all of it at kThrowFullSpeedUnits, which keeps a drop, a toss and a
// throw apart.
constexpr UInt32 kThrowHistory = 8;
constexpr float kThrowFullSpeedUnits = 210.0f;  // 3 m/s

struct VelocityHistory {
	NiPoint3 v[kThrowHistory];
	UInt32 count = 0;
};

inline void PushVelocity(VelocityHistory& h, const NiPoint3& velocity) {
	if (h.count == kThrowHistory) {
		for (UInt32 i = 1; i < kThrowHistory; ++i) {
			h.v[i - 1] = h.v[i];
		}
		--h.count;
	}
	h.v[h.count] = velocity;
	++h.count;
}

// The ease: 0.1 at rest to 1 at full speed, smooth at both ends.
inline float ThrowEase(float speed) {
	float t = speed / kThrowFullSpeedUnits;
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return 0.1f + 0.9f * t * t * (3.0f - 2.0f * t);
}

// The velocity to leave with, units a second: the fastest sample kept, eased
// and times the strength - zero with no samples, a strength of zero, or a
// hand slower than a throw.
inline NiPoint3 ThrowVelocity(const VelocityHistory& h, float strength) {
	if (h.count == 0 || !(strength > 0.0f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	UInt32 best = 0;
	for (UInt32 i = 1; i < h.count; ++i) {
		if (h.v[i].LengthSquared() > h.v[best].LengthSquared()) {
			best = i;
		}
	}
	const NiPoint3 v = h.v[best];
	const float speed = math::Sqrt(v.LengthSquared());
	if (speed < kThrowMinUnitsPerSecond) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return v * (ThrowEase(speed) * strength);
}

// The held point's velocity from the controller's: v + w x r, r from the
// controller to the point.
inline NiPoint3 PointVelocity(const NiPoint3& v, const NiPoint3& w, const NiPoint3& r) {
	return NiPoint3{v.x + (w.y * r.z - w.z * r.y), v.y + (w.z * r.x - w.x * r.z),
	                v.z + (w.x * r.y - w.y * r.x)};
}

// Once per frame while the hand mode runs: follows the engine's grab (the
// spring at player+0x574), gives a held body the player's group while
// `passBody`, and on its release puts the group back and, with a strength
// above zero, sends the body off with the held point's velocity
// (velocityValid false when the hand is not tracked this frame).
void StepGrabPhysics(bool passBody, float throwStrength, bool velocityValid,
                     const NiPoint3& velocityUnits);

// Where the object was last seen in the hand (game::HeldObject writes it each
// frame it places the object): on release, the body is put there before it
// is sent off.
void NoteHeldPose(UInt32 ref, const NiMatrix33& rot, const NiPoint3& pos);

}  // namespace obvr::game
