// Checks what the stand-in OpenVR runtime answers (tools/mock-openvr/
// MockRuntime.h): the interfaces it has, the sleeping headset, the frame
// clock, the overlays' and the actions' books.

#include <cstdio>
#include <cstring>

#include "MockRuntime.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.0001f) { return a - b < eps && b - a < eps; }

using namespace obvr;
using namespace obvr::mockvr;

void TestInterfaces() {
	std::printf("Interfaces\n");
	Check(InterfaceByName("FnTable:IVRSystem_026") == Interface::System, "the system table OBVR names");
	Check(InterfaceByName("FnTable:IVRCompositor_029") == Interface::Compositor, "the compositor table");
	Check(InterfaceByName("FnTable:IVROverlay_028") == Interface::Overlay, "the overlay table");
	Check(InterfaceByName("FnTable:IVRInput_011") == Interface::Input, "the input table");
	Check(InterfaceByName("FnTable:IVRRenderModels_006") == Interface::RenderModels, "the render models table");
	Check(InterfaceByName("IVRSystem_026") == Interface::None, "the C++ flavour: not there");
	Check(InterfaceByName("FnTable:IVRSystem_027") == Interface::None, "another version: not there");
	Check(InterfaceByName("") == Interface::None && InterfaceByName(nullptr) == Interface::None, "no name: not there");
}

void TestProfiles() {
	std::printf("The headsets\n");
	const Headset air = MakeHeadset(HeadsetKind::DreamAir);
	Check(air.eyeWidth == 4036 && air.eyeHeight == 3376 && Near(air.displayHz, 72.0f),
	      "the Dream Air: 4036x3376 an eye at 72 Hz");
	Check(Near(air.raw[0][0], -1.4019f) && Near(air.raw[0][1], 1.2131f) && Near(air.raw[0][2], -1.0913f) &&
	          Near(air.raw[0][3], 1.0913f) && Near(air.raw[1][0], -1.2131f) && Near(air.raw[1][1], 1.4019f),
	      "its frustums, the right eye's the left one's mirrored");
	Check(Near(air.eyeX[0], -0.0315f) && Near(air.eyeX[1], 0.0315f), "its eyes 63 mm apart");
	Check(Near(air.runningStartMs, 1000.0f / 72.0f, 0.01f), "its poses handed out a whole period before the vsync");
	Check(std::strcmp(air.model, "OBVR stand-in (Pimax Dream Air)") == 0, "named as the Dream Air's stand-in");
	const Headset air90 = MakeHeadset(HeadsetKind::DreamAir, 90.0f);
	Check(Near(air90.displayHz, 90.0f) && Near(air90.runningStartMs, 1000.0f / 90.0f, 0.01f) && air90.eyeWidth == 4036,
	      "at 90 Hz: the same headset, the period and the hand-out with it");
	const Headset beyond = MakeHeadset(HeadsetKind::Beyond);
	Check(beyond.eyeWidth == 2568 && beyond.eyeHeight == 2568 && Near(beyond.displayHz, 75.0f) &&
	          Near(beyond.eyeX[1], 0.031f) && Near(beyond.runningStartMs, 3.0f) &&
	          std::strcmp(beyond.model, "OBVR stand-in (Bigscreen Beyond)") == 0,
	      "the Beyond: 2568x2568 at 75 Hz, 62 mm, a 3 ms running start");
	Check(Near(MakeHeadset(HeadsetKind::Beyond, 90.0f).runningStartMs, 3.0f), "its running start the same at 90 Hz");
	Check(Near(MakeHeadset(HeadsetKind::DreamAir, 0.0f).displayHz, 72.0f) &&
	          Near(MakeHeadset(HeadsetKind::DreamAir, -5.0f).displayHz, 72.0f),
	      "no rate given: the headset's own");
	Check(kDefaultHeadset == HeadsetKind::DreamAir, "the Dream Air is the default");
}

