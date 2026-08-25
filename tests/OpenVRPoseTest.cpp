// Checks the conversion of an OpenVR pose into a quaternion.
//
// As with the rest of the quaternion code, the cross-check runs against
// EulerToMatrix, which has been verified in the running game. That ties the
// new path from OpenVR to Oblivion to established evidence instead of leaving
// it merely self-consistent.

#include <cmath>
#include <cstddef>
#include <cstdio>

#include "core/Rotation.h"
#include "vr/OpenVRTypes.h"
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

// Builds an OpenVR pose: rotation from the matrix, position freely chosen.
// OpenVR stores both in a single float[3][4].
void MakePose(const obvr::NiMatrix33& rotation, float px, float py, float pz,
              float out[3][4]) {
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			out[row][col] = rotation.data[row][col];
		}
	}
	out[0][3] = px;
	out[1][3] = py;
	out[2][3] = pz;
}

// Runs a pose through the full chain the way the game does:
// OpenVR pose -> quaternion -> basis change -> Oblivion camera matrix.
obvr::NiMatrix33 FullChain(const float pose[3][4]) {
	return obvr::vr::ToMatrix(obvr::vr::FromOpenXR(obvr::vr::FromOpenVRMatrix(pose)));
}

void TestIdentity() {
	std::printf("Rest position\n");

	float pose[3][4];
	MakePose(obvr::NiMatrix33::Identity(), 0.0f, 0.0f, 0.0f, pose);

	const Quaternion q = obvr::vr::FromOpenVRMatrix(pose);
	CheckNear(q.w, 1.0f, "identity matrix yields w = 1");
	CheckNear(q.x, 0.0f, "identity matrix yields x = 0");
	CheckNear(q.y, 0.0f, "identity matrix yields y = 0");
	CheckNear(q.z, 0.0f, "identity matrix yields z = 0");
}

void TestPositionIsIgnored() {
	std::printf("Position is discarded\n");

	// 0.0.3 delivers 3DoF. If the position part were read by accident, the
	// headset would show an offset nobody could explain.
	const obvr::NiMatrix33 rotation =
		obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.3f, 0.5f, 0.8f, 41.0f));

	float atOrigin[3][4];
	float faraway[3][4];
	MakePose(rotation, 0.0f, 0.0f, 0.0f, atOrigin);
	MakePose(rotation, 1.7f, -250.0f, 33.25f, faraway);

	CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromOpenVRMatrix(faraway)),
	                obvr::vr::ToMatrix(obvr::vr::FromOpenVRMatrix(atOrigin)),
	                "position column does not change the rotation");
}

void TestRoundTripThroughAllBranches() {
	std::printf("Round trip through all four branches\n");

	// The conversion picks a different branch depending on the trace. A
	// rotation of 180 degrees about an axis pushes the trace down to -1 and
	// forces the branch in which that axis holds the largest component.
	// Without these cases most of the function would stay untested - and that
	// is exactly where the arithmetic lives that has to take over when w gets
	// small.
	struct Case {
		float axisX, axisY, axisZ, degrees;
		const char* what;
	};

	const Case cases[] = {
		{0.2f, 0.3f, 0.9f, 15.0f, "small rotation (trace branch)"},
		{1.0f, 0.0f, 0.0f, 180.0f, "180 degrees about X (x branch)"},
		{0.0f, 1.0f, 0.0f, 180.0f, "180 degrees about Y (y branch)"},
		{0.0f, 0.0f, 1.0f, 180.0f, "180 degrees about Z (z branch)"},
		{0.4f, -0.7f, 0.2f, 179.0f, "179 degrees about a skewed axis"},
	};

	for (const Case& testCase : cases) {
		const Quaternion original = obvr::vr::FromAxisAngle(
			testCase.axisX, testCase.axisY, testCase.axisZ, testCase.degrees);
		const obvr::NiMatrix33 asMatrix = obvr::vr::ToMatrix(original);

		float pose[3][4];
		MakePose(asMatrix, 0.0f, 0.0f, 0.0f, pose);

		// Compared through the matrix rather than component by component:
		// q and -q describe the same rotation, so comparing signs would be
		// wrongly strict.
		CheckMatrixNear(obvr::vr::ToMatrix(obvr::vr::FromOpenVRMatrix(pose)), asMatrix,
		                testCase.what);
	}
}

