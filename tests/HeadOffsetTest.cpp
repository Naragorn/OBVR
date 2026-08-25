// Checks the arithmetic that turns a tracked head position into a camera
// offset.
//
// This is the whole of positional tracking that can be checked without a
// headset, and it is where the mistakes live. A wrong sign in the change of
// basis makes leaning forward pull the camera backwards; a missing rotation by
// the reference makes leaning work only if the seated zero in SteamVR happens
// to face the same way as the player; a smoothing factor above 1 makes the
// camera oscillate around the head instead of settling on it.
//
// Pure arithmetic, so this runs on Linux as well.

#include <cstdio>

#include "vr/HeadOffset.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float actual, float expected) {
	const float difference = actual - expected;
	return difference <= 0.001f && difference >= -0.001f;
}

void CheckPoint(const obvr::NiPoint3& actual, float x, float y, float z, const char* what) {
	const bool ok = Near(actual.x, x) && Near(actual.y, y) && Near(actual.z, z);
	if (!ok) {
		std::printf("        expected (%.3f, %.3f, %.3f), got (%.3f, %.3f, %.3f)\n",
		            static_cast<double>(x), static_cast<double>(y), static_cast<double>(z),
		            static_cast<double>(actual.x), static_cast<double>(actual.y),
		            static_cast<double>(actual.z));
	}
	Check(ok, what);
}

// A round 70 rather than the real 69.99125, so that every expected value below
// reads as "how many units is a metre" and a wrong scale is visible at a
// glance.
constexpr float kUnitsPerMetre = 70.0f;

void TestOffsetAxes() {
	std::printf("Which way the head moves the camera\n");

	using obvr::NiPoint3;
	using obvr::vr::OffsetFromPose;

	const obvr::vr::Quaternion noRotation = obvr::vr::Quaternion::Identity();
	const NiPoint3 origin{0.0f, 0.0f, 0.0f};

	// OpenXR has -Z forward, Oblivion has +Y forward.
	CheckPoint(OffsetFromPose(noRotation, NiPoint3{0.0f, 0.0f, -1.0f}, origin, kUnitsPerMetre),
	           0.0f, 70.0f, 0.0f, "leaning a metre forward moves the camera forward");

	CheckPoint(OffsetFromPose(noRotation, NiPoint3{0.0f, 0.0f, 1.0f}, origin, kUnitsPerMetre),
	           0.0f, -70.0f, 0.0f, "leaning back moves it back");

	// OpenXR has +Y up, Oblivion has +Z up.
	CheckPoint(OffsetFromPose(noRotation, NiPoint3{0.0f, 1.0f, 0.0f}, origin, kUnitsPerMetre),
	           0.0f, 0.0f, 70.0f, "standing up raises it");

	// Right stays right in both.
	CheckPoint(OffsetFromPose(noRotation, NiPoint3{1.0f, 0.0f, 0.0f}, origin, kUnitsPerMetre),
	           70.0f, 0.0f, 0.0f, "leaning right moves it right");
}

void TestReferencePosition() {
	std::printf("The reference position\n");

	using obvr::NiPoint3;
	using obvr::vr::OffsetFromPose;

	const obvr::vr::Quaternion noRotation = obvr::vr::Quaternion::Identity();

	// The seated origin sits on the floor, so a head reads as over a metre up
	// even while sitting perfectly still. Without the reference being taken
	// off, that height alone would displace the camera by some 85 units for
	// as long as the game runs.
	const NiPoint3 head{0.2f, 1.25f, -0.1f};

	CheckPoint(OffsetFromPose(noRotation, head, head, kUnitsPerMetre), 0.0f, 0.0f, 0.0f,
	           "sitting where the reference was taken means no offset at all");

	const NiPoint3 leaned{0.2f, 1.25f, -0.6f};
	CheckPoint(OffsetFromPose(noRotation, leaned, head, kUnitsPerMetre), 0.0f, 35.0f, 0.0f,
	           "only the difference against the reference counts");
}

