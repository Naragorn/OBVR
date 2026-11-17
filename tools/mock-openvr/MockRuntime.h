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
// The headset's numbers are the ones OBVR logged in the harness run
// options-vr-buttons-game of 2026-11-16 15:54 on Linux (a Bigscreen Beyond
// under SteamVR): the eye size, the display rate, both eyes' raw projection,
// their offsets from the head, and how long before a vsync the compositor
// handed the poses out. The eyes' rotation against the head is not in that
// log and is taken as none; the projection is the log's four decimals.
//
// The error codes are Valve's, read from openvr_capi.h (ValveSoftware/openvr,
// master, 2026-11-17). Which of them SteamVR picks in a refusal is known only
// where a comment says so; OBVR tests every answer against zero alone.

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

struct Headset {
	UInt32 eyeWidth = 2568;
	UInt32 eyeHeight = 2568;
	float displayHz = 75.0f;
	// GetProjectionRaw per eye: left, right, top, bottom.
	float raw[2][4] = {{-1.1516f, 0.8250f, -1.1646f, 0.8165f}, {-0.8419f, 1.1417f, -1.1572f, 0.8309f}};
	// Each eye's place right of the head's middle, metres (an IPD of 62 mm).
	float eyeX[2] = {-0.0310f, 0.0310f};
	// The compositor's running start: the poses handed out this long before
	// the vsync (2.95 to 3.41 ms in that run's "Submit pacing" lines). OBVR
	// holds its submit until past that vsync (render/SubmitPacing.h), so the
	// stand-in reports one for that hold to run as it does with SteamVR.
	float runningStartMs = 3.0f;
};

constexpr const char* kModelNumber = "OBVR stand-in headset";
constexpr const char* kManufacturer = "OBVR";

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
inline UInt32 StringProperty(UInt32 device, int property, char* value, UInt32 capacity, int* error) {
	if (device != openvr::kTrackedDeviceIndexHmd) {
		SetError(error, kPropErrorInvalidDevice);
		return 0;
	}
	const char* text = property == openvr::kPropModelNumber        ? kModelNumber
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
