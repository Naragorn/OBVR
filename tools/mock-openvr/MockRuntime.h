#pragma once

// What the stand-in OpenVR runtime answers (MockOpenVR.cpp, openvr_mock.dll):
// the decisions, apart from the function tables and the clock, so that every
// flow runs in a unit test (tests/MockRuntimeTest.cpp).
//
// The stand-in is for the harness alone (docs/hand-script-harness.md, "Without
// SteamVR"). OBVR loads it in place of openvr_api.dll when [Debug]
// OpenVRLibrary names it, and then runs Full VR with no SteamVR and no headset
// behind it. It answers what SteamVR answered in the harness runs so far: a
// headset that is connected and asleep, and no controllers - the scenario's
// head and hands come from its hand script, as they always did.
//
// Two headsets can be stood in for, each with the numbers OBVR logged with
// the real one under SteamVR - the eye size SteamVR recommends, the display
// rate, both eyes' raw projection, their offsets from the head, and how long
// before a vsync the compositor handed the poses out:
//   - the Pimax Dream Air (the default): the tester's Windows game, OBVR.log
//     and OBVR-logs of 2026-11-15 and 2026-11-16;
//   - the Bigscreen Beyond: the harness run options-vr-buttons-game of
//     2026-11-16 15:54 on Linux.
// The eyes' rotation against the head is in neither log and is taken as none;
// the projections are the logs' four decimals. The rate, the eye size and the
// eye distance can be set over the headset's own (ConfiguredHeadset).
//
// The error codes are Valve's, read from openvr_capi.h (ValveSoftware/openvr,
// master, 2026-11-17). Which of them SteamVR picks in a refusal is known only
// where a comment says so; OBVR tests every answer against zero alone.

#include <cstdlib>
#include <cstring>

#include "core/Types.h"
#include "vr/ControllerActions.h"
#include "vr/OpenVRTypes.h"