void TestHeadsetNames() {
	std::printf("Headsets by name\n");
	HeadsetKind kind = HeadsetKind::Beyond;
	Check(HeadsetByName("dreamair", kind) && kind == HeadsetKind::DreamAir, "dreamair");
	Check(HeadsetByName("beyond", kind) && kind == HeadsetKind::Beyond, "beyond");
	Check(HeadsetByName("DreamAir", kind) && kind == HeadsetKind::DreamAir, "in another case");
	kind = HeadsetKind::Beyond;
	Check(HeadsetByName("", kind) && kind == HeadsetKind::DreamAir, "no name: the default");
	kind = HeadsetKind::Beyond;
	Check(HeadsetByName(nullptr, kind) && kind == HeadsetKind::DreamAir, "no text: the default");
	kind = HeadsetKind::Beyond;
	Check(!HeadsetByName("index", kind) && kind == HeadsetKind::DreamAir, "a headset not known: refused, the default");
}

void TestOverrides() {
	std::printf("A headset as configured\n");
	float hz = 0.0f;
	Check(ParseRate("90", hz) && Near(hz, 90.0f) && ParseRate("72.5", hz) && Near(hz, 72.5f), "a rate");
	Check(!ParseRate("", hz) && !ParseRate(nullptr, hz) && !ParseRate("fast", hz) && !ParseRate("90hz", hz),
	      "no rate, no number, a number with letters after it: none");
	Check(!ParseRate("29", hz) && !ParseRate("241", hz) && !ParseRate("-90", hz) && ParseRate("30", hz) &&
	          ParseRate("240", hz),
	      "rates from 30 to 240 only");
	UInt32 w = 0, h = 0;
	Check(ParseEyeSize("3840x3552", w, h) && w == 3840 && h == 3552, "an eye size");
	Check(ParseEyeSize("1024X768", w, h) && w == 1024 && h == 768, "with a capital X");
	Check(!ParseEyeSize("", w, h) && !ParseEyeSize(nullptr, w, h) && !ParseEyeSize("3840", w, h) &&
	          !ParseEyeSize("3840x", w, h) && !ParseEyeSize("x3552", w, h) && !ParseEyeSize("3840x3552px", w, h) &&
	          !ParseEyeSize("3840 x 3552", w, h),
	      "half a size, or one with more around it: none");
	Check(!ParseEyeSize("255x3552", w, h) && !ParseEyeSize("3840x16385", w, h) && ParseEyeSize("256x16384", w, h),
	      "sides from 256 to 16384 only");
	Check(w == 256 && h == 16384, "a size refused leaves the last one read");
	float mm = 0.0f;
	Check(ParseIpdMm("63", mm) && Near(mm, 63.0f) && ParseIpdMm("58.5", mm) && Near(mm, 58.5f), "an eye distance");
	Check(!ParseIpdMm("", mm) && !ParseIpdMm(nullptr, mm) && !ParseIpdMm("49", mm) && !ParseIpdMm("81", mm) &&
	          !ParseIpdMm("63mm", mm),
	      "none, one under 50 or over 80 mm, one with letters: none");

	const Headset plain = ConfiguredHeadset(nullptr, nullptr, nullptr, nullptr);
	Check(plain.eyeWidth == 4036 && Near(plain.displayHz, 72.0f) && Near(plain.eyeX[1], 0.0315f),
	      "nothing configured: the Dream Air as it is");
	const Headset all = ConfiguredHeadset("dreamair", "90", "3840x3552", "65");
	Check(all.eyeWidth == 3840 && all.eyeHeight == 3552 && Near(all.displayHz, 90.0f) && Near(all.eyeX[0], -0.0325f) &&
	          Near(all.eyeX[1], 0.0325f) && Near(all.runningStartMs, 1000.0f / 90.0f, 0.01f) &&
	          Near(all.raw[0][0], -1.4019f),
	      "a rate, an eye size and an eye distance over the Dream Air's own");
	const Headset beyond = ConfiguredHeadset("beyond", "", "", "");
	Check(beyond.eyeWidth == 2568 && Near(beyond.displayHz, 75.0f) && Near(beyond.eyeX[1], 0.031f),
	      "the Beyond by name, nothing over it");
	const Headset wrong = ConfiguredHeadset("index", "fast", "big", "wide");
	Check(wrong.eyeWidth == 4036 && wrong.eyeHeight == 3376 && Near(wrong.displayHz, 72.0f) && Near(wrong.eyeX[1], 0.0315f),
	      "values that cannot be read: the default headset as it is");
}

