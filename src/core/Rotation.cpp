#include "core/Rotation.h"

#include "core/MathFns.h"

namespace obvr {

NiMatrix33 EulerToMatrix(float degreesX, float degreesY, float degreesZ) {
	const float a = degreesX * math::kDegreesToRadians;
	const float b = degreesY * math::kDegreesToRadians;
	const float c = degreesZ * math::kDegreesToRadians;

	const float sa = math::Sin(a);
	const float ca = math::Cos(a);
	const float sb = math::Sin(b);
	const float cb = math::Cos(b);
	const float sc = math::Sin(c);
	const float cc = math::Cos(c);

	NiMatrix33 rx = NiMatrix33::Identity();
	rx.data[1][1] = ca;
	rx.data[1][2] = -sa;
	rx.data[2][1] = sa;
	rx.data[2][2] = ca;

	NiMatrix33 ry = NiMatrix33::Identity();
	ry.data[0][0] = cb;
	ry.data[0][2] = sb;
	ry.data[2][0] = -sb;
	ry.data[2][2] = cb;

	NiMatrix33 rz = NiMatrix33::Identity();
	rz.data[0][0] = cc;
	rz.data[0][1] = -sc;
	rz.data[1][0] = sc;
	rz.data[1][1] = cc;

	return rz * ry * rx;
}

bool HeadingOf(const NiMatrix33& rotation, Heading& out) {
	const float x = rotation.data[0][0];
	const float y = rotation.data[1][0];

	const float lengthSquared = x * x + y * y;

	// Only reachable with a camera rolled close to ninety degrees. Vanilla
	// Oblivion has no such camera, but a mod or a scripted sequence might.
	if (lengthSquared < 1.0e-6f) {
		return false;
	}

	const float inverseLength = 1.0f / math::Sqrt(lengthSquared);
	out = Heading{x * inverseLength, y * inverseLength};
	return true;
}

NiMatrix33 RotationFromHeading(const Heading& heading) {
	NiMatrix33 result = NiMatrix33::Identity();
	result.data[0][0] = heading.cosine;
	result.data[0][1] = -heading.sine;
	result.data[1][0] = heading.sine;
	result.data[1][1] = heading.cosine;
	return result;
}

float SinPitchOf(const NiMatrix33& rotation) {
	// Column 1 is where the camera's forward axis points, and its vertical
	// component is the sine of the tilt. Clamped because a matrix that has
	// drifted slightly out of orthonormality can push it past one, and the
	// caller multiplies it by a distance.
	const float value = rotation.data[2][1];
	if (value > 1.0f) {
		return 1.0f;
	}
	if (value < -1.0f) {
		return -1.0f;
	}
	return value;
}

}  // namespace obvr