void TestAgainstVerifiedRotation() {
	std::printf("Full chain OpenVR -> Oblivion, against EulerToMatrix\n");

	// OpenVR uses the same axis convention as OpenXR: Y is up, -Z is the view
	// direction. Looking left and right means rotating about Y there - in
	// Oblivion yaw belongs on Z.
	float yawPose[3][4];
	MakePose(obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 25.0f)),
	         0.0f, 0.0f, 0.0f, yawPose);
	CheckMatrixNear(FullChain(yawPose), obvr::EulerToMatrix(0.0f, 0.0f, 25.0f),
	                "HMD yaw about Y becomes Oblivion yaw about Z");

	// Pitch rotates about X in both systems.
	float pitchPose[3][4];
	MakePose(obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(1.0f, 0.0f, 0.0f, 25.0f)),
	         0.0f, 0.0f, 0.0f, pitchPose);
	CheckMatrixNear(FullChain(pitchPose), obvr::EulerToMatrix(25.0f, 0.0f, 0.0f),
	                "HMD pitch about X stays pitch about X");

	// Roll rotates about Z in OpenVR and about -Y in Oblivion.
	float rollPose[3][4];
	MakePose(obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.0f, 0.0f, 1.0f, 25.0f)),
	         0.0f, 0.0f, 0.0f, rollPose);
	CheckMatrixNear(FullChain(rollPose), obvr::EulerToMatrix(0.0f, -25.0f, 0.0f),
	                "HMD roll about Z becomes Oblivion roll about -Y");

	// Handedness has to survive this route as well.
	float arbitraryPose[3][4];
	MakePose(obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.4f, -0.7f, 0.2f, 63.0f)),
	         12.0f, -3.0f, 0.5f, arbitraryPose);
	const obvr::NiMatrix33 m = FullChain(arbitraryPose);
	const float determinant =
		m.data[0][0] * (m.data[1][1] * m.data[2][2] - m.data[1][2] * m.data[2][1]) -
		m.data[0][1] * (m.data[1][0] * m.data[2][2] - m.data[1][2] * m.data[2][0]) +
		m.data[0][2] * (m.data[1][0] * m.data[2][1] - m.data[1][1] * m.data[2][0]);
	CheckNear(determinant, 1.0f, "determinant stays +1, no mirroring");
}

void TestNormalization() {
	std::printf("Normalisation\n");

	// SteamVR delivers poses with floating point error. A slightly scaled
	// matrix must not turn into a scaling camera matrix.
	obvr::NiMatrix33 slightlyOff =
		obvr::vr::ToMatrix(obvr::vr::FromAxisAngle(0.1f, 0.9f, 0.3f, 33.0f));
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			slightlyOff.data[row][col] *= 1.001f;
		}
	}

	float pose[3][4];
	MakePose(slightlyOff, 0.0f, 0.0f, 0.0f, pose);
	CheckNear(obvr::vr::FromOpenVRMatrix(pose).LengthSquared(), 1.0f,
	          "result is normalised");
}

void CheckEqual(std::size_t actual, std::size_t expected, const char* what) {
	if (actual == expected) {
		std::printf("  ok    %s\n", what);
	} else {
		std::printf("  FAIL  %s: %zu, expected %zu\n", what, actual, expected);
		++g_failures;
	}
}

void TestCompositorLayout() {
	std::printf("The compositor interface, as declared\n");

	namespace openvr = obvr::vr::openvr;

	// Function pointer indices, checked in units of one pointer rather than
	// in bytes: the DLL that ships is 32 bit and this test binary is not, so
	// a byte count would be right for one of them and wrong for the other.
	//
	// These are worth pinning because getting one wrong does not produce a
	// wrong picture, it produces a call into a different function with a
	// different number of arguments. On a 32-bit stdcall stack that corrupts
	// the stack and crashes somewhere unrelated.
	CheckEqual(offsetof(openvr::IVRCompositorFnTable, SetTrackingSpace), 0,
	           "SetTrackingSpace is the first entry");
	CheckEqual(offsetof(openvr::IVRCompositorFnTable, WaitGetPoses), 2 * sizeof(void*),
	           "WaitGetPoses is at index 2");

	// The one that a stale listing gets wrong. GetSubmitTexture sits between
	// GetLastPoseForTrackedDeviceIndex and Submit, and it is missing from
	// older documentation - leaving Submit one slot early, on
	// SubmitWithArrayIndex, which takes an extra argument.
	CheckEqual(offsetof(openvr::IVRCompositorFnTable, Submit), 6 * sizeof(void*),
	           "Submit is at index 6, not 5");

	// Two structs that cross the boundary by value.
	CheckEqual(sizeof(openvr::Texture), sizeof(void*) + 2 * sizeof(int),
	           "Texture_t packs without padding");
	CheckEqual(sizeof(openvr::VRTextureBounds), 4 * sizeof(float),
	           "VRTextureBounds_t is four floats");
	CheckEqual(offsetof(openvr::VRTextureBounds, vMax), 3 * sizeof(float),
	           "the bounds are in the order uMin, vMin, uMax, vMax");

	// Scene and Background are mutually exclusive, and OBVR needs both at
	// different times. Pinning the pair here keeps a later edit from
	// collapsing them into one value.
	CheckEqual(static_cast<std::size_t>(openvr::kApplicationScene), 1,
	           "VRApplication_Scene is 1");
	CheckEqual(static_cast<std::size_t>(openvr::kApplicationBackground), 3,
	           "VRApplication_Background is 3, and a different thing");
	CheckEqual(static_cast<std::size_t>(openvr::kCompositorErrorIsNotSceneApplication), 103,
	           "the error a background application gets from Submit is 103");
}

}  // namespace

int main() {
	std::printf("OBVR OpenVR pose test\n\n");

	TestIdentity();
	std::printf("\n");
	TestPositionIsIgnored();
	std::printf("\n");
	TestRoundTripThroughAllBranches();
	std::printf("\n");
	TestAgainstVerifiedRotation();
	std::printf("\n");
	TestNormalization();
	std::printf("\n");
	TestCompositorLayout();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
