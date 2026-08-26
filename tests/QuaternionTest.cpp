// Checks the quaternion maths and the change of basis from OpenXR to
// Oblivion.
//
// The most important test is the cross-check against EulerToMatrix: that
// function was verified in the running game (pitch and roll visibly correct).
// If the quaternion route produces the same matrices, it is tied to
// established evidence rather than merely self-consistent.

#include <cmath>
#include <cstdio>

#include "core/Rotation.h"
#include "vr/Quaternion.h"

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

using obvr::vr::Quaternion;

void TestBasics() {
	std::printf("Basic arithmetic\n");

	const Quaternion identity = Quaternion::Identity();
	CheckMatrixNear(obvr::vr::ToMatrix(identity), obvr::NiMatrix33::Identity(),
	                "identity yields the unit matrix");

	// A rotation composed with its own inverse has to cancel out.
	const Quaternion rotation = obvr::vr::FromAxisAngle(0.3f, 0.5f, 0.8f, 47.0f);
	const Quaternion undone = rotation.Conjugate() * rotation;
	CheckNear(undone.w, 1.0f, "q^-1 * q has w = 1");
	CheckNear(undone.x, 0.0f, "q^-1 * q has x = 0");
	CheckNear(undone.y, 0.0f, "q^-1 * q has y = 0");
	CheckNear(undone.z, 0.0f, "q^-1 * q has z = 0");

	// FromAxisAngle has to normalise the axis itself, otherwise the result
	// would depend on the length of the vector passed in.
	const Quaternion fromLongAxis = obvr::vr::FromAxisAngle(0.0f, 0.0f, 5.0f, 30.0f);
	const Quaternion fromUnitAxis = obvr::vr::FromAxisAngle(0.0f, 0.0f, 1.0f, 30.0f);
	CheckNear(fromLongAxis.z, fromUnitAxis.z, "axis length does not affect the result");

	CheckNear(obvr::vr::FromAxisAngle(1.0f, 2.0f, 3.0f, 90.0f).LengthSquared(), 1.0f,
	          "result is normalised");
}

// The decisive tie to the reference confirmed in the game.
void TestAgainstVerifiedRotation() {
	std::printf("Cross-check against EulerToMatrix (verified in the game)\n");

	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(1.0f, 0.0f, 0.0f, 20.0f)),
	                obvr::EulerToMatrix(20.0f, 0.0f, 0.0f),
	                "rotation about X matches EulerToMatrix(20,0,0) = pitch");

	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 20.0f)),
	                obvr::EulerToMatrix(0.0f, 20.0f, 0.0f),
	                "rotation about Y matches EulerToMatrix(0,20,0) = roll");

	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.0f, 0.0f, 1.0f, 20.0f)),
	                obvr::EulerToMatrix(0.0f, 0.0f, 20.0f),
	                "rotation about Z matches EulerToMatrix(0,0,20) = yaw");
}

void TestOpenXrAxisSwap() {
	std::printf("Change of basis, OpenXR to Oblivion\n");

	// OpenXR: Y is up. Looking left and right means rotating about Y there.
	// In Oblivion Z is up, so yaw belongs on Z.
	const Quaternion xrYaw = obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 25.0f);
	const Quaternion oblYaw = obvr::vr::FromOpenXR(xrYaw);
	CheckMatrixNear(obvr::vr::ToMatrix(oblYaw), obvr::EulerToMatrix(0.0f, 0.0f, 25.0f),
	                "OpenXR yaw about Y becomes Oblivion yaw about Z");

	// OpenXR: X is right, pitch rotates about X. Same in Oblivion.
	const Quaternion xrPitch = obvr::vr::FromAxisAngle(1.0f, 0.0f, 0.0f, 25.0f);
	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromOpenXR(xrPitch)),
	                obvr::EulerToMatrix(25.0f, 0.0f, 0.0f),
	                "OpenXR pitch about X stays pitch about X");

	// OpenXR: -Z is the view direction, so roll rotates about Z. In Oblivion
	// the view direction is +Y, and because of the inversion that becomes -Y.
	const Quaternion xrRoll = obvr::vr::FromAxisAngle(0.0f, 0.0f, 1.0f, 25.0f);
	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromOpenXR(xrRoll)),
	                obvr::EulerToMatrix(0.0f, -25.0f, 0.0f),
	                "OpenXR roll about Z becomes Oblivion roll about -Y");

	// The change of basis must not flip handedness; a mirrored camera would
	// be immediately recognisable as wrong in the headset.
	const Quaternion arbitrary = obvr::vr::FromAxisAngle(0.4f, -0.7f, 0.2f, 63.0f);
	const obvr::NiMatrix33 m = obvr::vr::ToMatrix(obvr::vr::FromOpenXR(arbitrary));
	const float determinant =
		m.data[0][0] * (m.data[1][1] * m.data[2][2] - m.data[1][2] * m.data[2][1]) -
		m.data[0][1] * (m.data[1][0] * m.data[2][2] - m.data[1][2] * m.data[2][0]) +
		m.data[0][2] * (m.data[1][0] * m.data[2][1] - m.data[1][1] * m.data[2][0]);
	CheckNear(determinant, 1.0f, "determinant stays +1, no mirroring");
}

