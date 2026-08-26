// Checks the fallback path of the OpenVR backend on a machine without
// SteamVR.
//
// This is not a side issue: the vast majority of users will start OBVR at
// least once without SteamVR running. If OBVR crashed there or kept Oblivion
// from loading, the mod would be useless - for people who have not even been
// in VR yet.
//
// The test also compiles the whole backend for x86, which is what arms the
// static_asserts in OpenVRTypes.h.

#include <cmath>
#include <cstddef>
#include <cstdio>

#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

// Builds a pose looking in a direction, with a deliberate roll about it.
//
// Column 2 is backward, so forward is its negative; columns 0 and 1 are right
// and up, rotated about the view direction by the roll angle.
obvr::vr::openvr::HmdMatrix34 PoseLookingAt(float pitchRadians, float yawRadians,
                                            float rollRadians) {
	const float cp = std::cos(pitchRadians);
	const float sp = std::sin(pitchRadians);
	const float cy = std::cos(yawRadians);
	const float sy = std::sin(yawRadians);
	const float cr = std::cos(rollRadians);
	const float sr = std::sin(rollRadians);

	// Yaw about world up, then pitch about the new right, then roll about the
	// view direction. Written out so the test does not depend on the code it
	// is checking.
	const float fx = -sy * cp;
	const float fy = sp;
	const float fz = -cy * cp;

	// Right and up before any roll.
	const float r0x = cy, r0y = 0.0f, r0z = -sy;
	const float u0x = sy * sp, u0y = cp, u0z = cy * sp;

	obvr::vr::openvr::HmdMatrix34 pose{};
	pose.m[0][0] = r0x * cr + u0x * sr;
	pose.m[1][0] = r0y * cr + u0y * sr;
	pose.m[2][0] = r0z * cr + u0z * sr;

	pose.m[0][1] = -r0x * sr + u0x * cr;
	pose.m[1][1] = -r0y * sr + u0y * cr;
	pose.m[2][1] = -r0z * sr + u0z * cr;

	pose.m[0][2] = -fx;
	pose.m[1][2] = -fy;
	pose.m[2][2] = -fz;

	pose.m[0][3] = 1.0f;
	pose.m[1][3] = 2.0f;
	pose.m[2][3] = 3.0f;
	return pose;
}

void CheckNear(float actual, float expected, const char* what) {
	const bool ok = std::fabs(actual - expected) < 0.002f;
	std::printf(ok ? "  ok    %s\n" : "  FAIL  %s (%.4f, expected %.4f)\n", what,
	            static_cast<double>(actual), static_cast<double>(expected));
	if (!ok) {
		++g_failures;
	}
}

void TestLevelPoseKeepsPitch() {
	std::printf("Levelling an anchor pose takes out roll and leaves pitch\n");

	using obvr::vr::LevelPose;

	// Looking 30 degrees down, turned 40 degrees, head tilted 25 degrees.
	const float pitch = -0.5236f;
	const float yaw = 0.6981f;
	const float roll = 0.4363f;

	obvr::vr::openvr::HmdMatrix34 pose = PoseLookingAt(pitch, yaw, roll);
	const obvr::vr::openvr::HmdMatrix34 before = pose;
	LevelPose(pose);

	// The view direction is untouched, which is what keeping pitch and yaw
	// means. This is the check that would have failed before: levelling used
	// to flatten the third column too, and a menu anchored to it hung at eye
	// level however the wearer was looking.
	CheckNear(pose.m[0][2], before.m[0][2], "the view direction keeps its x");
	CheckNear(pose.m[1][2], before.m[1][2], "and its y, which is the pitch");
	CheckNear(pose.m[2][2], before.m[2][2], "and its z");

	// Right is horizontal, which is what having no roll means: the horizon in
	// the picture runs level however the head is tilted.
	CheckNear(pose.m[1][0], 0.0f, "right has no vertical component, so nothing is tilted");

	// Still a rotation: the three columns unit length and mutually
	// perpendicular. A matrix that is not would shear the picture rather than
	// turn it, and the failure would look like a rendering fault.
	const float rr = pose.m[0][0] * pose.m[0][0] + pose.m[1][0] * pose.m[1][0] +
	                 pose.m[2][0] * pose.m[2][0];
	const float uu = pose.m[0][1] * pose.m[0][1] + pose.m[1][1] * pose.m[1][1] +
	                 pose.m[2][1] * pose.m[2][1];
	CheckNear(rr, 1.0f, "right is a unit vector");
	CheckNear(uu, 1.0f, "up is a unit vector");

	const float ru = pose.m[0][0] * pose.m[0][1] + pose.m[1][0] * pose.m[1][1] +
	                 pose.m[2][0] * pose.m[2][1];
	const float rb = pose.m[0][0] * pose.m[0][2] + pose.m[1][0] * pose.m[1][2] +
	                 pose.m[2][0] * pose.m[2][2];
	const float ub = pose.m[0][1] * pose.m[0][2] + pose.m[1][1] * pose.m[1][2] +
	                 pose.m[2][1] * pose.m[2][2];
	CheckNear(ru, 0.0f, "right and up are perpendicular");
	CheckNear(rb, 0.0f, "right and backward are perpendicular");
	CheckNear(ub, 0.0f, "up and backward are perpendicular");

	// Up points upwards. With pitch kept it is not (0,1,0) any more, but a
	// wearer looking 30 degrees down still has up above them.
	obvr::vr::openvr::HmdMatrix34 level = PoseLookingAt(pitch, yaw, roll);
	LevelPose(level);
	std::printf(level.m[1][1] > 0.0f ? "  ok    up still points upwards\n"
	                                 : "  FAIL  up still points upwards\n");
	if (!(level.m[1][1] > 0.0f)) {
		++g_failures;
	}

	// The position is not an orientation and is left alone.
	CheckNear(pose.m[0][3], 1.0f, "the position keeps its x");
	CheckNear(pose.m[1][3], 2.0f, "and its y");
	CheckNear(pose.m[2][3], 3.0f, "and its z");
}