void TestHeadset() {
	std::printf("The headset's optics\n");
	const Headset headset = MakeHeadset(HeadsetKind::Beyond);
	float l = 9.0f, r = 9.0f, t = 9.0f, b = 9.0f;
	ProjectionRaw(headset, openvr::kEyeLeft, &l, &r, &t, &b);
	Check(Near(l, -1.1516f) && Near(r, 0.8250f) && Near(t, -1.1646f) && Near(b, 0.8165f), "the left eye's frustum");
	ProjectionRaw(headset, openvr::kEyeRight, &l, &r, &t, &b);
	Check(Near(l, -0.8419f) && Near(r, 1.1417f) && Near(t, -1.1572f) && Near(b, 0.8309f), "the right eye's frustum");
	l = r = t = b = 9.0f;
	ProjectionRaw(headset, 2, &l, &r, &t, &b);
	ProjectionRaw(headset, -1, &l, &r, &t, &b);
	Check(Near(l, 9.0f) && Near(r, 9.0f) && Near(t, 9.0f) && Near(b, 9.0f), "an eye that is neither: nothing written");
	ProjectionRaw(headset, openvr::kEyeLeft, nullptr, &r, &t, &b);
	ProjectionRaw(headset, openvr::kEyeLeft, &l, nullptr, &t, &b);
	ProjectionRaw(headset, openvr::kEyeLeft, &l, &r, nullptr, &b);
	ProjectionRaw(headset, openvr::kEyeLeft, &l, &r, &t, nullptr);
	Check(Near(l, 9.0f) && Near(r, 9.0f) && Near(t, 9.0f) && Near(b, 9.0f), "a missing place to write to: nothing written");

	const openvr::HmdMatrix34 left = EyeToHead(headset, openvr::kEyeLeft);
	const openvr::HmdMatrix34 right = EyeToHead(headset, openvr::kEyeRight);
	Check(Near(left.m[0][3], -0.031f) && Near(right.m[0][3], 0.031f), "the eyes 31 mm either side of the middle");
	Check(Near(left.m[0][0], 1.0f) && Near(left.m[1][1], 1.0f) && Near(left.m[2][2], 1.0f) && Near(left.m[0][1], 0.0f) &&
	          Near(left.m[1][3], 0.0f) && Near(left.m[2][3], 0.0f),
	      "not turned against the head, and at its height and depth");
	const openvr::HmdMatrix34 neither = EyeToHead(headset, 5);
	Check(Near(neither.m[0][3], 0.0f) && Near(neither.m[0][0], 1.0f), "an eye that is neither: the head itself");
}

