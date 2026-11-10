#pragma once

// The body's frame the holster places live in: metres right, forward and up
// from the eyes as they are when the head is upright, turned with the body
// rather than with the head (Nadi, 2026-11-10, placing the weapon places:
// the circles went with the head, and since one keeps looking around one
// could not put them anywhere).
//
// Before this the places were in vr::BodyRelative's frame: the eyes and the
// head's own heading. A glance aside turned all four places round the eyes,
// and looking down at them - which the window asks for - moved them forward
// and down with the eyes, which swing about the neck, and near straight down
// the heading itself was lost (its level forward a few millimetres long).
// The same frame decided the draws and stows in play.
//
// Now:
// - The heading is the torso's (game::StepTorsoYaw, the drawn body's own rule
//   and settings: a glance stays a glance, a turn held turns the body), from
//   a head heading that survives a bow (LevelHeading). While the circles are
//   up it is held still (`holdHeading`): the player stands to place them.
// - The origin is where the eyes would be with the head upright over the
//   neck: the neck (the eyes less kEyesFromNeck turned by the head) plus the
//   same offset turned by the body's heading alone. Upright and facing the
//   body, it is the eyes - so places fitted before stay where they were.
//
// Pure: plain values in, plain values out; covered by body_frame_test.

#include "core/MathFns.h"
#include "game/BodyIkLogic.h"
#include "game/NiMath.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// The eyes from the neck's pivot in the head's own frame (OpenVR: x right,
// y up, -z forward), metres: 7.5 cm up, 8.05 cm ahead - the neck model the
// hand scripts carry a scripted head's eyes round (test/HandScriptPose.h,
// there given as the Oculus SDK's, ANSUR-88 averages; not checked here).
// Not measured on the player: a real neck's pivot differs by some
// centimetres, which is what a deep bow can still move the places by.
inline constexpr NiPoint3 kEyesFromNeck{0.0f, 0.075f, -0.0805f};

// The head's heading on the level, as a unit (fx, fz) in tracking space
// (forward is -z). Its forward alone is lost as the head bows: straight down
// it has no length on the level. Bowed, the head's up leans the way the face
// looks (raised, the other way), so forward plus up (minus it, looking up)
// keeps its length at every bow. A head on its side with nothing level
// keeps -z.
inline void LevelHeading(const Quaternion& head, float& fx, float& fz) {
	const NiMatrix33 m = ToMatrix(head);
	const NiPoint3 forward = m * NiPoint3{0.0f, 0.0f, -1.0f};
	const NiPoint3 up = m * NiPoint3{0.0f, 1.0f, 0.0f};
	const float sign = forward.y <= 0.0f ? 1.0f : -1.0f;
	float x = forward.x + sign * up.x;
	float z = forward.z + sign * up.z;
	const float length = math::Sqrt(x * x + z * z);
	if (!(length > 1e-4f)) {
		x = 0.0f;
		z = -1.0f;
	} else {
		x /= length;
		z /= length;
	}
	fx = x;
	fz = z;
}

struct BodyFrame {
	bool valid = false;
	float forwardX = 0.0f;  // the body's forward on the level, tracking space
	float forwardZ = -1.0f;
	NiPoint3 origin{0.0f, 0.0f, 0.0f};  // the upright eyes, tracking space
};

struct BodyFrameState {
	game::TorsoYawState torso;
	BodyFrame last;
};

inline bool FiniteHead(const Quaternion& q, const NiPoint3& p) {
	const float sum = q.x + q.y + q.z + q.w + p.x + p.y + p.z;
	return sum == sum && sum - sum == 0.0f && q.LengthSquared() > 1e-6f;
}

// One frame. `holdHeading`: the heading stays as it was (the circles up).
// `moving`: walking or stepping through the room - standing, the torso holds
// through a look aside as the drawn body's does (game/BodyTorsoLogic.h). No
// head, or one that is not a number: the last frame as it was.
inline BodyFrame StepBodyFrame(BodyFrameState& s, bool headValid, const Quaternion& head, const NiPoint3& eyes,
                               float dtSeconds, const game::TorsoYawSettings& follow, bool holdHeading = false,
                               bool moving = true) {
	if (!headValid || !FiniteHead(head, eyes)) {
		return s.last;
	}
	float hx = 0.0f;
	float hz = 0.0f;
	LevelHeading(head, hx, hz);
	// Yaw about tracking's up, left positive: forward (-sin, -cos).
	const float headYaw = math::Atan2(-hx, -hz);
	float bodyYaw = headYaw;
	if (holdHeading && s.last.valid) {
		bodyYaw = math::Atan2(-s.last.forwardX, -s.last.forwardZ);
		// The torso's state kept on the held heading, so letting go does not
		// swing the body round.
		s.torso.valid = true;
		s.torso.torso = bodyYaw;
		s.torso.lastHead = headYaw;
	} else {
		bodyYaw = math::WrapAngle(headYaw + game::StepTorsoYaw(s.torso, headYaw, dtSeconds, follow, moving));
	}
	BodyFrame f;
	f.valid = true;
	f.forwardX = -math::Sin(bodyYaw);
	f.forwardZ = -math::Cos(bodyYaw);
	const NiPoint3 neck = eyes - ToMatrix(head) * kEyesFromNeck;
	// kEyesFromNeck turned by the body's heading alone: up as it is, its
	// forward along the body's forward.
	const float ahead = -kEyesFromNeck.z;
	f.origin = NiPoint3{neck.x + f.forwardX * ahead, neck.y + kEyesFromNeck.y, neck.z + f.forwardZ * ahead};
	s.last = f;
	return f;
}

// A tracking-space point in the body's frame: metres right, forward, up from
// the origin.
inline NiPoint3 ToBodyFrame(const BodyFrame& f, const NiPoint3& point) {
	const NiPoint3 d = point - f.origin;
	// Right is forward turned a quarter clockwise seen from above: (-fz, fx).
	return NiPoint3{d.x * -f.forwardZ + d.z * f.forwardX, d.x * f.forwardX + d.z * f.forwardZ, d.y};
}

// And back: a place in the body's frame in tracking space.
inline NiPoint3 FromBodyFrame(const BodyFrame& f, const NiPoint3& place) {
	return NiPoint3{f.origin.x + place.x * -f.forwardZ + place.y * f.forwardX, f.origin.y + place.z,
	                f.origin.z + place.x * f.forwardX + place.y * f.forwardZ};
}

}  // namespace obvr::vr