void TestLevelPoseLookingStraightDown() {
	std::printf("Levelling a pose that looks straight down\n");

	using obvr::vr::LevelPose;

	// Backward is parallel to world up, so there is no direction left for
	// right to point in. Every answer is as good as any other, so the pose is
	// left as it is rather than swung by whatever the arithmetic produced.
	obvr::vr::openvr::HmdMatrix34 pose{};
	pose.m[0][0] = 1.0f;
	pose.m[1][1] = 0.0f;
	pose.m[2][1] = -1.0f;
	pose.m[1][2] = 1.0f;

	const obvr::vr::openvr::HmdMatrix34 before = pose;
	LevelPose(pose);

	bool unchanged = true;
	for (int row = 0; row < 3; ++row) {
		for (int column = 0; column < 4; ++column) {
			if (pose.m[row][column] != before.m[row][column]) {
				unchanged = false;
			}
		}
	}
	Check(unchanged, "straight down leaves the pose exactly as it was");
}

}  // namespace

int main() {
	std::printf("OBVR OpenVR backend, fallback without SteamVR\n\n");

	TestLevelPoseKeepsPitch();
	std::printf("\n");
	TestLevelPoseLookingStraightDown();
	std::printf("\n");
	std::printf("Struct layout\n");
	Check(sizeof(obvr::vr::openvr::HmdMatrix34) == 48, "HmdMatrix34 is 48 bytes");
	Check(sizeof(obvr::vr::openvr::TrackedDevicePose) == 80, "TrackedDevicePose is 80 bytes");

	// The typed entry has to sit exactly behind the twelve unused ones. The
	// check is written against sizeof(void*) rather than a fixed 48 because
	// the tests build natively, which is 64 bit on this machine, while
	// OBVR.dll itself is always x86. What matters is the index, not the byte
	// offset - and the index is what a wrong listing of the interface would
	// get wrong.
	Check(offsetof(obvr::vr::openvr::IVRSystemFnTable, GetDeviceToAbsoluteTrackingPose) ==
	          12 * sizeof(void*),
	      "GetDeviceToAbsoluteTrackingPose sits at index 12");

	std::printf("\nStarting without SteamVR\n");
	obvr::vr::OpenVRBackend backend;

	const bool started = backend.Start(false);
	Check(!started, "Start reports false instead of crashing");
	Check(!backend.IsRunning(), "backend reports itself as not running");
	Check(!backend.IsSceneApplication(),
	      "and does not claim to hold the compositor either");

	obvr::vr::Quaternion orientation = obvr::vr::Quaternion::Identity();
	obvr::NiPoint3 position{0.0f, 0.0f, 0.0f};
	const bool read = backend.ReadHeadPose(orientation, position);
	Check(!read, "ReadHeadPose reports false without a connection");

	// A second attempt must neither crash nor flood the log.
	Check(!backend.Start(false), "second Start stays without effect as well");

	// Asking for the scene changes nothing here, and that is the point: the
	// route through Connect, the compositor query and the retreat back to
	// background all have to end in the same harmless place on a machine with
	// no SteamVR. This is the path most people meet first, and it is also the
	// one with the most steps to go wrong now.
	obvr::vr::OpenVRBackend sceneBackend;
	Check(!sceneBackend.Start(true), "asking for the scene fails just as quietly");
	Check(!sceneBackend.IsRunning(), "no runtime means no connection, scene or not");
	Check(!sceneBackend.IsSceneApplication(), "and certainly no compositor");
	sceneBackend.Stop();

	// Stop on a backend that never started has to be harmless too.
	backend.Stop();
	Check(true, "Stop without a prior Start does not crash");

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}
	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