void TestReferenceOrientation() {
	std::printf("The reference orientation\n");

	using obvr::NiPoint3;
	using obvr::vr::OffsetFromPose;

	// The user recentered while facing 90 degrees to the left of the seated
	// zero - a rotation about the OpenXR up axis, which is Y.
	const obvr::vr::Quaternion facingLeft = obvr::vr::FromAxisAngle(0.0f, 1.0f, 0.0f, 90.0f);
	const NiPoint3 origin{0.0f, 0.0f, 0.0f};

	// Now they lean along the room's -Z. Relative to the direction they were
	// facing, that is a lean to their right rather than forward. Leaving this
	// rotation out is invisible whenever the seated zero happens to agree with
	// the player's facing, which is exactly why it needs a test rather than a
	// play session.
	CheckPoint(OffsetFromPose(facingLeft, NiPoint3{0.0f, 0.0f, -1.0f}, origin, kUnitsPerMetre),
	           70.0f, 0.0f, 0.0f, "a lean is read in the frame the user recentered in");

	// Up is up regardless of which way anybody faces.
	CheckPoint(OffsetFromPose(facingLeft, NiPoint3{0.0f, 1.0f, 0.0f}, origin, kUnitsPerMetre),
	           0.0f, 0.0f, 70.0f, "standing up is unaffected by the facing");
}

void TestClamp() {
	std::printf("The lean limit\n");

	using obvr::NiPoint3;
	using obvr::vr::ClampOffset;

	CheckPoint(ClampOffset(NiPoint3{0.0f, 10.0f, 0.0f}, 40.0f), 0.0f, 10.0f, 0.0f,
	           "a small offset passes untouched");

	CheckPoint(ClampOffset(NiPoint3{0.0f, 40.0f, 0.0f}, 40.0f), 0.0f, 40.0f, 0.0f,
	           "an offset exactly at the limit passes untouched");

	CheckPoint(ClampOffset(NiPoint3{0.0f, 100.0f, 0.0f}, 40.0f), 0.0f, 40.0f, 0.0f,
	           "a longer one is cut to the limit");

	// Direction has to survive the cut, otherwise running into the limit would
	// swing the camera sideways rather than simply stopping it. 3-4-5 triangle
	// scaled up: length 500, cut to 50, so a tenth of each component.
	CheckPoint(ClampOffset(NiPoint3{300.0f, 400.0f, 0.0f}, 50.0f), 30.0f, 40.0f, 0.0f,
	           "the direction survives the cut");

	CheckPoint(ClampOffset(NiPoint3{0.0f, 1000.0f, 0.0f}, 0.0f), 0.0f, 1000.0f, 0.0f,
	           "a limit of 0 means no limit");
}

void TestApproach() {
	std::printf("Smoothing\n");

	using obvr::NiPoint3;
	using obvr::vr::Approach;

	const NiPoint3 here{0.0f, 0.0f, 0.0f};
	const NiPoint3 there{0.0f, 100.0f, 0.0f};
	constexpr float kFrame = 1.0f / 60.0f;

	// Speed 15 per second at 60 fps is a quarter of what is left.
	CheckPoint(Approach(here, there, 15.0f, kFrame), 0.0f, 25.0f, 0.0f,
	           "one frame covers speed times delta of the distance");

	// The same setting has to cover twice as much in a frame twice as long.
	// This is the whole reason the speed is per second rather than per frame,
	// and it is what UEVR does with t = lerp_speed * delta.
	CheckPoint(Approach(here, there, 15.0f, kFrame * 2.0f), 0.0f, 50.0f, 0.0f,
	           "a frame twice as long covers twice as much");

	// It arrives rather than overshooting, however long the frame.
	CheckPoint(Approach(here, there, 15.0f, 10.0f), 0.0f, 100.0f, 0.0f,
	           "an absurdly long frame arrives instead of overshooting");

	CheckPoint(Approach(here, there, 0.0f, kFrame), 0.0f, 100.0f, 0.0f,
	           "a speed of 0 means no smoothing, not a frozen camera");
	CheckPoint(Approach(here, there, 15.0f, 0.0f), 0.0f, 100.0f, 0.0f,
	           "no frame time means no smoothing either");
	CheckPoint(Approach(here, there, 15.0f, -1.0f), 0.0f, 100.0f, 0.0f,
	           "and neither does a negative one");

	// Repeated application has to converge on the target rather than circle it.
	NiPoint3 current = here;
	for (int frame = 0; frame < 120; ++frame) {
		current = Approach(current, there, 15.0f, kFrame);
	}
	Check(current.y > 99.9f && current.y <= 100.0f,
	      "two seconds of frames arrive at the target without passing it");
}

}  // namespace

int main() {
	std::printf("OBVR head offset test\n\n");

	TestOffsetAxes();
	std::printf("\n");
	TestReferencePosition();
	std::printf("\n");
	TestReferenceOrientation();
	std::printf("\n");
	TestClamp();
	std::printf("\n");
	TestApproach();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