void TestRecenter() {
	std::printf("Recenter\n");

	// Recentering remembers the current orientation as the new zero. Right
	// after that the relative rotation has to be the identity.
	const Quaternion reference = obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 130.0f);
	const Quaternion relative = reference.Conjugate() * reference;
	CheckMatrixNear(obvr::vr::ToMatrix(relative.Normalized()), obvr::NiMatrix33::Identity(),
	                "immediately after recentering the rotation is neutral");

	// If the head then turns another 30 degrees, exactly those 30 degrees
	// have to remain - regardless of where the reference stood.
	const Quaternion current =
		obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 160.0f);
	const Quaternion delta = (reference.Conjugate() * current).Normalized();
	CheckMatrixNear(obvr::vr::ToMatrix(delta),
	                obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 30.0f)),
	                "30 degrees after recentering yield 30 degrees of camera rotation");
}


void TestYawOnly() {
	std::printf("Recentring keeps the horizon level\n");

	using obvr::vr::FromAxisAngle;
	using obvr::vr::Rotate;
	using obvr::vr::YawOnly;

	// A head turned 40 degrees to the left and tilted 25 down and 15 sideways.
	// What the key is pressed for is the 40; the other two are posture, and
	// carrying them into the reference tilts the world for as long as it
	// stands.
	const obvr::vr::Quaternion tilted =
		(FromAxisAngle(0.0f, 1.0f, 0.0f, 40.0f) * FromAxisAngle(1.0f, 0.0f, 0.0f, -25.0f) *
		 FromAxisAngle(0.0f, 0.0f, 1.0f, 15.0f))
			.Normalized();

	const obvr::vr::Quaternion flat = YawOnly(tilted);

	// The result must be a rotation about the vertical axis only, which for a
	// quaternion means the x and z parts are gone.
	CheckNear(flat.x, 0.0f, "no pitch survives");
	CheckNear(flat.z, 0.0f, "and no roll");

	// Up stays up. This is the property the whole function exists for, and it
	// is the one a person feels: a reference that tilts the horizon never
	// stops being wrong, because the inner ear keeps insisting.
	const obvr::NiPoint3 up = Rotate(flat, obvr::NiPoint3{0.0f, 1.0f, 0.0f});
	CheckNear(up.x, 0.0f, "the vertical axis is untouched");
	CheckNear(up.y, 1.0f, "up is still up");
	CheckNear(up.z, 0.0f, "in every direction");

	// And the heading is the one that was there. Forward, flattened, has to
	// point the same way it did before the tilt was removed.
	const obvr::NiPoint3 wasFacing = Rotate(tilted, obvr::NiPoint3{0.0f, 0.0f, -1.0f});
	const obvr::NiPoint3 nowFacing = Rotate(flat, obvr::NiPoint3{0.0f, 0.0f, -1.0f});
	const float wasLength = std::sqrt(wasFacing.x * wasFacing.x + wasFacing.z * wasFacing.z);
	CheckNear(nowFacing.x, wasFacing.x / wasLength, "the heading is kept across");
	CheckNear(nowFacing.z, wasFacing.z / wasLength, "and along");

	// Straight up has no heading to keep, and inventing one would swing the
	// world by whatever the arithmetic happened to produce.
	const obvr::vr::Quaternion straightUp = FromAxisAngle(1.0f, 0.0f, 0.0f, 90.0f);
	const obvr::vr::Quaternion fromUp = YawOnly(straightUp);
	CheckNear(fromUp.w, 1.0f, "looking straight up yields no rotation rather than a guess");
}

}  // namespace

int main() {
	std::printf("OBVR quaternion test\n\n");

	TestBasics();
	std::printf("\n");
	TestAgainstVerifiedRotation();
	std::printf("\n");
	TestOpenXrAxisSwap();
	std::printf("\n");
	TestRecenter();
	std::printf("\n");
	TestYawOnly();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
