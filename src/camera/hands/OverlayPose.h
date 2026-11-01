#pragma once

// The pose of a panel hung in the room (an OpenVR overlay's transform in
// tracking space): at its point, its face turned to the eyes, upright. The
// stow spot's, the holster sign's and the shake sign's.
//
// Moved out of camera/CameraHook.cpp (StowSpotPose) on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.5), unchanged. Pure.

#include "core/MathFns.h"
#include "game/NiMath.h"
#include "vr/OpenVRTypes.h"

namespace obvr::camera::hands {

// The spot's pose: at its point, its face turned to the eyes, upright.
inline vr::openvr::HmdMatrix34 StowSpotPose(const NiPoint3& at, const NiPoint3& eyes) {
	NiPoint3 z = eyes - at;
	float length = math::Sqrt(z.LengthSquared());
	z = length > 1e-4f ? z * (1.0f / length) : NiPoint3{0.0f, 0.0f, 1.0f};
	// x = up cross z, level; y = z cross x.
	NiPoint3 x{z.z, 0.0f, -z.x};
	length = math::Sqrt(x.LengthSquared());
	x = length > 1e-4f ? x * (1.0f / length) : NiPoint3{1.0f, 0.0f, 0.0f};
	const NiPoint3 y{z.y * x.z - z.z * x.y, z.z * x.x - z.x * x.z, z.x * x.y - z.y * x.x};
	vr::openvr::HmdMatrix34 pose{};
	const NiPoint3 axes[3] = {x, y, z};
	for (int column = 0; column < 3; ++column) {
		pose.m[0][column] = axes[column].x;
		pose.m[1][column] = axes[column].y;
		pose.m[2][column] = axes[column].z;
	}
	pose.m[0][3] = at.x;
	pose.m[1][3] = at.y;
	pose.m[2][3] = at.z;
	return pose;
}

}  // namespace obvr::camera::hands
