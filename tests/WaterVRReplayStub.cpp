#include "test/WaterVRTestRuntime.h"

namespace obvr::test {

bool g_waterReplayActive = false;

bool WaterVRReplayActive() { return g_waterReplayActive; }

void GetWaterVRReplayPose(vr::Quaternion& orientation, NiPoint3& position) {
	orientation = {0.5f, 0.25f, -0.125f, 0.75f};
	position = {4.0f, 5.0f, 6.0f};
}

void GetWaterVRReplayPoseMatrix(vr::openvr::HmdMatrix34& matrix) {
	matrix = {};
	matrix.m[0][0] = matrix.m[1][1] = matrix.m[2][2] = 1.0f;
	matrix.m[0][3] = 4.0f;
	matrix.m[1][3] = 5.0f;
	matrix.m[2][3] = 6.0f;
}

}  // namespace obvr::test

// The hand script's head, off in these tests (test/HandScriptRuntime.h).
#include "test/HandScriptRuntime.h"

namespace obvr::test {

bool HandScriptHeadActive() { return false; }

void GetHandScriptHeadPose(vr::Quaternion& orientation, NiPoint3& position) {
	orientation = vr::Quaternion::Identity();
	position = {0.0f, 0.0f, 0.0f};
}

void GetHandScriptHeadPoseMatrix(vr::openvr::HmdMatrix34& matrix) { matrix = {}; }

}  // namespace obvr::test
