// Checks the cross-check on Oblivion's own render frustum.
//
// Reading the frustum needs a running game and is not tested here. What is
// tested is the question asked of what comes back, and that question exists
// because of how these addresses fail.
//
// The scene graph pointer, the camera offset and the frustum offset all came
// from documentation of somebody else's reverse engineering. A wrong one does
// not crash and does not complain: it hands back four floats that are some
// other object's contents and look like numbers. Comparing the angles they
// imply against angles measured a completely different way - off the
// projection matrix the device already reported - is what turns "these
// addresses are probably right" into a question with an answer.
//
// No Windows API, so this builds and runs on Linux as well.

#include <cstdio>

#include "game/GameCamera.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

// Oblivion as measured on this machine: 75 degrees across a 16:9 frame, so
// tan(37.5) across and 0.5625 of that down.
constexpr float kTanAcross = 0.767327f;
constexpr float kTanDown = 0.431621f;

// A frustum describing exactly that, at a near plane of 10 - which is
// Oblivion's own fNearDistance.
obvr::game::NiFrustum MeasuredFrustum() {
	obvr::game::NiFrustum frustum{};
	frustum.n = 10.0f;
	frustum.f = 10000.0f;
	frustum.l = -10.0f * kTanAcross;
	frustum.r = 10.0f * kTanAcross;
	frustum.t = 10.0f * kTanDown;
	frustum.b = -10.0f * kTanDown;
	frustum.o = false;
	return frustum;
}

void TestAgreement() {
	std::printf("A frustum that matches the projection matrix\n");

	using obvr::game::FrustumLooksRight;

	Check(FrustumLooksRight(MeasuredFrustum(), kTanAcross, kTanDown),
	      "the frustum Oblivion should have is recognised");

	// The near plane is what the other four are divided by, so the same angles
	// at a different near distance have to pass just as well. This is the
	// property that makes the check about angles rather than about distances.
	obvr::game::NiFrustum far = MeasuredFrustum();
	far.l *= 7.0f;
	far.r *= 7.0f;
	far.t *= 7.0f;
	far.b *= 7.0f;
	far.n *= 7.0f;
	Check(FrustumLooksRight(far, kTanAcross, kTanDown),
	      "and so is the same view at a different near plane");

	// Signs are not settled here and do not need to be: a frustum two units
	// wide spans the same angle whichever way its edges are signed.
	obvr::game::NiFrustum flipped = MeasuredFrustum();
	flipped.t = -flipped.t;
	flipped.b = -flipped.b;
	Check(FrustumLooksRight(flipped, kTanAcross, kTanDown),
	      "and the vertical signs the other way round, which is not settled");
}

void TestRejection() {
	std::printf("What a wrong address hands back\n");

	using obvr::game::FrustumLooksRight;

	// All zeroes - an object that was never a frustum, or one not built yet.
	obvr::game::NiFrustum zero{};
	Check(!FrustumLooksRight(zero, kTanAcross, kTanDown), "all zeroes is refused");

	// A near plane past the far plane, which no frustum has and stray bytes
	// often do.
	obvr::game::NiFrustum inverted = MeasuredFrustum();
	inverted.n = 20000.0f;
	Check(!FrustumLooksRight(inverted, kTanAcross, kTanDown),
	      "a near plane beyond the far plane is refused");

	// The case that matters most: plausible-looking numbers describing the
	// wrong view. Reading some other object would give this, and it is the
	// failure that would otherwise be believed.
	obvr::game::NiFrustum wrongAngle = MeasuredFrustum();
	wrongAngle.l *= 2.0f;
	wrongAngle.r *= 2.0f;
	Check(!FrustumLooksRight(wrongAngle, kTanAcross, kTanDown),
	      "a well-formed frustum describing twice the width is refused");

	obvr::game::NiFrustum wrongHeight = MeasuredFrustum();
	wrongHeight.t *= 0.5f;
	wrongHeight.b *= 0.5f;
	Check(!FrustumLooksRight(wrongHeight, kTanAcross, kTanDown),
	      "and one describing half the height, so both axes are checked");

	// Within tolerance, though. The two readings round differently and a few
	// per cent must not be called a mismatch.
	obvr::game::NiFrustum slightly = MeasuredFrustum();
	slightly.l *= 1.03f;
	slightly.r *= 1.03f;
	Check(FrustumLooksRight(slightly, kTanAcross, kTanDown),
	      "while three per cent is rounding, not disagreement");
}

void TestLayout() {
	std::printf("The structure itself\n");

	// 0x1C rather than 0x1B, because the flag is padded out to the alignment
	// of the floats before it. This is not pedantry: xOBSE puts the frustum at
	// 0xEC and the viewport at 0x110, and 0xEC + 0x1C + two more floats is
	// exactly 0x110. A structure of the wrong size would break that agreement,
	// and the agreement is most of why these offsets are believed at all.
	Check(sizeof(obvr::game::NiFrustum) == 0x1C, "NiFrustum is 28 bytes");
	Check(obvr::game::kNiCameraFrustumOffset + 0x1C + 2 * sizeof(float) == 0x110,
	      "and frustum, minNearPlaneDist and maxFarNearRatio reach the viewport at 0x110");
}

}  // namespace

int main() {
	std::printf("OBVR game camera test\n\n");

	TestAgreement();
	std::printf("\n");
	TestRejection();
	std::printf("\n");
	TestLayout();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