void TestPoses() {
	std::printf("Poses\n");
	const openvr::TrackedDevicePose hmd = HmdPose();
	Check(!hmd.poseIsValid && hmd.deviceIsConnected && hmd.trackingResult == 101,
	      "the headset connected and asleep: valid=0 connected=1 result=101");
	Check(Near(hmd.deviceToAbsoluteTracking.m[0][0], 1.0f) && Near(hmd.deviceToAbsoluteTracking.m[1][3], 0.0f),
	      "its matrix the identity");
	openvr::TrackedDevicePose poses[3];
	std::memset(poses, 0x7F, sizeof(poses));
	FillPoses(poses, 3);
	Check(poses[0].deviceIsConnected && poses[0].trackingResult == 101, "the array's first entry is the headset");
	Check(!poses[1].deviceIsConnected && !poses[1].poseIsValid && poses[1].trackingResult == 1 &&
	          !poses[2].deviceIsConnected && poses[2].trackingResult == 1,
	      "nothing connected after it");
	std::memset(poses, 0x7F, sizeof(poses));
	FillPoses(poses, 0);
	Check(poses[0].trackingResult == 0x7F7F7F7F, "a count of none: nothing written");
	FillPoses(nullptr, 4);
	Check(true, "no array: nothing written");
	Check(DeviceClass(0) == kDeviceClassHmd && DeviceClass(1) == kDeviceClassInvalid &&
	          DeviceClass(openvr::kTrackedDeviceIndexInvalid) == kDeviceClassInvalid,
	      "device 0 is the headset, no other device is anything");
}

void TestProperties() {
	std::printf("Properties\n");
	const Headset headset = MakeHeadset(HeadsetKind::Beyond);
	int error = -1;
	Check(Near(FloatProperty(headset, 0, openvr::kPropDisplayFrequency, &error), 75.0f) && error == 0,
	      "the headset's display rate");
	Check(Near(FloatProperty(headset, 0, 2003, &error), 0.0f) && error == kPropErrorUnknownProperty,
	      "another float: unknown property");
	Check(Near(FloatProperty(headset, 3, openvr::kPropDisplayFrequency, &error), 0.0f) && error == kPropErrorInvalidDevice,
	      "another device: invalid device");
	Check(Near(FloatProperty(headset, 0, openvr::kPropDisplayFrequency, nullptr), 75.0f), "no place for the error: still answered");

	char text[64] = "untouched";
	UInt32 length = StringProperty(headset, 0, openvr::kPropModelNumber, text, sizeof(text), &error);
	Check(error == 0 && std::strcmp(text, "OBVR stand-in (Bigscreen Beyond)") == 0 && length == std::strlen(text) + 1,
	      "the model's name, its length with the end");
	length = StringProperty(headset, 0, openvr::kPropManufacturerName, text, sizeof(text), &error);
	Check(error == 0 && std::strcmp(text, "OBVR") == 0 && length == 5, "the maker's name");
	std::snprintf(text, sizeof(text), "untouched");
	length = StringProperty(headset, 0, openvr::kPropModelNumber, text, 4, &error);
	Check(error == kPropErrorBufferTooSmall && length == 33 && std::strcmp(text, "untouched") == 0,
	      "a buffer too small: the size needed, nothing written");
	length = StringProperty(headset, 0, openvr::kPropModelNumber, nullptr, 0, &error);
	Check(error == kPropErrorBufferTooSmall && length == 33, "no buffer: the size needed");
	length = StringProperty(headset, 0, openvr::kPropRenderModelName, text, sizeof(text), &error);
	Check(error == kPropErrorUnknownProperty && length == 0, "another text: unknown property");
	length = StringProperty(headset, 1, openvr::kPropModelNumber, text, sizeof(text), &error);
	Check(error == kPropErrorInvalidDevice && length == 0, "another device: invalid device");
	Check(StringProperty(headset, 0, openvr::kPropManufacturerName, text, sizeof(text), nullptr) == 5,
	      "no place for the error: still answered");
}

