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

void TestLevelPoseKeepsHeadingOnly() {
	std::printf("Levelling an anchor pose leaves the heading and nothing else\n");

	using obvr::vr::LevelPose;

	// Looking 30 degrees down, turned 40 degrees, head tilted 25 degrees.
	const float pitch = -0.5236f;
	const float yaw = 0.6981f;
	const float roll = 0.4363f;

	obvr::vr::openvr::HmdMatrix34 pose = PoseLookingAt(pitch, yaw, roll);
	LevelPose(pose);

	// Up is world up exactly. Roll would tilt it, and so would pitch.
	CheckNear(pose.m[0][1], 0.0f, "up has no x");
	CheckNear(pose.m[1][1], 1.0f, "up is world up");
	CheckNear(pose.m[2][1], 0.0f, "up has no z");

	// The view direction is horizontal: the heading is kept, the aim is not.
	// This is deliberate - in the world the recenter key turns the wearer and
	// does not change how high they are looking, and an anchor that kept pitch
	// would make the same key mean two things.
	CheckNear(pose.m[1][2], 0.0f, "the view direction is level, so pitch is gone");

	// The heading itself survives. Forward is the negative of column 2, and
	// for a yaw of 40 degrees that is ( -sin 40, 0, -cos 40 ).
	CheckNear(-pose.m[0][2], -std::sin(yaw), "the heading keeps its x");
	CheckNear(-pose.m[2][2], -std::cos(yaw), "and its z");

	// Still a rotation: unit columns, mutually perpendicular. One that is not
	// would shear the picture rather than turn it, and the failure would look
	// like a rendering fault rather than a matrix.
	const float rr = pose.m[0][0] * pose.m[0][0] + pose.m[1][0] * pose.m[1][0] +
	                 pose.m[2][0] * pose.m[2][0];
	const float bb = pose.m[0][2] * pose.m[0][2] + pose.m[1][2] * pose.m[1][2] +
	                 pose.m[2][2] * pose.m[2][2];
	CheckNear(rr, 1.0f, "right is a unit vector");
	CheckNear(bb, 1.0f, "backward is a unit vector");

	const float rb = pose.m[0][0] * pose.m[0][2] + pose.m[1][0] * pose.m[1][2] +
	                 pose.m[2][0] * pose.m[2][2];
	CheckNear(rb, 0.0f, "right and backward are perpendicular");

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

void TestOverlayPoseAhead() {
	std::printf("Placing the HUD ahead of its anchor\n");

	using obvr::vr::LevelPose;
	using obvr::vr::OverlayPoseAhead;

	// Facing straight down negative Z, standing at eye height.
	obvr::vr::openvr::HmdMatrix34 pose = PoseLookingAt(0.0f, 0.0f, 0.0f);
	pose.m[0][3] = 0.0f;
	pose.m[1][3] = 1.7f;
	pose.m[2][3] = 0.0f;

	const obvr::vr::openvr::HmdMatrix34 ahead = OverlayPoseAhead(pose, 1.2f);

	// Forward is negative Z, so the quad is 1.2 metres that way - and at the
	// same height, because forward is horizontal for a levelled pose.
	CheckNear(ahead.m[0][3], 0.0f, "no sideways drift facing forward");
	CheckNear(ahead.m[1][3], 1.7f, "stays at the anchor's height");
	CheckNear(ahead.m[2][3], -1.2f, "one distance along negative z");

	// The rotation is carried over untouched. A quad that picked up a
	// rotation here would hang crooked for as long as the anchor stands.
	bool rotationKept = true;
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			if (ahead.m[row][col] != pose.m[row][col]) {
				rotationKept = false;
			}
		}
	}
	Check(rotationKept, "the anchor's rotation is carried over exactly");

	// Turned a quarter turn, the quad goes round with the heading rather
	// than staying where it was. This is the flow that decides whether the
	// wearer finds their HUD after recentering while facing a new way.
	obvr::vr::openvr::HmdMatrix34 turned = PoseLookingAt(0.0f, 1.5708f, 0.0f);
	turned.m[0][3] = 0.0f;
	turned.m[1][3] = 1.7f;
	turned.m[2][3] = 0.0f;
	LevelPose(turned);

	const obvr::vr::openvr::HmdMatrix34 aheadTurned = OverlayPoseAhead(turned, 1.2f);
	CheckNear(aheadTurned.m[1][3], 1.7f, "a turned anchor still hangs at head height");

	// Whichever way it faces, the quad is exactly one distance from the
	// anchor. That is the property worth holding: distance is what the
	// setting promises, and it must not depend on the heading.
	const float dx = aheadTurned.m[0][3] - turned.m[0][3];
	const float dy = aheadTurned.m[1][3] - turned.m[1][3];
	const float dz = aheadTurned.m[2][3] - turned.m[2][3];
	CheckNear(dx * dx + dy * dy + dz * dz, 1.44f, "one distance away whatever the heading");

	// Zero distance leaves the pose alone, which is what a person setting
	// HudDistanceMetres=0 should get rather than a division or a flip.
	const obvr::vr::openvr::HmdMatrix34 here = OverlayPoseAhead(pose, 0.0f);
	CheckNear(here.m[2][3], pose.m[2][3], "no distance means no move");
}

void TestPoseDistance() {
	std::printf("Measuring the distance between two poses\n");

	using obvr::vr::PoseDistanceSq;

	obvr::vr::openvr::HmdMatrix34 a = PoseLookingAt(0.0f, 0.0f, 0.0f);
	a.m[0][3] = 1.0f;
	a.m[1][3] = 1.7f;
	a.m[2][3] = -2.0f;

	Check(PoseDistanceSq(a, a) == 0.0f, "a pose is at distance zero from itself");

	obvr::vr::openvr::HmdMatrix34 b = a;
	b.m[0][3] += 3.0f;
	b.m[2][3] += 4.0f;
	CheckNear(PoseDistanceSq(a, b), 25.0f, "3-0-4 apart measures 25 squared");

	// Rotation must not contribute: the anchor self-heal asks where the head
	// is, not where it looks - turning in place must never drop the anchor.
	obvr::vr::openvr::HmdMatrix34 turnedInPlace = PoseLookingAt(0.3f, 2.0f, 0.5f);
	turnedInPlace.m[0][3] = a.m[0][3];
	turnedInPlace.m[1][3] = a.m[1][3];
	turnedInPlace.m[2][3] = a.m[2][3];
	Check(PoseDistanceSq(a, turnedInPlace) == 0.0f, "turning in place is distance zero");
}

}  // namespace

int main() {
	std::printf("OBVR OpenVR backend, fallback without SteamVR\n\n");

	TestLevelPoseKeepsHeadingOnly();
	std::printf("\n");
	TestLevelPoseLookingStraightDown();
	std::printf("\n");
	TestOverlayPoseAhead();
	std::printf("\n");
	TestPoseDistance();
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
