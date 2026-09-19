#include "test/WaterVRTestRuntime.h"
#include "test/WaterVRTestPlan.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/Config.h"
#include "camera/CameraHook.h"
#include "core/Log.h"
#include "platform/PluginPath.h"
#include "render/LayoutProbe.h"
#include "render/InterfaceRenderHook.h"
#include "render/WaterReprojection.h"

namespace obvr::test {
namespace {

bool g_enabled = false;
bool g_active = false;
bool g_finished = false;
UInt32 g_frame = 0;
UInt32 g_settleFrames = 0;
float g_replayPitch = -35.0f;
UInt32 g_imageMask = 0;
UInt32 g_matrixMask = 0;
UInt32 g_fixedCaptureMask = 0;
UInt32 g_projectionScaleMask = 0;
UInt32 g_eyeReuseMask = 0;
UInt32 g_firstWorldEyeMask = 0;
UInt32 g_lastSnapshotSerial = 0;
NiMatrix33 g_captureRotation{};
bool g_captureRotationValid = false;
bool g_lifecycleSuppressed = false;
bool g_lifecycleRecovered = false;
UInt32 g_leftProjectionSerial[kWaterTestViews][2]{};
UInt32 g_leftRenderSerial[kWaterTestViews][2]{};
UInt32 g_leftReuseSerial[kWaterTestViews][2]{};

bool SameRotation(const NiMatrix33& left, const NiMatrix33& right) {
	float error = 0.0f;
	for (unsigned row = 0; row < 3; ++row)
		for (unsigned column = 0; column < 3; ++column) {
			const float delta = left.data[row][column] - right.data[row][column];
			error += delta * delta;
		}
	return std::isfinite(error) && error < 1.0e-6f;
}

void ArmWaterVRTest() {
	g_replayPitch = WaterTestPitch(GetConfig().vrTestWaterPitch);
	g_enabled = true;
	g_active = false;
	g_finished = false;
	g_frame = 0;
	g_settleFrames = 0;
	g_imageMask = 0;
	g_matrixMask = 0;
	g_fixedCaptureMask = 0;
	g_projectionScaleMask = 0;
	g_eyeReuseMask = 0;
	g_firstWorldEyeMask = 0;
	g_lastSnapshotSerial = 0;
	g_captureRotation = NiMatrix33{};
	g_captureRotationValid = false;
	g_lifecycleSuppressed = false;
	g_lifecycleRecovered = false;
	std::memset(g_leftProjectionSerial, 0, sizeof(g_leftProjectionSerial));
	std::memset(g_leftRenderSerial, 0, sizeof(g_leftRenderSerial));
	std::memset(g_leftReuseSerial, 0, sizeof(g_leftReuseSerial));
	OBVR_LOG("VRTEST water runner armed schema=21 views=%u yaw=-60..60 original-target=1",
	         kWaterTestViews);
	OBVR_LOG("VRTEST water replay pitch=%.9g", static_cast<double>(g_replayPitch));
}

}  // namespace

void InstallWaterVRTest() {
	if (GetConfig().vrTestSuite && GetConfig().vrTestWaterOnly) ArmWaterVRTest();
}
bool WaterVRReplayActive() {
	return WaterTestReplayActive(g_enabled, g_active, g_finished);
}

bool SuppressWaterVRFirstEyeReflection(bool firstEye) {
	if (!WaterTestReplayActive(g_enabled, g_active, g_finished) ||
	    !WaterTestShouldSuppressLifecycleCapture(g_frame, firstEye,
	                                             g_lifecycleSuppressed)) return false;
	g_lifecycleSuppressed = true;
	OBVR_LOG("VRTEST lifecycle suppressed first-eye reflection frame=%u", g_frame);
	return true;
}

void ObserveWaterVRReflectionRendered(bool secondEye) {
	if (g_lifecycleSuppressed && secondEye && !g_lifecycleRecovered) {
		g_lifecycleRecovered = true;
		OBVR_LOG("VRTEST lifecycle recovered by second-eye render frame=%u", g_frame);
	}
}

void GetWaterVRReplayPose(vr::Quaternion& orientation, NiPoint3& position) {
	orientation = vr::FromAxisAngle(0.0f, 1.0f, 0.0f, WaterTestViewYaw(g_frame)) *
	              vr::FromAxisAngle(1.0f, 0.0f, 0.0f, g_replayPitch);
	position = NiPoint3{0.0f, 1.6f, 0.0f};
}

void GetWaterVRReplayPoseMatrix(vr::openvr::HmdMatrix34& matrix) {
	vr::Quaternion orientation{};
	NiPoint3 position{};
	GetWaterVRReplayPose(orientation, position);
	const NiMatrix33 rotation = vr::ToMatrix(orientation);
	for (unsigned row = 0; row < 3; ++row)
		for (unsigned column = 0; column < 3; ++column)
			matrix.m[row][column] = rotation.data[row][column];
	matrix.m[0][3] = position.x;
	matrix.m[1][3] = position.y;
	matrix.m[2][3] = position.z;
}

void ObserveWaterVREye(bool left, void* device) {
	if (g_active && !g_finished && g_frame == 0) {
		const UInt32 eyeBit = left ? 1u : 2u;
		if ((g_firstWorldEyeMask & eyeBit) == 0) {
			char firstName[64]{};
			char firstPath[512]{};
			std::snprintf(firstName, sizeof(firstName), "OBVR-VRTest-first-world-%s.bmp",
			              left ? "L" : "R");
			const bool saved = platform::BuildGamePath(firstName, firstPath, sizeof(firstPath)) &&
			                   render::DumpBackBufferBmp(device, firstPath);
			if (saved) g_firstWorldEyeMask |= eyeBit;
			OBVR_LOG("VRTEST first-world eye=%s image=%u frame=%u",
			         left ? "left" : "right", saved, g_frame);
		}
	}
	const int phase = WaterTestCapturePhase(g_active, g_finished, g_frame);
	if (phase < 0) return;
	const UInt32 view = WaterTestViewIndex(g_frame);
	const UInt32 step = g_frame % kWaterTestFramesPerView;
	const UInt32 bit = WaterTestEyeBit(view, static_cast<unsigned>(phase), left);
	char name[96]{};
	char path[512]{};
	std::snprintf(name, sizeof(name), "OBVR-VRTest-water-view-%u-step-%u-%s.bmp",
	          view, step, left ? "L" : "R");
	const bool imageOk = platform::BuildGamePath(name, path, sizeof(path)) &&
	                     render::DumpBackBufferBmp(device, path);
	if (imageOk) g_imageMask |= bit;
	// Independent of the water shader path: a native-mode control has no
	// fresh reprojection snapshot. Read the camera at this eye's capture point.
	NiTransform observedCamera{};
	const bool observedCameraValid = camera::GetCurrentCameraWorldTransform(observedCamera);
	for (unsigned row = 0; row < 3; ++row) {
		OBVR_LOG("VRTEST water-observed-camera view=%u step=%u eye=%s valid=%u row=%u rotation=(%.9g,%.9g,%.9g) pos=(%.9g,%.9g,%.9g) scale=%.9g",
			view, step, left ? "left" : "right", observedCameraValid, row,
			observedCamera.rot.data[row][0], observedCamera.rot.data[row][1], observedCamera.rot.data[row][2],
			observedCamera.pos.x, observedCamera.pos.y, observedCamera.pos.z, observedCamera.scale);
	}
	std::snprintf(name, sizeof(name), "OBVR-VRTest-reflection-view-%u-step-%u-%s.bmp",
	          view, step, left ? "L" : "R");
	const bool targetOk = platform::BuildGamePath(name, path, sizeof(path)) &&
	                     render::DumpWaterReflectionTarget(device, path);
	render::WaterReflectionTargetSnapshot target{};
	const bool targetMeasured = render::GetWaterReflectionTargetSnapshot(target);
	OBVR_LOG("VRTEST reflection-target view=%u step=%u eye=%s image=%u measured=%u serial=%u candidates=%u size=%ux%u viewport=%u,%u,%u,%u",
		view, step, left ? "left" : "right", targetOk, targetMeasured,
		target.serial, target.candidates, target.width, target.height,
		target.viewportX, target.viewportY, target.viewportWidth, target.viewportHeight);

	render::WaterReprojectionSnapshot snapshot{};
	const bool matrixOk = render::GetWaterReprojectionSnapshot(snapshot) &&
	                      snapshot.serial > g_lastSnapshotSerial &&
	                      !snapshot.fallback;
	if (matrixOk) {
		g_lastSnapshotSerial = snapshot.serial;
		g_matrixMask |= bit;
		if (!g_captureRotationValid) {
			g_captureRotation = snapshot.capture.rot;
			g_captureRotationValid = true;
		}
		if (SameRotation(snapshot.capture.rot, g_captureRotation))
			g_fixedCaptureMask |= bit;
		if (std::fabs(snapshot.captureHorizontalScale - 3.0f) < 0.001f &&
		    std::fabs(snapshot.captureVerticalScale - 1.5f) < 0.001f)
			g_projectionScaleMask |= bit;

		bool reuseOk = false;
		if (left) {
			g_leftProjectionSerial[view][phase] = snapshot.captureProjectionSerial;
			g_leftRenderSerial[view][phase] = snapshot.reflectionRenderSerial;
			g_leftReuseSerial[view][phase] = snapshot.reflectionReuseSerial;
			reuseOk = snapshot.pass == render::WaterStereoPass::First &&
			          !snapshot.reusedCaptureProjection &&
			          snapshot.captureProjectionSerial != 0 &&
			          snapshot.reflectionRenderSerial != 0 &&
			          snapshot.storedWaterDrawCount > 0;
		} else {
			reuseOk = snapshot.pass == render::WaterStereoPass::Second &&
			          snapshot.reusedCaptureProjection &&
			          snapshot.replayedWaterDrawCount > 0 &&
			          snapshot.captureProjectionSerial == g_leftProjectionSerial[view][phase] &&
			          snapshot.reflectionRenderSerial == g_leftRenderSerial[view][phase] &&
			          snapshot.reflectionReuseSerial > g_leftReuseSerial[view][phase];
		}
		if (reuseOk) g_eyeReuseMask |= bit;
	}

	OBVR_LOG("VRTEST water-image view=%u step=%u eye=%s yaw=%.1f image=%u matrix=%u "
	         "fallback=%u scale=(%.2f,%.2f) storedDraws=%u replayedDraws=%u "
	         "capture=(%.3f,%.3f,%.3f)",
	         view, step, left ? "left" : "right", static_cast<double>(WaterTestViewYaw(g_frame)),
	         imageOk, matrixOk, snapshot.fallback,
	         static_cast<double>(snapshot.captureHorizontalScale),
	         static_cast<double>(snapshot.captureVerticalScale),
	         snapshot.storedWaterDrawCount, snapshot.replayedWaterDrawCount,
	         static_cast<double>(snapshot.capture.pos.x),
	         static_cast<double>(snapshot.capture.pos.y),
	         static_cast<double>(snapshot.capture.pos.z));

	// Camera pose alone cannot prove that the shader's projective coordinates
	// are head independent. Remove the current mesh transform before comparing
	// samples: different yaw views can end on different water meshes.
	render::WaterMatrix worldProjection{};
	const bool worldProjectionOk = matrixOk && render::BuildWaterWorldProjectionMatrix(
		snapshot.reprojected, snapshot.world, worldProjection);
	for (unsigned row = 0; row < 4; ++row) {
		OBVR_LOG("VRTEST water-input view=%u step=%u eye=%s row=%u "
		         "mvp=(%.9g,%.9g,%.9g,%.9g) world=(%.9g,%.9g,%.9g,%.9g)",
		         view, step, left ? "left" : "right", row,
		         static_cast<double>(snapshot.current.m[row][0]),
		         static_cast<double>(snapshot.current.m[row][1]),
		         static_cast<double>(snapshot.current.m[row][2]),
		         static_cast<double>(snapshot.current.m[row][3]),
		         static_cast<double>(snapshot.world.m[row][0]),
		         static_cast<double>(snapshot.world.m[row][1]),
		         static_cast<double>(snapshot.world.m[row][2]),
		         static_cast<double>(snapshot.world.m[row][3]));
		OBVR_LOG("VRTEST water-world-projection view=%u step=%u eye=%s valid=%u row=%u "
		         "value=(%.9g,%.9g,%.9g,%.9g)", view, step, left ? "left" : "right",
		         worldProjectionOk, row, static_cast<double>(worldProjection.m[row][0]),
		         static_cast<double>(worldProjection.m[row][1]),
		         static_cast<double>(worldProjection.m[row][2]),
		         static_cast<double>(worldProjection.m[row][3]));
	}
	for (unsigned row = 0; row < 3; ++row) {
		OBVR_LOG("VRTEST water-live-camera view=%u step=%u eye=%s row=%u "
		         "rotation=(%.9g,%.9g,%.9g) pos=(%.9g,%.9g,%.9g) scale=%.9g",
		         view, step, left ? "left" : "right", row,
		         static_cast<double>(snapshot.live.rot.data[row][0]),
		         static_cast<double>(snapshot.live.rot.data[row][1]),
		         static_cast<double>(snapshot.live.rot.data[row][2]),
		         static_cast<double>(snapshot.live.pos.x),
		         static_cast<double>(snapshot.live.pos.y),
		         static_cast<double>(snapshot.live.pos.z),
		         static_cast<double>(snapshot.live.scale));
	}
}

void AdvanceWaterVRTest(bool playerInWorld) {
	const bool requested = GetConfig().vrTestSuite && GetConfig().vrTestWaterOnly;
	switch (ChooseWaterTestEnableAction(requested, g_enabled, g_active)) {
	case WaterTestEnableAction::Arm:
		ArmWaterVRTest();
		break;
	case WaterTestEnableAction::Disarm:
		g_enabled = false;
		return;
	case WaterTestEnableAction::None:
		break;
	}
	if (!g_enabled || g_finished) return;
	if (!g_active) {
		if (!playerInWorld) return;
		g_active = true;
		g_frame = 0;
		g_settleFrames = 0;
		OBVR_LOG("VRTEST first-world capture window started");
		return;
	}
	if (!playerInWorld) return;
	if (!WaterTestSweepMayAdvance(g_settleFrames)) {
		++g_settleFrames;
		if (WaterTestSweepMayAdvance(g_settleFrames))
			OBVR_LOG("VRTEST water-only sweep started after %u settle frames", g_settleFrames);
		return;
	}
	++g_frame;
	if (g_frame < kWaterTestViews * kWaterTestFramesPerView) return;

	const bool passed = WaterTestEvidenceComplete(
		g_imageMask, g_matrixMask, g_fixedCaptureMask,
		g_projectionScaleMask, g_eyeReuseMask) && g_lifecycleRecovered;
	g_finished = true;
	g_active = false;
	OBVR_LOG("VRTEST water-sweep status=%s views=%u imageMask=%08X "
	         "matrixMask=%08X fixedCaptureMask=%08X projectionScaleMask=%08X "
	         "eyeReuseMask=%08X expected=%08X",
	         passed ? "pass" : "fail", kWaterTestViews, g_imageMask, g_matrixMask,
	         g_fixedCaptureMask, g_projectionScaleMask, g_eyeReuseMask,
	         kWaterTestExpectedMask);
	OBVR_LOG("VRTEST lifecycle status=%s suppressed=%u recovered=%u",
	         g_lifecycleRecovered ? "pass" : "fail",
	         g_lifecycleSuppressed, g_lifecycleRecovered);
	OBVR_LOG("VRTEST finished status=%s cases=0", passed ? "pass" : "fail");
}

}  // namespace obvr::test
