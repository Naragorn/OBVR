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
// position. The position is discarded here - 0.0.3 delivers 3DoF, no head
// movement through space.
//
// The parameter is deliberately a bare float[3][4] rather than an
// HmdMatrix34_t, which keeps this header free of OpenVR declarations and
// natively testable like the rest of the maths.
//
// The result is in OpenVR convention, which matches OpenXR (X right, Y up,
// -Z forward) - so it still has to go through FromOpenXR.
Quaternion FromOpenVRMatrix(const float matrix[3][4]);

}  // namespace obvr::vr
