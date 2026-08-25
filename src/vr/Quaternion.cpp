#include "vr/Quaternion.h"

#include "core/MathFns.h"

namespace obvr::vr {

Quaternion Quaternion::operator*(const Quaternion& rhs) const {
	// Hamilton-Produkt: erst rhs, dann this.
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
		// Eine Nullquaternion kann nicht normiert werden. Das ist kein
		// erwarteter Zustand, darf aber keine NaN in die Kameramatrix tragen.
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
	// Basiswechsel, siehe Herleitung im Header:
	//   x_obl = x_xr, y_obl = -z_xr, z_obl = y_xr
	// Der Skalarteil bleibt unberuehrt, weil die Abbildung die Haendigkeit
	// erhaelt (Determinante +1).
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

}  // namespace obvr::vr
