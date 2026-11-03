#pragma once

// The headset turning the view while the mouse or the stick aims ([Look]
// HeadTurnsViewOnly; a Nexus user's request of 2026-11-02): the mouse and the
// stick turn and tilt the camera and the aim together, as on a monitor; the
// headset turns and tilts the camera on top of that, the aim not - the
// crosshair stays the reference point the head looks around.
//
// The camera is not the plain product base * head. With the mouse's tilt in
// the base, a turn of the head about the tilted camera's own up axis tips the
// horizon (the headset, 2026-09-07: "links/rechts dreht die Sicht hoch").
// Here the head is taken apart into its turn, its tilt and its roll, and put
// back on the base's turn with the two tilts added: the horizon stays level,
// the head's roll is its own. Pure, covered by free_aim_test.
//
// The game's camera axes: column 0 right, column 1 forward, column 2 up
// (core/Rotation.h ForwardOf); OpenVR's headset frame: x right, y up, -z
// forward.

#include <cmath>

#include "game/NiMath.h"
#include "vr/OpenVRTypes.h"

namespace obvr::camera {

// How far the view may tilt, mouse and head together: the engine's own 89.
inline constexpr float kFreeAimPitchMostDegrees = 89.0f;

// On only seated (Full VR's hands aim), with the headset steering the view
// at all (HeadLook) and a headset there.
inline bool FreeAimActive(bool setting, bool headLook, bool fullVr, bool headset) {
	return setting && headLook && !fullVr && headset;
}

// A turn about the world's up by `yaw` (to the left for positive), then a
// tilt about the turned right axis by `pitch` (up for positive), radians.
inline NiMatrix33 YawPitch(float yaw, float pitch) {
	const float cy = std::cos(yaw);
	const float sy = std::sin(yaw);
	const float cp = std::cos(pitch);
	const float sp = std::sin(pitch);
	NiMatrix33 m{};
	m.data[0][0] = cy;
	m.data[1][0] = sy;
	m.data[2][0] = 0.0f;
	m.data[0][1] = -sy * cp;
	m.data[1][1] = cy * cp;
	m.data[2][1] = sp;
	m.data[0][2] = sy * sp;
	m.data[1][2] = -cy * sp;
	m.data[2][2] = cp;
	return m;
}

// A roll about the forward axis by `roll` radians.
inline NiMatrix33 RollAbout(float roll) {
	const float c = std::cos(roll);
	const float s = std::sin(roll);
	NiMatrix33 m{};
	m.data[0][0] = c;
	m.data[2][0] = -s;
	m.data[1][1] = 1.0f;
	m.data[0][2] = s;
	m.data[2][2] = c;
	return m;
}

inline void TurnAndTiltOf(const NiMatrix33& m, float& yaw, float& pitch) {
	const float fx = m.data[0][1];
	const float fy = m.data[1][1];
	float fz = m.data[2][1];
	fz = fz > 1.0f ? 1.0f : (fz < -1.0f ? -1.0f : fz);
	yaw = std::atan2(-fx, fy);
	pitch = std::asin(fz);
}

inline NiMatrix33 Transpose33(const NiMatrix33& m) {
	NiMatrix33 t{};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			t.data[r][c] = m.data[c][r];
		}
	}
	return t;
}

// The view: the base's turn and the head's, the base's tilt and the head's
// added (held within kFreeAimPitchMostDegrees), then the head's roll.
inline NiMatrix33 FreeAimView(const NiMatrix33& base, const NiMatrix33& head) {
	float baseYaw = 0.0f;
	float basePitch = 0.0f;
	TurnAndTiltOf(base, baseYaw, basePitch);
	float headYaw = 0.0f;
	float headPitch = 0.0f;
	TurnAndTiltOf(head, headYaw, headPitch);
	const NiMatrix33 roll = Transpose33(YawPitch(headYaw, headPitch)) * head;
	const float most = kFreeAimPitchMostDegrees * 3.14159265f / 180.0f;
	float pitch = basePitch + headPitch;
	pitch = pitch > most ? most : (pitch < -most ? -most : pitch);
	return YawPitch(baseYaw + headYaw, pitch) * roll;
}

// Where `aimWorld` (the aim's direction in the world) lies seen from the
// view, in the headset's own frame (x right, y up, -z forward), unit length.
inline NiPoint3 AimInHeadset(const NiMatrix33& view, const NiPoint3& aimWorld) {
	const float right = view.data[0][0] * aimWorld.x + view.data[1][0] * aimWorld.y + view.data[2][0] * aimWorld.z;
	const float forward = view.data[0][1] * aimWorld.x + view.data[1][1] * aimWorld.y + view.data[2][1] * aimWorld.z;
	const float up = view.data[0][2] * aimWorld.x + view.data[1][2] * aimWorld.y + view.data[2][2] * aimWorld.z;
	const float len = std::sqrt(right * right + forward * forward + up * up);
	if (!(len > 1e-6f)) {
		return NiPoint3{0.0f, 0.0f, -1.0f};
	}
	return NiPoint3{right / len, up / len, -forward / len};
}

// The crosshair's quad `distanceMetres` along `direction` (the headset's
// frame, unit), facing back along it, upright: relative to the headset.
inline vr::openvr::HmdMatrix34 AimQuadMatrix(float distanceMetres, const NiPoint3& direction) {
	const NiPoint3 front{-direction.x, -direction.y, -direction.z};
	// Right: across the direction and the headset's up.
	NiPoint3 right{-direction.z, 0.0f, direction.x};
	float len = std::sqrt(right.x * right.x + right.z * right.z);
	right = len > 1e-6f ? NiPoint3{right.x / len, 0.0f, right.z / len} : NiPoint3{1.0f, 0.0f, 0.0f};
	const NiPoint3 up{front.y * right.z - front.z * right.y, front.z * right.x - front.x * right.z,
	                  front.x * right.y - front.y * right.x};
	vr::openvr::HmdMatrix34 m{};
	m.m[0][0] = right.x;
	m.m[1][0] = right.y;
	m.m[2][0] = right.z;
	m.m[0][1] = up.x;
	m.m[1][1] = up.y;
	m.m[2][1] = up.z;
	m.m[0][2] = front.x;
	m.m[1][2] = front.y;
	m.m[2][2] = front.z;
	m.m[0][3] = direction.x * distanceMetres;
	m.m[1][3] = direction.y * distanceMetres;
	m.m[2][3] = direction.z * distanceMetres;
	return m;
}

}  // namespace obvr::camera
