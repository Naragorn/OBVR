// Runs the backend against the harness's stand-in runtime itself
// (tools/mock-openvr, openvr_mock.dll, built beside this test): the DLL's
// exports and function tables as OBVR calls them, with no SteamVR. Also the
// two refusals of [Debug] OpenVRLibrary (vr/OpenVRLibrary.h): a name that is
// not a plain DLL's, and a stand-in that is not there.
//
// Windows only, like the backend's own test: it goes through LoadLibrary.

#include <cstdio>

#include "core/Types.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"
#include "vr/Quaternion.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.0001f) { return a - b < eps && b - a < eps; }

using namespace obvr;
using namespace obvr::vr;

void TestRefusals() {
	std::printf("Stand-ins that are not loaded\n");
	OpenVRBackend folder;
	Check(!folder.Start(true, "..\\openvr_mock.dll") && !folder.IsRunning(), "a name with a folder: refused");
	OpenVRBackend missing;
	Check(!missing.Start(true, "no_such_runtime.dll") && !missing.IsRunning(),
	      "a stand-in that is not there: not running, and openvr_api.dll not taken in its place");
	Check(!missing.Start(true, "openvr_mock.dll"), "a second Start stays without effect, whatever it names");
}

void TestOptics(const OpenVRBackend& backend) {
	std::printf("The stand-in's headset\n");
	UInt32 width = 0, height = 0;
	Check(backend.GetRecommendedRenderTargetSize(width, height) && width == 2568 && height == 2568,
	      "2568x2568 an eye");
	float l = 0.0f, r = 0.0f, t = 0.0f, b = 0.0f;
	Check(backend.GetEyeProjection(openvr::kEyeLeft, l, r, t, b) && Near(l, -1.1516f) && Near(r, 0.8250f) &&
	          Near(t, -1.1646f) && Near(b, 0.8165f),
	      "the left eye's frustum");
	Check(backend.GetEyeProjection(openvr::kEyeRight, l, r, t, b) && Near(l, -0.8419f) && Near(r, 1.1417f),
	      "the right eye's frustum");
	NiPoint3 left{}, right{};
	Check(backend.GetEyeOffset(openvr::kEyeLeft, left) && backend.GetEyeOffset(openvr::kEyeRight, right) &&
	          Near(right.x - left.x, 0.062f),
	      "the eyes 62 mm apart");
	openvr::HmdMatrix34 eye{};
	Check(backend.GetEyeToHead(openvr::kEyeRight, eye) && Near(eye.m[0][3], 0.031f) && Near(eye.m[0][0], 1.0f),
	      "the eye's matrix returned by value through the table");
	Check(Near(backend.DisplayFrequencyHz(), 75.0f), "75 Hz");
}

void TestPosesAndFrames(OpenVRBackend& backend) {
	std::printf("Poses and frames\n");
	Quaternion orientation = Quaternion::Identity();
	NiPoint3 position{};
	Check(!backend.ReadHeadPose(orientation, position), "the headset asleep: no head pose");
	float metres = 0.0f;
	Check(!backend.ReadStandingEyeHeight(metres), "and no eye height");
	HandPose hand{};
	Check(!backend.ReadHand(true, hand) && !backend.ReadHand(false, hand), "no controller in either hand");
	Check(backend.HandDeviceIndex(true) == openvr::kTrackedDeviceIndexInvalid, "and no device for one");

	Check(backend.WaitGetPoses() == openvr::kCompositorErrorNone, "WaitGetPoses answers ok");
	Check(!backend.GetRenderPose(orientation, position), "its pose not valid, so no render pose");
	const long long frequency = ReadPerformanceFrequency();
	const long long before = ReadPerformanceCounter();
	bool allOk = true;
	for (int i = 0; i < 15; ++i) {
		allOk = allOk && backend.WaitGetPoses() == openvr::kCompositorErrorNone;
	}
	const double ms = static_cast<double>(ReadPerformanceCounter() - before) * 1000.0 / static_cast<double>(frequency);
	std::printf("        15 frames took %.1f ms\n", ms);
	// 200 ms on the beat (199.9 under Wine, 2026-11-17). The lower bound is the
	// proof of the wait; the upper one is loose, since a sleep on Windows may
	// run to the next 15.6 ms timer tick and skip every other beat.
	Check(allOk && ms > 150.0 && ms < 900.0, "15 frames held to the 75 Hz beat, not handed out at once");

	openvr::CompositorFrameTiming timing{};
	Check(backend.GetFrameTiming(timing, 1) && timing.numFramePresents == 1 && timing.frameIndex == 15 &&
	          Near(timing.newPosesReadyMs, -3.0f),
	      "the frame before the last: shown once, its poses 3 ms before its vsync");

	int picture = 0;
	Check(backend.SubmitEye(openvr::kEyeLeft, &picture, openvr::kTextureTypeVulkan, nullptr) ==
	          openvr::kCompositorErrorNone,
	      "an eye submitted: taken");
	openvr::HmdMatrix34 pose{};
	Check(backend.SubmitEye(openvr::kEyeRight, &picture, openvr::kTextureTypeVulkan, nullptr, &pose) ==
	          openvr::kCompositorErrorNone,
	      "an eye submitted with its pose: taken");
}

