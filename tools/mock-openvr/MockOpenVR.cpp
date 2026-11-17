// openvr_mock.dll: a stand-in for openvr_api.dll with no SteamVR and no
// headset behind it, for the harness (docs/hand-script-harness.md, "Without
// SteamVR"). OBVR loads it when [Debug] OpenVRLibrary names it, which the
// runner's --no-steamvr writes into the run's OBVR-test.ini.
//
// Only the glue is here: the exports openvr_api.dll has, and the five
// function tables in the layout OBVR declares them (vr/OpenVRTypes.h,
// vr/ControllerActions.h - the same headers, so the two cannot drift apart).
// What each call answers is decided in MockRuntime.h. An entry OBVR declares
// as a plain pointer stays null: OBVR does not call it.
//
// The one thing done here and not there is waiting: WaitGetPoses holds the
// game to the headset's rate as the compositor does. Its beats are the
// moments the poses are handed out; the vsync each belongs to is the
// headset's running start later (MockRuntime.h, Headset::runningStartMs).
//
// Never part of a release (tools/package-release.* do not know it).

#include "MockRuntime.h"

#include "platform/Win32Min.h"

namespace {

using namespace obvr;
using namespace obvr::mockvr;

struct State {
	Headset headset;
	Overlays overlays;
	Actions actions;
	bool clockStarted = false;
	UInt64 origin = 0;
	UInt32 frames = 0;
};

State g_state;

UInt64 Now() { return static_cast<UInt64>(ReadPerformanceCounter()); }
UInt64 Frequency() { return static_cast<UInt64>(ReadPerformanceFrequency()); }

// ----------------------------------------------------------------- system

void __stdcall GetRecommendedRenderTargetSize(UInt32* width, UInt32* height) {
	if (width != nullptr) {
		*width = g_state.headset.eyeWidth;
	}
	if (height != nullptr) {
		*height = g_state.headset.eyeHeight;
	}
}

void __stdcall GetProjectionRaw(int eye, float* left, float* right, float* top, float* bottom) {
	ProjectionRaw(g_state.headset, eye, left, right, top, bottom);
}

openvr::HmdMatrix34 __stdcall GetEyeToHeadTransform(int eye) { return EyeToHead(g_state.headset, eye); }

void __stdcall GetDeviceToAbsoluteTrackingPose(int, float, openvr::TrackedDevicePose* poses, UInt32 count) {
	FillPoses(poses, count);
}

UInt32 __stdcall GetTrackedDeviceIndexForControllerRole(int) { return openvr::kTrackedDeviceIndexInvalid; }
int __stdcall GetControllerRoleForTrackedDeviceIndex(UInt32) { return kControllerRoleInvalid; }
int __stdcall GetTrackedDeviceClass(UInt32 device) { return DeviceClass(device); }
bool __stdcall IsTrackedDeviceConnected(UInt32 device) { return DeviceClass(device) == kDeviceClassHmd; }

float __stdcall GetFloatTrackedDeviceProperty(UInt32 device, int property, int* error) {
	return FloatProperty(g_state.headset, device, property, error);
}

UInt32 __stdcall GetStringTrackedDeviceProperty(UInt32 device, int property, char* value, UInt32 capacity,
                                                int* error) {
	return StringProperty(device, property, value, capacity, error);
}

bool __stdcall PollNextEvent(openvr::VREvent*, UInt32) { return false; }
bool __stdcall GetControllerState(UInt32, openvr::VRControllerState*, UInt32) { return false; }
bool __stdcall GetControllerStateWithPose(int, UInt32, openvr::VRControllerState*, UInt32,
                                          openvr::TrackedDevicePose*) {
	return false;
}
void __stdcall AcknowledgeQuit_Exiting() {}

openvr::IVRSystemFnTable* SystemTable() {
	static openvr::IVRSystemFnTable table{};
	table.GetRecommendedRenderTargetSize = GetRecommendedRenderTargetSize;
	table.GetProjectionRaw = GetProjectionRaw;
	table.GetEyeToHeadTransform = GetEyeToHeadTransform;
	table.GetDeviceToAbsoluteTrackingPose = GetDeviceToAbsoluteTrackingPose;
	table.GetTrackedDeviceIndexForControllerRole = GetTrackedDeviceIndexForControllerRole;
	table.GetControllerRoleForTrackedDeviceIndex = GetControllerRoleForTrackedDeviceIndex;
	table.GetTrackedDeviceClass = GetTrackedDeviceClass;
	table.IsTrackedDeviceConnected = IsTrackedDeviceConnected;
	table.GetFloatTrackedDeviceProperty = GetFloatTrackedDeviceProperty;
	table.GetStringTrackedDeviceProperty = GetStringTrackedDeviceProperty;
	table.PollNextEvent = PollNextEvent;
	table.GetControllerState = GetControllerState;
	table.GetControllerStateWithPose = GetControllerStateWithPose;
	table.AcknowledgeQuit_Exiting = AcknowledgeQuit_Exiting;
	return &table;
}

// ------------------------------------------------------------- compositor

void __stdcall SetTrackingSpace(int) {}

// Returns on the next beat of the headset's rate: most of the way asleep, the
// last millisecond in yields (MockRuntime.h, SleepMsBefore).
int __stdcall WaitGetPoses(openvr::TrackedDevicePose* renderPoses, UInt32 renderCount,
                           openvr::TrackedDevicePose* gamePoses, UInt32 gameCount) {
	const UInt64 frequency = Frequency();
	const UInt64 now = Now();
	if (!g_state.clockStarted) {
		g_state.clockStarted = true;
		g_state.origin = now;
	}
	const UInt64 target = NextFrameTick(g_state.origin, now, frequency, g_state.headset.displayHz);
	const UInt32 sleepMs = SleepMsBefore(now, target, frequency);
	if (sleepMs != 0) {
		Sleep(sleepMs);
	}
	while (Now() < target) {
		Sleep(0);
	}
	++g_state.frames;
	FillPoses(renderPoses, renderCount);
	FillPoses(gamePoses, gameCount);
	return openvr::kCompositorErrorNone;
}

int __stdcall Submit(int, const openvr::Texture* texture, const openvr::VRTextureBounds*, int) {
	return SubmitResult(texture);
}

void __stdcall ClearLastSubmittedFrame() {}

bool __stdcall GetFrameTiming(openvr::CompositorFrameTiming* timing, UInt32 framesAgo) {
	const UInt64 frequency = Frequency();
	const double seconds = frequency != 0 ? static_cast<double>(Now()) / static_cast<double>(frequency) : 0.0;
	return FillFrameTiming(g_state.headset, timing, framesAgo, g_state.frames, seconds);
}

void __stdcall FadeToColor(float, float, float, float, float, bool) {}
void __stdcall FadeGrid(float, bool) {}
// 0, as the compositor on Linux answered a second after a FadeGrid (OpenVRBackend.h, HoldScene).
float __stdcall GetCurrentGridAlpha() { return 0.0f; }
void __stdcall ShowMirrorWindow() {}
void __stdcall CompositorDumpImages() {}

int __stdcall SetStageOverride_Async(const char*, const openvr::HmdMatrix34*, const openvr::StageRenderSettings*,
                                     UInt32) {
	return openvr::kCompositorErrorNone;
}

void __stdcall ClearStageOverride() {}

openvr::IVRCompositorFnTable* CompositorTable() {
	static openvr::IVRCompositorFnTable table{};
	table.SetTrackingSpace = SetTrackingSpace;
	table.WaitGetPoses = WaitGetPoses;
	table.Submit = Submit;
	table.ClearLastSubmittedFrame = ClearLastSubmittedFrame;
	table.GetFrameTiming = GetFrameTiming;
	table.FadeToColor = FadeToColor;
	table.FadeGrid = FadeGrid;
	table.GetCurrentGridAlpha = GetCurrentGridAlpha;
	table.ShowMirrorWindow = ShowMirrorWindow;
	table.CompositorDumpImages = CompositorDumpImages;
	table.SetStageOverride_Async = SetStageOverride_Async;
	table.ClearStageOverride = ClearStageOverride;
	return &table;
}

// ---------------------------------------------------------------- overlay

int Known(openvr::VROverlayHandle handle) { return KnownOverlay(g_state.overlays, handle); }

int __stdcall CreateOverlayEntry(const char* key, const char*, openvr::VROverlayHandle* handle) {
	return CreateOverlay(g_state.overlays, key, handle);
}
int __stdcall DestroyOverlayEntry(openvr::VROverlayHandle handle) { return DestroyOverlay(g_state.overlays, handle); }
int __stdcall SetOverlayFlag(openvr::VROverlayHandle handle, int, bool) { return Known(handle); }
int __stdcall SetOverlayColor(openvr::VROverlayHandle handle, float, float, float) { return Known(handle); }
int __stdcall SetOverlayAlpha(openvr::VROverlayHandle handle, float) { return Known(handle); }
int __stdcall SetOverlaySortOrder(openvr::VROverlayHandle handle, UInt32) { return Known(handle); }
int __stdcall SetOverlayWidthInMeters(openvr::VROverlayHandle handle, float) { return Known(handle); }
int __stdcall SetOverlayCurvature(openvr::VROverlayHandle handle, float) { return Known(handle); }
int __stdcall SetOverlayTextureBounds(openvr::VROverlayHandle handle, const openvr::VRTextureBounds*) {
	return Known(handle);
}
int __stdcall SetOverlayTransformAbsolute(openvr::VROverlayHandle handle, int, const openvr::HmdMatrix34*) {
	return Known(handle);
}
int __stdcall SetOverlayTransformTrackedDeviceRelative(openvr::VROverlayHandle handle, UInt32,
                                                       const openvr::HmdMatrix34*) {
	return Known(handle);
}
int __stdcall ShowOverlay(openvr::VROverlayHandle handle) { return Known(handle); }
int __stdcall HideOverlay(openvr::VROverlayHandle handle) { return Known(handle); }
int __stdcall SetOverlayTexture(openvr::VROverlayHandle handle, const openvr::Texture*) { return Known(handle); }
int __stdcall ClearOverlayTexture(openvr::VROverlayHandle handle) { return Known(handle); }
int __stdcall SetOverlayRaw(openvr::VROverlayHandle handle, void*, UInt32, UInt32, UInt32) { return Known(handle); }

// SteamVR's own keyboard: there is none to show, so the request is refused
// and OBVR carries on as it does when the runtime refuses.
int __stdcall ShowKeyboard(int, int, UInt32, const char*, UInt32, const char*, UInt64) {
	return kOverlayErrorRequestFailed;
}
UInt32 __stdcall GetKeyboardText(char* text, UInt32 capacity) {
	if (text != nullptr && capacity != 0) {
		text[0] = 0;
	}
	return 0;
}
void __stdcall HideKeyboard() {}
void __stdcall SetKeyboardTransformAbsolute(int, const openvr::HmdMatrix34*) {}

openvr::IVROverlayFnTable* OverlayTable() {
	static openvr::IVROverlayFnTable table{};
	table.CreateOverlay = CreateOverlayEntry;
	table.DestroyOverlay = DestroyOverlayEntry;
	table.SetOverlayFlag = SetOverlayFlag;
	table.SetOverlayColor = SetOverlayColor;
	table.SetOverlayAlpha = SetOverlayAlpha;
	table.SetOverlaySortOrder = SetOverlaySortOrder;
	table.SetOverlayWidthInMeters = SetOverlayWidthInMeters;
	table.SetOverlayCurvature = SetOverlayCurvature;
	table.SetOverlayTextureBounds = SetOverlayTextureBounds;
	table.SetOverlayTransformAbsolute = SetOverlayTransformAbsolute;
	table.SetOverlayTransformTrackedDeviceRelative = SetOverlayTransformTrackedDeviceRelative;
	table.ShowOverlay = ShowOverlay;
	table.HideOverlay = HideOverlay;
	table.SetOverlayTexture = SetOverlayTexture;
	table.ClearOverlayTexture = ClearOverlayTexture;
	table.SetOverlayRaw = SetOverlayRaw;
	table.ShowKeyboard = ShowKeyboard;
	table.GetKeyboardText = GetKeyboardText;
	table.HideKeyboard = HideKeyboard;
	table.SetKeyboardTransformAbsolute = SetKeyboardTransformAbsolute;
	return &table;
}

// ------------------------------------------------------------------ input

int __stdcall SetManifest(const char* path) { return path != nullptr ? kInputErrorNone : kInputErrorInvalidParam; }
int __stdcall GetHandle(const char* path, UInt64* handle) { return ActionHandle(g_state.actions, path, handle); }
int __stdcall Update(input::ActionSet* sets, UInt32 size, UInt32 count) { return UpdateActions(sets, size, count); }
int __stdcall Digital(UInt64 action, input::DigitalData* data, UInt32 size, UInt64) {
	return ReadInactive(g_state.actions, action, data, size);
}
int __stdcall Analog(UInt64 action, input::AnalogData* data, UInt32 size, UInt64) {
	return ReadInactive(g_state.actions, action, data, size);
}
int __stdcall ReferenceTransforms(UInt64 action, int, int, input::BoneTransform*, UInt32) {
	return SkeletonResult(g_state.actions, action);
}
int __stdcall Bones(UInt64 action, int, int, input::BoneTransform*, UInt32) {
	return SkeletonResult(g_state.actions, action);
}
int __stdcall SkeletalSummary(UInt64 action, int, input::SkeletalSummary*) {
	return SkeletonResult(g_state.actions, action);
}
int __stdcall Haptic(UInt64 action, float, float, float, float, UInt64) {
	return HapticResult(g_state.actions, action);
}

input::Table* InputTable() {
	static input::Table table{};
	table.SetManifest = SetManifest;
	table.GetSet = GetHandle;
	table.GetAction = GetHandle;
	table.GetSource = GetHandle;
	table.Update = Update;
	table.Digital = Digital;
	table.Analog = Analog;
	table.ReferenceTransforms = ReferenceTransforms;
	table.Bones = Bones;
	table.SkeletalSummary = SkeletalSummary;
	table.Haptic = Haptic;
	return &table;
}

// ---------------------------------------------------------- render models

int __stdcall LoadRenderModel_Async(const char*, openvr::RenderModel** model) {
	if (model != nullptr) {
		*model = nullptr;
	}
	return kRenderModelErrorInvalidModel;
}
void __stdcall FreeRenderModel(openvr::RenderModel*) {}

openvr::IVRRenderModelsFnTable* RenderModelsTable() {
	static openvr::IVRRenderModelsFnTable table{};
	table.LoadRenderModel_Async = LoadRenderModel_Async;
	table.FreeRenderModel = FreeRenderModel;
	return &table;
}

void* TableOf(Interface kind) {
	switch (kind) {
		case Interface::System: return SystemTable();
		case Interface::Compositor: return CompositorTable();
		case Interface::Overlay: return OverlayTable();
		case Interface::Input: return InputTable();
		case Interface::RenderModels: return RenderModelsTable();
		case Interface::None: break;
	}
	return nullptr;
}

}  // namespace

// ---------------------------------------------------------------- exports
//
// The names and the calling convention of openvr_api.dll's own (vr/OpenVRTypes.h,
// VR_InitInternalFn and its neighbours).

extern "C" {

__declspec(dllexport) UInt32 __cdecl VR_InitInternal(int* error, int) {
	g_state = State{};
	SetError(error, openvr::kInitErrorNone);
	return 1;
}

__declspec(dllexport) void __cdecl VR_ShutdownInternal() { g_state = State{}; }

__declspec(dllexport) void* __cdecl VR_GetGenericInterface(const char* interfaceVersion, int* error) {
	void* const table = TableOf(InterfaceByName(interfaceVersion));
	SetError(error, table != nullptr ? openvr::kInitErrorNone : kInitErrorInterfaceNotFound);
	return table;
}

__declspec(dllexport) bool __cdecl VR_IsHmdPresent() { return true; }

__declspec(dllexport) bool __cdecl VR_IsRuntimeInstalled() { return true; }

__declspec(dllexport) const char* __cdecl VR_GetVRInitErrorAsEnglishDescription(int error) {
	return error == openvr::kInitErrorNone ? "No Error" : "the stand-in runtime has no such interface";
}

}  // extern "C"