void TestFrameClock() {
	std::printf("The frame clock\n");
	const UInt64 frequency = 10000000;  // ticks a second
	const UInt64 period = frequency / 75;  // 133333
	Check(NextFrameTick(1000, 1000, frequency, 75.0f) == 1000 + period, "the first call: one period on");
	Check(NextFrameTick(1000, 1000 + period / 2, frequency, 75.0f) == 1000 + period, "half a period in: the same beat");
	const UInt64 late = NextFrameTick(1000, 1000 + period * 3 + 10, frequency, 75.0f);
	Check(late > 1000 + period * 3 + 10 && late <= 1000 + period * 4 + 1, "a slow frame: the next beat after now, beats skipped");
	const UInt64 onBeat = NextFrameTick(0, 2 * 1000000, 75000000, 75.0f);  // a period of exactly 1000000 ticks
	Check(onBeat == 3 * 1000000, "exactly on a beat: the one after");
	Check(NextFrameTick(1000, 5000, 0, 75.0f) == 5000, "no clock frequency: at once");
	Check(NextFrameTick(1000, 5000, frequency, 0.0f) == 5000, "no display rate: at once");
	Check(NextFrameTick(1000, 5000, frequency, -1.0f) == 5000, "a negative display rate: at once");
	Check(NextFrameTick(5000, 1000, frequency, 75.0f) == 1000, "a clock before its origin: at once");

	Check(SleepMsBefore(0, frequency / 100, frequency) == 9, "10 ms to go: 9 asleep, the last in yields");
	Check(SleepMsBefore(0, frequency / 1000 * 2, frequency) == 1, "2 ms to go: 1 asleep");
	Check(SleepMsBefore(0, frequency / 1000, frequency) == 0, "1 ms to go: yields alone");
	Check(SleepMsBefore(500, 500, frequency) == 0 && SleepMsBefore(600, 500, frequency) == 0, "at or past the beat: none");
	Check(SleepMsBefore(0, 500, 0) == 0, "no clock frequency: none");
}

void TestFrameTiming() {
	std::printf("Frame timing\n");
	openvr::CompositorFrameTiming timing{};
	timing.size = sizeof(openvr::CompositorFrameTiming);
	const Headset headset = MakeHeadset(HeadsetKind::Beyond);
	Check(Near(headset.runningStartMs, 3.0f), "the poses handed out 3 ms before the vsync");
	Check(!FillFrameTiming(headset, &timing, 0, 0, 1.0), "before the first frame: nothing");
	Check(!FillFrameTiming(headset, &timing, 1, 1, 1.0), "the frame before the first: nothing");
	Check(!FillFrameTiming(headset, nullptr, 0, 5, 1.0), "no struct: nothing");
	timing.size = 100;
	Check(!FillFrameTiming(headset, &timing, 0, 5, 1.0) && timing.frameIndex == 0,
	      "a struct of another size: nothing, untouched");
	timing.size = sizeof(openvr::CompositorFrameTiming);
	timing.numDroppedFrames = 7;
	Check(FillFrameTiming(headset, &timing, 1, 5, 12.5), "the frame before the last of five: answered");
	Check(timing.frameIndex == 4 && timing.numFramePresents == 1 && timing.numDroppedFrames == 0 &&
	          timing.numMisPresented == 0 && timing.size == sizeof(openvr::CompositorFrameTiming),
	      "frame 4, shown once, on time, nothing dropped");
	Check(timing.systemTimeInSeconds == 12.5 && Near(timing.newPosesReadyMs, -3.0f),
	      "its time the clock's; its poses ready 3 ms before its vsync, a running start");
	Check(Near(timing.waitGetPosesCalledMs, 0.0f) && Near(timing.totalRenderGpuMs, 0.0f),
	      "nothing measured beyond that");
	Check(timing.hmdPose.deviceIsConnected && !timing.hmdPose.poseIsValid, "drawn with the sleeping headset's pose");
}

void TestSubmit() {
	std::printf("Submit\n");
	int picture = 0;
	openvr::Texture texture{};
	texture.handle = &picture;
	Check(SubmitResult(&texture) == 0, "a texture with a handle: taken");
	texture.handle = nullptr;
	Check(SubmitResult(&texture) == kCompositorErrorRequestFailed, "a texture without a handle: refused");
	Check(SubmitResult(nullptr) == kCompositorErrorRequestFailed, "no texture: refused");
}