void TestOverlays(OpenVRBackend& backend) {
	std::printf("Overlays\n");
	openvr::VROverlayHandle hud = openvr::kOverlayHandleInvalid;
	openvr::VROverlayHandle twice = openvr::kOverlayHandleInvalid;
	Check(backend.CreateOverlay("obvr.test.hud", "OBVR test", hud) && hud != openvr::kOverlayHandleInvalid,
	      "an overlay made");
	Check(!backend.CreateOverlay("obvr.test.hud", "OBVR test", twice), "its key a second time: refused");
	Check(backend.SetOverlayWidthInMetres(hud, 1.6f) == 0 && backend.SetOverlayAlpha(hud, 0.5f) == 0 &&
	          backend.SetOverlayColor(hud, 1.0f, 1.0f, 1.0f) == 0 && backend.SetOverlaySortOrder(hud, 3) == 0,
	      "its width, alpha, colour and order taken");
	openvr::HmdMatrix34 place{};
	const openvr::VRTextureBounds bounds{0.0f, 0.0f, 0.5f, 0.5f};
	const UInt32 pixel = 0xFFFFFFFFu;
	Check(backend.SetOverlayTransformAbsolute(hud, place) == 0 && backend.SetOverlayTransformHmdRelative(hud, place) == 0 &&
	          backend.SetOverlayTextureBounds(hud, bounds) == 0 && backend.SetOverlayRaw(hud, &pixel, 1, 1) == 0,
	      "its place, bounds and pixels taken");
	Check(backend.ShowOverlay(hud) == 0 && backend.HideOverlay(hud) == 0, "shown and hidden");
	Check(backend.SetOverlayAlpha(hud + 1000, 0.5f) != 0, "an overlay never made: an error");
	Check(backend.DestroyOverlay(hud) == 0 && backend.SetOverlayAlpha(hud, 0.5f) != 0, "destroyed: an error from then on");
	Check(!backend.ShowKeyboard("test", "", 16), "SteamVR's keyboard: refused");
}

void TestStandIn() {
	std::printf("The stand-in runtime\n");
	OpenVRBackend backend;
	Check(backend.Start(true, "openvr_mock.dll"), "Start with the stand-in beside the test: connected");
	if (!backend.IsRunning()) {
		Check(false, "running (the rest needs it; is openvr_mock.dll beside the test?)");
		return;
	}
	Check(backend.IsSceneApplication(), "as a scene application, with the compositor");
	TestOptics(backend);
	TestPosesAndFrames(backend);
	TestOverlays(backend);
	FrameEvents events{};
	backend.PollEvents(false, events);
	Check(!events.quit, "no event waiting, no quit");
	backend.FadeToColor(0.1f, 0.0f, 0.0f, 0.0f, 1.0f);
	backend.HoldScene(true);
	backend.HoldScene(false);
	Check(Near(backend.CurrentGridAlpha(), 0.0f), "fades and the held scene taken; the grid's alpha 0");
	backend.Stop();
	Check(!backend.IsRunning(), "stopped");
}

}  // namespace

int main() {
	// "Beside OBVR.dll" is beside this test: the plugin folder is the module's own.
	obvr::platform::SetPluginModule(GetModuleHandleA(nullptr));
	TestRefusals();
	TestStandIn();
	std::printf(g_failures == 0 ? "\nAll checks passed.\n" : "\nCHECKS FAILED.\n");
	return g_failures == 0 ? 0 : 1;
}
