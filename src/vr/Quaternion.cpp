#include "vr/Quaternion.h"

#include "core/MathFns.h"

namespace obvr::vr {

Quaternion Quaternion::operator*(const Quaternion& rhs) const {
	// Hamilton product: rhs first, then this.
	return Quaternion{
		w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
		w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
		w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
		w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
	};
}

Quaternion Quaternion::Normalized() const {
	const float lengthSquared = LengthSquared();
	if (lengthSquared <= 0.0f) {
		// A zero quaternion cannot be normalised. Not an expected state, but
		// it must not carry a NaN into the camera matrix.
		return Identity();
	}

	const float inverseLength = 1.0f / math::Sqrt(lengthSquared);
	return Quaternion{x * inverseLength, y * inverseLength, z * inverseLength,
	                  w * inverseLength};
}

Quaternion FromAxisAngle(float axisX, float axisY, float axisZ, float degrees) {
	const float lengthSquared = axisX * axisX + axisY * axisY + axisZ * axisZ;
	if (lengthSquared <= 0.0f) {
		return Quaternion::Identity();
	}

	const float inverseLength = 1.0f / math::Sqrt(lengthSquared);
	const float halfAngle = degrees * math::kDegreesToRadians * 0.5f;
	const float sine = math::Sin(halfAngle);

	return Quaternion{
		axisX * inverseLength * sine,
		axisY * inverseLength * sine,
		axisZ * inverseLength * sine,
		math::Cos(halfAngle),
	};
}

Quaternion FromOpenXR(const Quaternion& openXrOrientation) {
	// Change of basis, see the derivation in the header:
	//   x_obl = x_xr, y_obl = -z_xr, z_obl = y_xr
	// The scalar part is untouched because the mapping preserves handedness
	// (determinant +1).
	return Quaternion{
		openXrOrientation.x,
		-openXrOrientation.z,
		openXrOrientation.y,
		openXrOrientation.w,
	};
}

NiMatrix33 ToMatrix(const Quaternion& rotation) {
	const float x = rotation.x;
	const float y = rotation.y;
	const float z = rotation.z;
	const float w = rotation.w;

	NiMatrix33 result{};

	result.data[0][0] = 1.0f - 2.0f * (y * y + z * z);
	result.data[0][1] = 2.0f * (x * y - z * w);
	result.data[0][2] = 2.0f * (x * z + y * w);

	result.data[1][0] = 2.0f * (x * y + z * w);
	result.data[1][1] = 1.0f - 2.0f * (x * x + z * z);
	result.data[1][2] = 2.0f * (y * z - x * w);

	result.data[2][0] = 2.0f * (x * z - y * w);
	result.data[2][1] = 2.0f * (y * z + x * w);
	result.data[2][2] = 1.0f - 2.0f * (x * x + y * y);

	return result;
}

Quaternion FromOpenVRMatrix(const float matrix[3][4]) {
	// Rotation part only; column 3 carries the position and is left alone.
	const float m00 = matrix[0][0], m01 = matrix[0][1], m02 = matrix[0][2];
	const float m10 = matrix[1][0], m11 = matrix[1][1], m12 = matrix[1][2];
	const float m20 = matrix[2][0], m21 = matrix[2][1], m22 = matrix[2][2];

	// The inverse of ToMatrix, branching on the trace.
	//
	// The naive route through w = sqrt(1 + trace) / 2 followed by division
	// falls apart as w approaches zero - precisely at rotations near 180
	// degrees, which are perfectly ordinary when looking around in a headset.
	// So the largest component is determined first and the rest derived from
	// it, which keeps a small number out of the denominator.
	const float trace = m00 + m11 + m22;

	Quaternion result{};

	if (trace > 0.0f) {
		const float s = math::Sqrt(trace + 1.0f) * 2.0f;  // s = 4w
		result.w = 0.25f * s;
		result.x = (m21 - m12) / s;
		result.y = (m02 - m20) / s;
		result.z = (m10 - m01) / s;
	} else if (m00 > m11 && m00 > m22) {
		const float s = math::Sqrt(1.0f + m00 - m11 - m22) * 2.0f;  // s = 4x
		result.w = (m21 - m12) / s;
		result.x = 0.25f * s;
		result.y = (m01 + m10) / s;
		result.z = (m02 + m20) / s;
	} else if (m11 > m22) {
		const float s = math::Sqrt(1.0f + m11 - m00 - m22) * 2.0f;  // s = 4y
		result.w = (m02 - m20) / s;
		result.x = (m01 + m10) / s;
		result.y = 0.25f * s;
		result.z = (m12 + m21) / s;
	} else {
		const float s = math::Sqrt(1.0f + m22 - m00 - m11) * 2.0f;  // s = 4z
		result.w = (m10 - m01) / s;
		result.x = (m02 + m20) / s;
		result.y = (m12 + m21) / s;
		result.z = 0.25f * s;
	}

	// Floating point drift in the delivered matrix would otherwise come
	// through as a scaling camera matrix.
	return result.Normalized();
}

NiPoint3 PositionFromOpenVRMatrix(const float matrix[3][4]) {
	// The fourth column. Row major, so it is m[row][3] rather than m[3][row].
	return NiPoint3{matrix[0][3], matrix[1][3], matrix[2][3]};
}

NiPoint3 PositionFromOpenXR(const NiPoint3& openXrPosition) {
	return NiPoint3{openXrPosition.x, -openXrPosition.z, openXrPosition.y};
}

NiPoint3 Rotate(const Quaternion& rotation, const NiPoint3& v) {
	const float qx = rotation.x;
	const float qy = rotation.y;
	const float qz = rotation.z;
	const float qw = rotation.w;

	// t = cross(q.xyz, v) + w * v
	const float tx = qy * v.z - qz * v.y + qw * v.x;
	const float ty = qz * v.x - qx * v.z + qw * v.y;
	const float tz = qx * v.y - qy * v.x + qw * v.z;

	// v + 2 * cross(q.xyz, t)
	return NiPoint3{
		v.x + 2.0f * (qy * tz - qz * ty),
		v.y + 2.0f * (qz * tx - qx * tz),
		v.z + 2.0f * (qx * ty - qy * tx),
	};
}

}  // namespace obvr::vr
