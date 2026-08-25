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

}  // namespace obvr