namespace obvr::mockvr {

namespace openvr = obvr::vr::openvr;
namespace input = obvr::vr::input;

constexpr const char* kInputFnTableVersion = "FnTable:IVRInput_011";

constexpr int kInitErrorInterfaceNotFound = 105;      // VRInitError_Init_InterfaceNotFound
constexpr int kTrackingUninitialized = 1;             // TrackingResult_Uninitialized
constexpr int kTrackingCalibratingOutOfRange = 101;   // TrackingResult_Calibrating_OutOfRange
constexpr int kDeviceClassInvalid = 0;                // TrackedDeviceClass_Invalid
constexpr int kDeviceClassHmd = 1;                    // TrackedDeviceClass_HMD
constexpr int kControllerRoleInvalid = 0;             // TrackedControllerRole_Invalid
constexpr int kPropErrorSuccess = 0;                  // TrackedProp_Success
constexpr int kPropErrorBufferTooSmall = 3;           // TrackedProp_BufferTooSmall
constexpr int kPropErrorUnknownProperty = 4;          // TrackedProp_UnknownProperty
constexpr int kPropErrorInvalidDevice = 5;            // TrackedProp_InvalidDevice
constexpr int kCompositorErrorRequestFailed = 1;      // VRCompositorError_RequestFailed
constexpr int kOverlayErrorUnknownOverlay = 10;       // VROverlayError_UnknownOverlay
constexpr int kOverlayErrorLimitExceeded = 13;        // VROverlayError_OverlayLimitExceeded
constexpr int kOverlayErrorKeyTooLong = 15;           // VROverlayError_KeyTooLong
constexpr int kOverlayErrorKeyInUse = 17;             // VROverlayError_KeyInUse
constexpr int kOverlayErrorInvalidParameter = 20;     // VROverlayError_InvalidParameter
constexpr int kOverlayErrorRequestFailed = 23;        // VROverlayError_RequestFailed
constexpr int kInputErrorNone = 0;                    // VRInputError_None
constexpr int kInputErrorInvalidHandle = 3;           // VRInputError_InvalidHandle
constexpr int kInputErrorInvalidParam = 4;            // VRInputError_InvalidParam
constexpr int kInputErrorMaxCapacityReached = 6;      // VRInputError_MaxCapacityReached
constexpr int kInputErrorNoData = 13;                 // VRInputError_NoData
constexpr int kRenderModelErrorInvalidModel = 301;    // VRRenderModelError_InvalidModel
constexpr UInt32 kOverlayMaxKeyLength = 128;          // k_unVROverlayMaxKeyLength

// ------------------------------------------------------------- interfaces

enum class Interface { None, System, Compositor, Overlay, Input, RenderModels };

// The interface a VR_GetGenericInterface name asks for: the five function
// tables OBVR talks to, by the exact versions it names. Anything else - a
// newer version, the C++ flavour without "FnTable:" - is not there.
inline Interface InterfaceByName(const char* name) {
	if (name == nullptr) {
		return Interface::None;
	}
	struct Row {
		const char* name;
		Interface kind;
	};
	const Row rows[] = {
		{openvr::kIVRSystemFnTableVersion, Interface::System},
		{openvr::kIVRCompositorFnTableVersion, Interface::Compositor},
		{openvr::kIVROverlayFnTableVersion, Interface::Overlay},
		{kInputFnTableVersion, Interface::Input},
		{openvr::kIVRRenderModelsFnTableVersion, Interface::RenderModels},
	};
	for (const Row& row : rows) {
		if (std::strcmp(name, row.name) == 0) {
			return row.kind;
		}
	}
	return Interface::None;
}

// ---------------------------------------------------------------- headset

enum class HeadsetKind { DreamAir, Beyond };

constexpr HeadsetKind kDefaultHeadset = HeadsetKind::DreamAir;

struct Headset {
	const char* model = "";
	// GetRecommendedRenderTargetSize: what SteamVR asks the game to draw an
	// eye at, not the panel's pixels.
	UInt32 eyeWidth = 0;
	UInt32 eyeHeight = 0;
	float displayHz = 0.0f;
	// GetProjectionRaw per eye: left, right, top, bottom.
	float raw[2][4] = {};
	// Each eye's place right of the head's middle, metres.
	float eyeX[2] = {};
	// How long before the vsync the compositor hands the poses out. OBVR
	// holds its submit until past that vsync when it is a running start, and
	// does not when it is longer (render/SubmitPacing.h); the stand-in
	// reports the headset's own so that either runs as with SteamVR.
	float runningStartMs = 0.0f;
};

constexpr const char* kManufacturer = "OBVR";

inline void SetOptics(Headset& headset, const float left[4], const float right[4], float ipdMm) {
	for (int i = 0; i < 4; ++i) {
		headset.raw[0][i] = left[i];
		headset.raw[1][i] = right[i];
	}
	headset.eyeX[0] = -ipdMm / 2000.0f;
	headset.eyeX[1] = ipdMm / 2000.0f;
}

// A headset with its own numbers; `hz` over its own rate when positive.
//
// The Dream Air: SteamVR recommended 4036x3376 an eye in 27 of the tester's 29
// starts with it (4028x3380 in two); the panels are 3840x3552 (pimax.com,
// "Complete specs", read 2026-11-17), which is not what a game is asked to
// draw. It ran at 72 Hz in the latest logs and at 90 Hz in earlier ones, its
// IPD read 63.0 mm. Its compositor handed the poses out anywhere from 6 ms to
// a whole period before the vsync, most often a whole period (890 of 1707
// samples at 90 Hz read 11 ms): the stand-in reports a whole period, which is
// no running start, so OBVR holds nothing - as in most of those frames.
//
// The Beyond: 2568x2568 at 75 Hz, 62 mm, the poses 2.95 to 3.41 ms before the
// vsync in that run's "Submit pacing" lines - a running start OBVR holds for.
inline Headset MakeHeadset(HeadsetKind kind, float hz = 0.0f) {
	Headset headset;
	if (kind == HeadsetKind::Beyond) {
		const float left[4] = {-1.1516f, 0.8250f, -1.1646f, 0.8165f};
		const float right[4] = {-0.8419f, 1.1417f, -1.1572f, 0.8309f};
		headset.model = "OBVR stand-in (Bigscreen Beyond)";
		headset.eyeWidth = 2568;
		headset.eyeHeight = 2568;
		headset.displayHz = hz > 0.0f ? hz : 75.0f;
		headset.runningStartMs = 3.0f;
		SetOptics(headset, left, right, 62.0f);
		return headset;
	}
	const float left[4] = {-1.4019f, 1.2131f, -1.0913f, 1.0913f};
	const float right[4] = {-1.2131f, 1.4019f, -1.0913f, 1.0913f};
	headset.model = "OBVR stand-in (Pimax Dream Air)";
	headset.eyeWidth = 4036;
	headset.eyeHeight = 3376;
	headset.displayHz = hz > 0.0f ? hz : 72.0f;
	headset.runningStartMs = 1000.0f / headset.displayHz;
	SetOptics(headset, left, right, 63.0f);
	return headset;
}

inline char LowerAscii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

inline bool SameNoCase(const char* a, const char* b) {
	for (; *a != 0 && *b != 0; ++a, ++b) {
		if (LowerAscii(*a) != LowerAscii(*b)) {
			return false;
		}
	}
	return *a == *b;
}

// "dreamair" or "beyond", in any case; no name is the default. False, with
// the default, for a name that is neither.
inline bool HeadsetByName(const char* name, HeadsetKind& kind) {
	kind = kDefaultHeadset;
	if (name == nullptr || name[0] == 0 || SameNoCase(name, "dreamair")) {
		return true;
	}
	if (SameNoCase(name, "beyond")) {
		kind = HeadsetKind::Beyond;
		return true;
	}
	return false;
}

// A whole text as one number inside [low, high]; false for anything else.
inline bool ParseNumber(const char* text, double low, double high, double& value) {
	if (text == nullptr || text[0] == 0) {
		return false;
	}
	char* end = nullptr;
	const double read = std::strtod(text, &end);
	if (end == text || *end != 0 || !(read >= low && read <= high)) {
		return false;
	}
	value = read;
	return true;
}

// A display rate in Hz, 30 to 240.
inline bool ParseRate(const char* text, float& hz) {
	double value = 0.0;
	if (!ParseNumber(text, 30.0, 240.0, value)) {
		return false;
	}
	hz = static_cast<float>(value);
	return true;
}

// An eye's size as "<width>x<height>", each side 256 to 16384 pixels.
inline bool ParseEyeSize(const char* text, UInt32& width, UInt32& height) {
	if (text == nullptr) {
		return false;
	}
	char first[16] = {};
	size_t i = 0;
	for (; text[i] != 0 && text[i] != 'x' && text[i] != 'X' && i + 1 < sizeof(first); ++i) {
		first[i] = text[i];
	}
	if (text[i] != 'x' && text[i] != 'X') {
		return false;
	}
	double w = 0.0, h = 0.0;
	if (!ParseNumber(first, 256.0, 16384.0, w) || !ParseNumber(text + i + 1, 256.0, 16384.0, h)) {
		return false;
	}
	if (w != static_cast<double>(static_cast<UInt32>(w)) || h != static_cast<double>(static_cast<UInt32>(h))) {
		return false;
	}
	width = static_cast<UInt32>(w);
	height = static_cast<UInt32>(h);
	return true;
}

// The eyes' distance in millimetres, 50 to 80.
inline bool ParseIpdMm(const char* text, float& mm) {
	double value = 0.0;
	if (!ParseNumber(text, 50.0, 80.0, value)) {
		return false;
	}
	mm = static_cast<float>(value);
	return true;
}

// The headset the run asked for (OBVR_MOCK_HEADSET, OBVR_MOCK_HZ,
// OBVR_MOCK_EYE, OBVR_MOCK_IPD in the game's environment, set by the runner's
// --headset, --hz, --eye and --ipd). A value that cannot be read is left
// out, the headset's own standing; the runner refuses such values before the
// start and checks OBVR's log for the numbers after the run.
inline Headset ConfiguredHeadset(const char* name, const char* hz, const char* eye, const char* ipdMm) {
	HeadsetKind kind = kDefaultHeadset;
	HeadsetByName(name, kind);
	float rate = 0.0f;
	ParseRate(hz, rate);
	Headset headset = MakeHeadset(kind, rate);
	ParseEyeSize(eye, headset.eyeWidth, headset.eyeHeight);
	float mm = 0.0f;
	if (ParseIpdMm(ipdMm, mm)) {
		headset.eyeX[0] = -mm / 2000.0f;
		headset.eyeX[1] = mm / 2000.0f;
	}
	return headset;
}

inline bool KnownEye(int eye) { return eye == openvr::kEyeLeft || eye == openvr::kEyeRight; }

// An eye other than the two leaves the four numbers as they were, which OBVR
// reads as no answer (a frustum without width).
inline void ProjectionRaw(const Headset& headset, int eye, float* left, float* right, float* top, float* bottom) {
	if (!KnownEye(eye) || left == nullptr || right == nullptr || top == nullptr || bottom == nullptr) {
		return;
	}
	*left = headset.raw[eye][0];
	*right = headset.raw[eye][1];
	*top = headset.raw[eye][2];
	*bottom = headset.raw[eye][3];
}

inline openvr::HmdMatrix34 IdentityMatrix() {
	openvr::HmdMatrix34 matrix{};
	matrix.m[0][0] = 1.0f;
	matrix.m[1][1] = 1.0f;
	matrix.m[2][2] = 1.0f;
	return matrix;
}

inline openvr::HmdMatrix34 EyeToHead(const Headset& headset, int eye) {
	openvr::HmdMatrix34 matrix = IdentityMatrix();
	if (KnownEye(eye)) {
		matrix.m[0][3] = headset.eyeX[eye];
	}
	return matrix;
}

// The headset as the harness always had it: connected, lying asleep, so its
// pose is not valid ("HMD pose valid=0 connected=1 tracking result=101" in
// every run's log). OBVR then keeps its resting pose until the hand script's
// own head takes over.
inline openvr::TrackedDevicePose HmdPose() {
	openvr::TrackedDevicePose pose{};
	pose.deviceToAbsoluteTracking = IdentityMatrix();
	pose.trackingResult = kTrackingCalibratingOutOfRange;
	pose.poseIsValid = false;
	pose.deviceIsConnected = true;
	return pose;
}

// The array as GetDeviceToAbsoluteTrackingPose and WaitGetPoses fill it: the
// headset first, nothing connected after it.
inline void FillPoses(openvr::TrackedDevicePose* poses, UInt32 count) {
	if (poses == nullptr) {
		return;
	}
	for (UInt32 i = 0; i < count; ++i) {
		if (i == openvr::kTrackedDeviceIndexHmd) {
			poses[i] = HmdPose();
		} else {
			poses[i] = openvr::TrackedDevicePose{};
			poses[i].trackingResult = kTrackingUninitialized;
		}
	}
}

inline int DeviceClass(UInt32 device) {
	return device == openvr::kTrackedDeviceIndexHmd ? kDeviceClassHmd : kDeviceClassInvalid;
}

inline void SetError(int* error, int value) {
	if (error != nullptr) {
		*error = value;
	}
}

inline float FloatProperty(const Headset& headset, UInt32 device, int property, int* error) {
	if (device != openvr::kTrackedDeviceIndexHmd) {
		SetError(error, kPropErrorInvalidDevice);
		return 0.0f;
	}
	if (property != openvr::kPropDisplayFrequency) {
		SetError(error, kPropErrorUnknownProperty);
		return 0.0f;
	}
	SetError(error, kPropErrorSuccess);
	return headset.displayHz;
}

// Answers the bytes the text needs with its end, as OpenVR does - also when
// the buffer is too small for it, which is how a caller learns the size.
inline UInt32 StringProperty(const Headset& headset, UInt32 device, int property, char* value, UInt32 capacity,
                             int* error) {
	if (device != openvr::kTrackedDeviceIndexHmd) {
		SetError(error, kPropErrorInvalidDevice);
		return 0;
	}
	const char* text = property == openvr::kPropModelNumber        ? headset.model
	                   : property == openvr::kPropManufacturerName ? kManufacturer
	                                                               : nullptr;
	if (text == nullptr) {
		SetError(error, kPropErrorUnknownProperty);
		return 0;
	}
	const UInt32 needed = static_cast<UInt32>(std::strlen(text)) + 1;
	if (value == nullptr || capacity < needed) {
		SetError(error, kPropErrorBufferTooSmall);
		return needed;
	}
	std::memcpy(value, text, needed);
	SetError(error, kPropErrorSuccess);
	return needed;
}

// ------------------------------------------------------------ frame clock

// The tick WaitGetPoses returns at: the next beat of the headset's rate after
// `now`, the beats counted from `origin` (the first call). A game slower than
// the rate skips beats, as it does against the compositor; it is never handed
// two frames inside one period.
inline UInt64 NextFrameTick(UInt64 origin, UInt64 now, UInt64 frequency, float hz) {
	if (frequency == 0 || !(hz > 0.0f) || now < origin) {
		return now;
	}
	const double period = static_cast<double>(frequency) / static_cast<double>(hz);
	const UInt64 beat = static_cast<UInt64>(static_cast<double>(now - origin) / period) + 1;
	return origin + static_cast<UInt64>(static_cast<double>(beat) * period);
}

// The whole milliseconds to sleep on the way to `target`, one short of it: the
// last stretch is waited out in short yields, since a sleep may run long.
inline UInt32 SleepMsBefore(UInt64 now, UInt64 target, UInt64 frequency) {
	if (frequency == 0 || target <= now) {
		return 0;
	}
	const UInt64 ms = (target - now) * 1000 / frequency;
	return ms > 1 ? static_cast<UInt32>(ms - 1) : 0;
}

// GetFrameTiming: a frame handed out `framesAgo` calls back, shown once and on
// time, its poses ready the headset's running start before its vsync. Nothing
// before the first frame, and nothing into a struct of another size (OpenVR's
// own rule; a SteamVR of another version answers false there). Nothing else
// is measured: the GPU times and the other moments stay zero.
inline bool FillFrameTiming(const Headset& headset, openvr::CompositorFrameTiming* timing, UInt32 framesAgo,
                            UInt32 frames, double seconds) {
	if (timing == nullptr || timing->size != sizeof(openvr::CompositorFrameTiming) || framesAgo >= frames) {
		return false;
	}
	*timing = openvr::CompositorFrameTiming{};
	timing->size = sizeof(openvr::CompositorFrameTiming);
	timing->frameIndex = frames - framesAgo;
	timing->numFramePresents = 1;
	timing->systemTimeInSeconds = seconds;
	timing->newPosesReadyMs = -headset.runningStartMs;
	timing->hmdPose = HmdPose();
	return true;
}

// Submit: the picture goes nowhere. A texture without a handle is refused, so
// an eye that was never made still shows as a problem in the run's log.
inline int SubmitResult(const openvr::Texture* texture) {
	return texture != nullptr && texture->handle != nullptr ? openvr::kCompositorErrorNone
	                                                        : kCompositorErrorRequestFailed;
}

// --------------------------------------------------------------- overlays

// The overlays OBVR made: their keys, so a key made twice is refused as
// SteamVR refuses it (17, KeyInUse - seen in the game on 2026-10-24), and
// their handles, so a call on one that was never made or is gone answers an
// error instead of success.
struct Overlays {
	static constexpr UInt32 kCapacity = 128;
	struct Entry {
		openvr::VROverlayHandle handle = openvr::kOverlayHandleInvalid;
		char key[kOverlayMaxKeyLength] = {};
	};
	Entry entries[kCapacity] = {};
	UInt32 count = 0;
	openvr::VROverlayHandle nextHandle = 1;
};

inline int CreateOverlay(Overlays& overlays, const char* key, openvr::VROverlayHandle* handle) {
	if (key == nullptr || key[0] == 0 || handle == nullptr) {
		return kOverlayErrorInvalidParameter;
	}
	const size_t length = std::strlen(key);
	if (length >= kOverlayMaxKeyLength) {
		return kOverlayErrorKeyTooLong;
	}
	for (UInt32 i = 0; i < overlays.count; ++i) {
		if (std::strcmp(overlays.entries[i].key, key) == 0) {
			return kOverlayErrorKeyInUse;
		}
	}
	if (overlays.count >= Overlays::kCapacity) {
		return kOverlayErrorLimitExceeded;
	}
	Overlays::Entry& entry = overlays.entries[overlays.count++];
	entry.handle = overlays.nextHandle++;
	std::memcpy(entry.key, key, length + 1);
	*handle = entry.handle;
	return openvr::kOverlayErrorNone;
}

// What every call on an overlay answers first: whether there is one.
inline int KnownOverlay(const Overlays& overlays, openvr::VROverlayHandle handle) {
	for (UInt32 i = 0; i < overlays.count; ++i) {
		if (overlays.entries[i].handle == handle) {
			return openvr::kOverlayErrorNone;
		}
	}
	return kOverlayErrorUnknownOverlay;
}

inline int DestroyOverlay(Overlays& overlays, openvr::VROverlayHandle handle) {
	for (UInt32 i = 0; i < overlays.count; ++i) {
		if (overlays.entries[i].handle == handle) {
			overlays.entries[i] = overlays.entries[--overlays.count];
			overlays.entries[overlays.count] = Overlays::Entry{};
			return openvr::kOverlayErrorNone;
		}
	}
	return kOverlayErrorUnknownOverlay;
}

// ------------------------------------------------------------------ input

// The action manifest's names as OBVR asked for them: one handle a path, the
// same path the same handle (an action set, an action and an input source are
// all paths). Nothing is read from the manifest's file.
struct Actions {
	static constexpr UInt32 kCapacity = 96;
	static constexpr UInt32 kPathLength = 96;
	char paths[kCapacity][kPathLength] = {};
	UInt32 count = 0;
};

inline int ActionHandle(Actions& actions, const char* path, UInt64* handle) {
	if (path == nullptr || path[0] == 0 || handle == nullptr) {
		return kInputErrorInvalidParam;
	}
	const size_t length = std::strlen(path);
	if (length >= Actions::kPathLength) {
		return kInputErrorInvalidParam;
	}
	for (UInt32 i = 0; i < actions.count; ++i) {
		if (std::strcmp(actions.paths[i], path) == 0) {
			*handle = i + 1;
			return kInputErrorNone;
		}
	}
	if (actions.count >= Actions::kCapacity) {
		return kInputErrorMaxCapacityReached;
	}
	std::memcpy(actions.paths[actions.count], path, length + 1);
	*handle = ++actions.count;
	return kInputErrorNone;
}

inline bool KnownAction(const Actions& actions, UInt64 handle) { return handle >= 1 && handle <= actions.count; }

// UpdateActionState: the sets' state taken; there is none to take.
inline int UpdateActions(const input::ActionSet* sets, UInt32 size, UInt32 count) {
	if (size != sizeof(input::ActionSet) || (sets == nullptr && count != 0)) {
		return kInputErrorInvalidParam;
	}
	return kInputErrorNone;
}

// A digital or an analog action with no controller behind it: read without
// an error and not active, which OBVR takes as nothing pressed.
template <typename Data>
int ReadInactive(const Actions& actions, UInt64 action, Data* data, UInt32 size) {
	if (data == nullptr || size != sizeof(Data)) {
		return kInputErrorInvalidParam;
	}
	if (!KnownAction(actions, action)) {
		return kInputErrorInvalidHandle;
	}
	*data = Data{};
	return kInputErrorNone;
}

// A hand's skeleton, and a pulse on it: no hand, so no bones; the pulse is
// taken and felt by nobody. Which error SteamVR gives for the skeleton of a
// controller that is off is not known here - OBVR asks only with one on.
inline int SkeletonResult(const Actions& actions, UInt64 action) {
	return KnownAction(actions, action) ? kInputErrorNoData : kInputErrorInvalidHandle;
}

inline int HapticResult(const Actions& actions, UInt64 action) {
	return KnownAction(actions, action) ? kInputErrorNone : kInputErrorInvalidHandle;
}

}  // namespace obvr::mockvr
