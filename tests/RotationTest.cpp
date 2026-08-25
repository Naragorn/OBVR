// Checks EulerToMatrix and the matrix arithmetic it is built on.
//
// EulerToMatrix is the reference every other rotation test compares against,
// which makes it the one thing that cannot be checked by comparing it to
// something else. So it is checked against its own definition instead: the
// documented composition order, the sign convention of a single axis, and the
// properties every rotation matrix has to satisfy.
//
// A silent sign flip here would invalidate every other test in the suite while
// leaving them all green, because they would agree with a wrong reference.

#include <cmath>
#include <cstdio>

#include "core/Rotation.h"

namespace {

int g_failures = 0;

constexpr float kEpsilon = 1e-4f;

void CheckNear(float actual, float expected, const char* what) {
	if (std::fabs(actual - expected) <= kEpsilon) {
		std::printf("  ok    %s\n", what);
	} else {
		std::printf("  FAIL  %s: %.6f, expected %.6f\n", what, actual, expected);
		++g_failures;
	}
}

void CheckMatrixNear(const obvr::NiMatrix33& actual, const obvr::NiMatrix33& expected,
                     const char* what) {
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			if (std::fabs(actual.data[row][col] - expected.data[row][col]) > kEpsilon) {
				std::printf("  FAIL  %s: [%d][%d] is %.6f, expected %.6f\n",
				            what, row, col, actual.data[row][col], expected.data[row][col]);
				++g_failures;
				return;
			}
		}
	}
	std::printf("  ok    %s\n", what);
}

float Determinant(const obvr::NiMatrix33& m) {
	return m.data[0][0] * (m.data[1][1] * m.data[2][2] - m.data[1][2] * m.data[2][1]) -
	       m.data[0][1] * (m.data[1][0] * m.data[2][2] - m.data[1][2] * m.data[2][0]) +
	       m.data[0][2] * (m.data[1][0] * m.data[2][1] - m.data[1][1] * m.data[2][0]);
}

void TestMatrixArithmetic() {
	std::printf("Matrix arithmetic\n");

	const obvr::NiMatrix33 identity = obvr::NiMatrix33::Identity();
	CheckNear(identity.data[0][0], 1.0f, "Identity has 1 on the diagonal");
	CheckNear(identity.data[0][1], 0.0f, "Identity has 0 off the diagonal");

	const obvr::NiMatrix33 rotation = obvr::EulerToMatrix(11.0f, -23.0f, 47.0f);
	CheckMatrixNear(rotation * identity, rotation, "M * I equals M");
	CheckMatrixNear(identity * rotation, rotation, "I * M equals M");

	// Rotation matrices do not commute in general. If this ever passed, the
	// multiplication would be doing something other than matrix multiplication.
	const obvr::NiMatrix33 a = obvr::EulerToMatrix(30.0f, 0.0f, 0.0f);
	const obvr::NiMatrix33 b = obvr::EulerToMatrix(0.0f, 0.0f, 30.0f);
	bool differs = false;
	for (int row = 0; row < 3 && !differs; ++row) {
		for (int col = 0; col < 3 && !differs; ++col) {
			differs = std::fabs((a * b).data[row][col] - (b * a).data[row][col]) > kEpsilon;
		}
	}
	std::printf(differs ? "  ok    %s\n" : "  FAIL  %s\n",
	            "multiplication does not commute, as matrix multiplication should not");
	if (!differs) {
		++g_failures;
	}
}

