#pragma once

#include "game/NiMath.h"

namespace obvr::vr {

// OpenXR delivers orientations as quaternions. Oblivion works with 3x3
// matrices in a different coordinate system. This header keeps the two apart
// and the conversion in one place.

struct Quaternion {
	float x;
	float y;
	float z;
	float w;

	static Quaternion Identity() { return Quaternion{0.0f, 0.0f, 0.0f, 1.0f}; }

	// The inverse of a unit quaternion. Needed for recentering: the stored
	// reference orientation is factored out of the current one.
	Quaternion Conjugate() const { return Quaternion{-x, -y, -z, w}; }

	// Composition. rhs first, then this.
	Quaternion operator*(const Quaternion& rhs) const;

	// Floating point drift over many frames lets the length wander off; a
	// quaternion that is not normalised would produce a scaling camera
	// matrix.
	Quaternion Normalized() const;

	float LengthSquared() const { return x * x + y * y + z * z + w * w; }
};

// Builds a quaternion from an axis and an angle. Mainly for the tests and the
// simulated head.
Quaternion FromAxisAngle(float axisX, float axisY, float axisZ, float degrees);

// Converts an orientation from OpenXR into Oblivion's camera space.
//
// The two coordinate systems differ in their axis assignment:
//
//     OpenXR                   Oblivion / Gamebryo
//     X = right                X = right
//     Y = up                   Y = forward
//     Z = back (-Z forward)    Z = up
//
// So the change of basis maps:
//
//     x_obl =  x_xr
//     y_obl = -z_xr
//     z_obl =  y_xr
//
// The corresponding matrix has determinant +1, so handedness is preserved and
// the vector part of the quaternion can be carried over directly.
//
// Cross-check against the axes confirmed in the game: a rotation about
// OpenXR's Y axis (looking left and right) becomes a rotation about
// Oblivion's Z axis, and Z is yaw there. A rotation about OpenXR's X axis
// stays X and therefore pitch.
Quaternion FromOpenXR(const Quaternion& openXrOrientation);

// Turns a quaternion into the rotation matrix Oblivion expects. Expects an
// already normalised quaternion in Oblivion convention.
NiMatrix33 ToMatrix(const Quaternion& rotation);

// Converts the rotation part of an OpenVR pose into a quaternion.
//
// OpenVR delivers poses as HmdMatrix34_t, that is float m[3][4] in row major
// order: the left three columns hold the rotation, the fourth holds the
// position. Only the rotation is read here; PositionFromOpenVRMatrix takes
// the fourth column.
//
// The parameter is deliberately a bare float[3][4] rather than an
// HmdMatrix34_t, which keeps this header free of OpenVR declarations and
// natively testable like the rest of the maths.
//
// The result is in OpenVR convention, which matches OpenXR (X right, Y up,
// -Z forward) - so it still has to go through FromOpenXR.
Quaternion FromOpenVRMatrix(const float matrix[3][4]);

// Reads the position out of an OpenVR pose: the fourth column of the same
// 3x4 matrix, in metres, still in OpenVR/OpenXR convention.
//
// The tracking universe is Seated, so the origin is where the user last set
// their seated zero in SteamVR. That origin is not OBVR's zero - the
// difference against a stored reference position is what matters, never the
// absolute value.
NiPoint3 PositionFromOpenVRMatrix(const float matrix[3][4]);

// The same change of basis as FromOpenXR, applied to a position rather than
// an orientation:
//
//     x_obl =  x_xr
//     y_obl = -z_xr
//     z_obl =  y_xr
//
// Leaning forward in the room is -Z in OpenXR and therefore +Y in Oblivion,
// which is forward there. Standing up is +Y in OpenXR and +Z in Oblivion,
// which is up. Both match, and they are the two directions a wrong sign would
// be noticed in first.
NiPoint3 PositionFromOpenXR(const NiPoint3& openXrPosition);

// Rotates a vector by a quaternion.
//
// Needed for recentering the position: the head offset is measured in the
// tracking universe, whose forward direction is wherever SteamVR's seated
// zero happens to point. Rotating the offset by the conjugate of the
// reference orientation expresses it in the frame the user was facing when
// they pressed the recenter key, which is the frame the camera lives in.
//
// v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
NiPoint3 Rotate(const Quaternion& rotation, const NiPoint3& v);

// The heading part of an orientation, with the horizon left level.
//
// What recentring is for is deciding which way is forward. It is not for
// deciding which way is up - and a reference that carries pitch and roll does
// exactly that: press the key with the head tilted and the whole world tilts
// with it, permanently, because every later pose is measured against a tilted
// zero. A horizon that is not level is one of the reliable ways to make
// somebody ill in a headset, and unlike most of them it never settles, because
// the inner ear keeps insisting and the picture keeps disagreeing.
//
// So only the heading is kept. Taken from where the head is looking projected
// onto the horizontal plane, which is the one part of an orientation that
// survives having its tilt removed - unlike reading an Euler angle out, which
// has no answer at all when the head is pointed straight up.
//
// In OpenVR's convention: X right, Y up, -Z forward.
Quaternion YawOnly(const Quaternion& orientation);

}  // namespace obvr::vr