void TestOverlays() {
	std::printf("Overlays\n");
	Overlays overlays;
	openvr::VROverlayHandle hud = 0, crosshair = 0, again = 99;
	Check(CreateOverlay(overlays, "obvr.hud", &hud) == 0 && hud != 0, "the first overlay: a handle");
	Check(CreateOverlay(overlays, "obvr.crosshair", &crosshair) == 0 && crosshair != hud && crosshair != 0,
	      "the second: another handle");
	Check(CreateOverlay(overlays, "obvr.hud", &again) == kOverlayErrorKeyInUse && again == 99,
	      "the first key again: 17, key in use, no handle");
	Check(CreateOverlay(overlays, nullptr, &again) == kOverlayErrorInvalidParameter, "no key: invalid parameter");
	Check(CreateOverlay(overlays, "", &again) == kOverlayErrorInvalidParameter, "an empty key: invalid parameter");
	Check(CreateOverlay(overlays, "obvr.other", nullptr) == kOverlayErrorInvalidParameter && overlays.count == 2,
	      "no place for the handle: invalid parameter, nothing made");
	char longKey[kOverlayMaxKeyLength + 1];
	std::memset(longKey, 'k', sizeof(longKey));
	longKey[kOverlayMaxKeyLength] = 0;
	Check(CreateOverlay(overlays, longKey, &again) == kOverlayErrorKeyTooLong, "a key of 128 letters: too long");
	longKey[kOverlayMaxKeyLength - 1] = 0;
	Check(CreateOverlay(overlays, longKey, &again) == 0, "a key of 127 letters: made");

	Check(KnownOverlay(overlays, hud) == 0 && KnownOverlay(overlays, crosshair) == 0, "both known");
	Check(KnownOverlay(overlays, openvr::kOverlayHandleInvalid) == kOverlayErrorUnknownOverlay &&
	          KnownOverlay(overlays, 9999) == kOverlayErrorUnknownOverlay,
	      "the invalid handle and one never made: unknown overlay");
	Check(DestroyOverlay(overlays, hud) == 0 && KnownOverlay(overlays, hud) == kOverlayErrorUnknownOverlay,
	      "destroyed: unknown from then on");
	Check(KnownOverlay(overlays, crosshair) == 0 && KnownOverlay(overlays, again) == 0, "the others still known");
	Check(DestroyOverlay(overlays, hud) == kOverlayErrorUnknownOverlay, "destroyed twice: unknown overlay");
	openvr::VROverlayHandle reborn = 0;
	Check(CreateOverlay(overlays, "obvr.hud", &reborn) == 0 && reborn != hud && reborn != crosshair && reborn != again,
	      "its key free again, under a handle never used before");

	Overlays full;
	char key[16];
	bool allMade = true;
	for (UInt32 i = 0; i < Overlays::kCapacity; ++i) {
		std::snprintf(key, sizeof(key), "k%u", i);
		allMade = allMade && CreateOverlay(full, key, &again) == 0;
	}
	Check(allMade, "as many as there is room for");
	Check(CreateOverlay(full, "one.more", &again) == kOverlayErrorLimitExceeded, "one more: limit exceeded");
}

