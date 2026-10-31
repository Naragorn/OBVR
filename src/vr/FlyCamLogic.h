#pragma once

// The sticks on the engine's fly camera (game/FlyCam.h; Nadi, 2026-10-31:
// "linker stick, vor zurück seitlich. rechter stick, dreht oder snappt je
// nachdem ob das an ist. rechter stick hoch lässt einen hoch fliegen, runter
// runter fliegen"). The engine's own step moves the fly camera from the
// movement keys, but the harness saw it stand still under OBVR's stick keys
// (flycam-probe, 2026-10-31), and it has no up or down at all - so OBVR moves
// it itself, from the raw sticks:
//
// - the left stick flies along the head's heading (level: forward, back,
//   sideways), at FlyCamSpeed units a second, scaled by how far it is pushed;
// - the right stick's x turns: the snap turn's steps when snap turning is on,
//   else FlyCamTurnDegrees a second;
// - the right stick's y flies straight up or down at the same speed.
//
// Inside the dead zone an axis does nothing. Yaw is rotZ's: forward is
// (sin, cos), clockwise grows it; the head's turn comes in the walk
// direction's sign (subtracted from the camera's yaw, PlayerYawForGaze).
// Pure, covered by fly_cam_test.

#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::vr {

struct FlyCamSettings {
	float speedUnits = 350.0f;   // [Hands] FlyCamSpeed, units a second (about 5 m/s)
	float turnDegrees = 120.0f;  // [Hands] FlyCamTurnDegrees, a second, for the smooth turn
	float deadZone = 0.15f;
};

struct FlyCamInput {
	float moveX = 0.0f;  // the left stick
	float moveY = 0.0f;
	float turnX = 0.0f;  // the right stick
	float liftY = 0.0f;
	float headYaw = 0.0f;       // the head's turn from the body, radians, the walk direction's sign
	float snapYaw = 0.0f;       // the snap turn's step this frame (radians, to add to rotZ)
	bool snapOwnsStick = false;  // snap turning: no smooth turn
	float dtSeconds = 0.0f;
};

struct FlyCamStep {
	NiPoint3 move{0.0f, 0.0f, 0.0f};  // to add to the camera's place
	float yaw = 0.0f;                 // to add to its yaw
};

inline float FlyCamAxis(float v, float deadZone) {
	if (!(v == v)) {
		return 0.0f;
	}
	const float a = v < 0.0f ? -v : v;
	if (a <= deadZone) {
		return 0.0f;
	}
	const float clamped = a > 1.0f ? 1.0f : a;
	const float share = deadZone < 1.0f ? (clamped - deadZone) / (1.0f - deadZone) : 0.0f;
	return v < 0.0f ? -share : share;
}

// One frame. `yaw` the fly camera's yaw now.
inline FlyCamStep StepFlyCam(const FlyCamInput& in, float yaw, const FlyCamSettings& s) {
	FlyCamStep out;
	const float dt = in.dtSeconds > 0.0f && in.dtSeconds < 0.25f ? in.dtSeconds : 0.0f;
	const float x = FlyCamAxis(in.moveX, s.deadZone);
	const float y = FlyCamAxis(in.moveY, s.deadZone);
	const float lift = FlyCamAxis(in.liftY, s.deadZone);
	const float turn = in.snapOwnsStick ? 0.0f : FlyCamAxis(in.turnX, s.deadZone);
	const float step = s.speedUnits * dt;
	const float heading = yaw - in.headYaw;
	const float fs = math::Sin(heading);
	const float fc = math::Cos(heading);
	// Forward (sin, cos), right (cos, -sin): rotZ grows clockwise seen from above.
	out.move.x = (fs * y + fc * x) * step;
	out.move.y = (fc * y - fs * x) * step;
	out.move.z = lift * step;
	out.yaw = in.snapOwnsStick ? in.snapYaw : turn * s.turnDegrees * (3.14159265f / 180.0f) * dt;
	return out;
}

}  // namespace obvr::vr