void TestSingleAxisSignConvention() {
	std::printf("Sign convention of a single axis\n");

	// 90 degrees about Z, written out. This is the check that pins the
	// handedness down: with the opposite sign convention every entry off the
	// diagonal would swap, and every other test in the suite would happily
	// agree with the wrong answer.
	const obvr::NiMatrix33 z = obvr::EulerToMatrix(0.0f, 0.0f, 90.0f);
	CheckNear(z.data[0][0], 0.0f, "Z90 [0][0] is cos 90 = 0");
	CheckNear(z.data[0][1], -1.0f, "Z90 [0][1] is -sin 90 = -1");
	CheckNear(z.data[1][0], 1.0f, "Z90 [1][0] is sin 90 = 1");
	CheckNear(z.data[1][1], 0.0f, "Z90 [1][1] is cos 90 = 0");
	CheckNear(z.data[2][2], 1.0f, "Z90 leaves its own axis alone");

	const obvr::NiMatrix33 x = obvr::EulerToMatrix(90.0f, 0.0f, 0.0f);
	CheckNear(x.data[1][2], -1.0f, "X90 [1][2] is -sin 90 = -1");
	CheckNear(x.data[2][1], 1.0f, "X90 [2][1] is sin 90 = 1");
	CheckNear(x.data[0][0], 1.0f, "X90 leaves its own axis alone");

	const obvr::NiMatrix33 y = obvr::EulerToMatrix(0.0f, 90.0f, 0.0f);
	CheckNear(y.data[0][2], 1.0f, "Y90 [0][2] is +sin 90 = 1");
	CheckNear(y.data[2][0], -1.0f, "Y90 [2][0] is -sin 90 = -1");
	CheckNear(y.data[1][1], 1.0f, "Y90 leaves its own axis alone");
}

void TestCompositionOrder() {
	std::printf("Composition order Z * Y * X\n");

	// The header documents the order as Z * Y * X. Composing the three single
	// axis matrices by hand has to reproduce the combined call exactly - and
	// it would not if the order were any of the other five.
	const float px = 17.0f;
	const float py = -34.0f;
	const float pz = 61.0f;

	const obvr::NiMatrix33 combined = obvr::EulerToMatrix(px, py, pz);
	const obvr::NiMatrix33 byHand = obvr::EulerToMatrix(0.0f, 0.0f, pz) *
	                                obvr::EulerToMatrix(0.0f, py, 0.0f) *
	                                obvr::EulerToMatrix(px, 0.0f, 0.0f);

	CheckMatrixNear(combined, byHand, "EulerToMatrix(x,y,z) equals Rz * Ry * Rx");
}

void TestRotationProperties() {
	std::printf("Properties every rotation matrix has\n");

	CheckMatrixNear(obvr::EulerToMatrix(0.0f, 0.0f, 0.0f), obvr::NiMatrix33::Identity(),
	                "zero angles give the identity");

	// A rotation followed by its opposite about the same axis cancels out.
	CheckMatrixNear(obvr::EulerToMatrix(0.0f, 0.0f, 40.0f) *
	                    obvr::EulerToMatrix(0.0f, 0.0f, -40.0f),
	                obvr::NiMatrix33::Identity(),
	                "+40 and -40 about Z cancel");

	// Determinant +1 rather than -1: a rotation, not a reflection. A mirrored
	// camera is instantly recognisable in a headset but easy to miss on a
	// monitor.
	CheckNear(Determinant(obvr::EulerToMatrix(23.0f, -41.0f, 67.0f)), 1.0f,
	          "determinant is +1, so no reflection");

	// Orthonormal rows: unit length and mutually perpendicular. Anything else
	// would scale or shear the view.
	const obvr::NiMatrix33 m = obvr::EulerToMatrix(23.0f, -41.0f, 67.0f);
	for (int row = 0; row < 3; ++row) {
		const float length = std::sqrt(m.data[row][0] * m.data[row][0] +
		                               m.data[row][1] * m.data[row][1] +
		                               m.data[row][2] * m.data[row][2]);
		CheckNear(length, 1.0f, "a row has unit length");
	}

	const float dot01 = m.data[0][0] * m.data[1][0] + m.data[0][1] * m.data[1][1] +
	                    m.data[0][2] * m.data[1][2];
	CheckNear(dot01, 0.0f, "rows 0 and 1 are perpendicular");

	// 360 degrees is a full turn back to where it started.
	CheckMatrixNear(obvr::EulerToMatrix(0.0f, 360.0f, 0.0f), obvr::NiMatrix33::Identity(),
	                "360 degrees is a full turn");
}

}  // namespace

int main() {
	std::printf("OBVR rotation test\n\n");

	TestMatrixArithmetic();
	std::printf("\n");
	TestSingleAxisSignConvention();
	std::printf("\n");
	TestCompositionOrder();
	std::printf("\n");
	TestRotationProperties();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