void TestActions() {
	std::printf("Actions\n");
	Actions actions;
	UInt64 set = 0, trigger = 0, same = 0, other = 0;
	Check(ActionHandle(actions, "/actions/obvr", &set) == 0 && set != 0, "the action set: a handle");
	Check(ActionHandle(actions, "/actions/obvr/in/right_trigger", &trigger) == 0 && trigger != 0 && trigger != set,
	      "an action: another handle");
	Check(ActionHandle(actions, "/actions/obvr/in/right_trigger", &same) == 0 && same == trigger && actions.count == 2,
	      "the same path: the same handle");
	Check(ActionHandle(actions, nullptr, &other) == kInputErrorInvalidParam &&
	          ActionHandle(actions, "", &other) == kInputErrorInvalidParam &&
	          ActionHandle(actions, "/actions/x", nullptr) == kInputErrorInvalidParam,
	      "no path, an empty one, no place for the handle: invalid parameter");
	char longPath[Actions::kPathLength + 1];
	std::memset(longPath, 'p', sizeof(longPath));
	longPath[Actions::kPathLength] = 0;
	Check(ActionHandle(actions, longPath, &other) == kInputErrorInvalidParam, "a path too long for the book: invalid parameter");
	Check(KnownAction(actions, set) && KnownAction(actions, trigger) && !KnownAction(actions, 0) && !KnownAction(actions, 3),
	      "the two known, 0 and a handle never given not");

	input::ActionSet active{};
	Check(UpdateActions(&active, sizeof(active), 1) == 0, "the action sets' state taken");
	Check(UpdateActions(nullptr, sizeof(active), 0) == 0, "none to take: fine");
	Check(UpdateActions(nullptr, sizeof(active), 1) == kInputErrorInvalidParam, "one promised, none given: invalid parameter");
	Check(UpdateActions(&active, 8, 1) == kInputErrorInvalidParam, "a set of another size: invalid parameter");

	input::DigitalData digital{};
	digital.active = true;
	digital.state = true;
	Check(ReadInactive(actions, trigger, &digital, sizeof(digital)) == 0 && !digital.active && !digital.state,
	      "a digital action: read, not active, not pressed");
	input::AnalogData analog{};
	analog.active = true;
	analog.x = 1.0f;
	Check(ReadInactive(actions, trigger, &analog, sizeof(analog)) == 0 && !analog.active && Near(analog.x, 0.0f),
	      "an analog action: read, not active, at rest");
	digital.active = true;
	Check(ReadInactive(actions, 0, &digital, sizeof(digital)) == kInputErrorInvalidHandle && digital.active,
	      "an action never asked for: invalid handle, the data untouched");
	Check(ReadInactive(actions, trigger, &digital, 4) == kInputErrorInvalidParam, "data of another size: invalid parameter");
	Check(ReadInactive<input::DigitalData>(actions, trigger, nullptr, sizeof(digital)) == kInputErrorInvalidParam,
	      "no place for the data: invalid parameter");

	Check(SkeletonResult(actions, trigger) == kInputErrorNoData, "a skeleton: no data, there is no hand");
	Check(SkeletonResult(actions, 77) == kInputErrorInvalidHandle, "a skeleton never asked for: invalid handle");
	Check(HapticResult(actions, trigger) == 0, "a pulse: taken");
	Check(HapticResult(actions, 0) == kInputErrorInvalidHandle, "a pulse on no action: invalid handle");

	Actions full;
	char path[32];
	bool allMade = true;
	for (UInt32 i = 0; i < Actions::kCapacity; ++i) {
		std::snprintf(path, sizeof(path), "/actions/a%u", i);
		allMade = allMade && ActionHandle(full, path, &other) == 0 && other == i + 1;
	}
	Check(allMade, "as many paths as there is room for, handles counting up");
	Check(ActionHandle(full, "/actions/one_more", &other) == kInputErrorMaxCapacityReached, "one more: capacity reached");
	Check(ActionHandle(full, "/actions/a5", &other) == 0 && other == 6, "a known path in a full book: still its handle");
}

void TestErrors() {
	std::printf("Error places\n");
	int error = 5;
	SetError(&error, 0);
	Check(error == 0, "written where there is a place");
	SetError(nullptr, 3);
	Check(true, "and not where there is none");
}

}  // namespace

int main() {
	TestInterfaces();
	TestProfiles();
	TestHeadsetNames();
	TestOverrides();
	TestHeadset();
	TestPoses();
	TestProperties();
	TestFrameClock();
	TestFrameTiming();
	TestSubmit();
	TestOverlays();
	TestActions();
	TestErrors();
	std::printf("%s\n", g_failures == 0 ? "all checks passed" : "CHECKS FAILED");
	return g_failures == 0 ? 0 : 1;
}
