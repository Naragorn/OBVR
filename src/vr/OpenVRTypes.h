#pragma once

#include "core/Types.h"

// Minimal, binary-compatible replica of the parts of the OpenVR C interface
// that OBVR needs.
//
// The source is headers/openvr_capi.h from ValveSoftware/openvr, as of
// IVRSystem_026. Every value below was read from that file rather than taken
// second-hand: with a function pointer index, guessing amounts to a crash.
//
// Why not include openvr_capi.h itself: same reasoning as
// src/obse/PluginInterface.h. Out of a 3200-line header OBVR needs exactly
// one function, three structs and four constants. Replicating that stays
// small and keeps the SDK-free cross build free of foreign dependencies.
//
// CAUTION - two different calling conventions in the same interface:
//
//   * The DLL's global VR_* functions use VR_CALLTYPE, which on Windows is
//     __cdecl (openvr.h, line 2299).
//   * The function pointers inside the FnTable structs use
//     OPENVR_FNTABLE_CALLTYPE, which is __stdcall (openvr_capi.h, line 21).
//
// Mixing the two pops the wrong number of bytes off a 32-bit stack. The
// damage does not show up at the call site but as a crash somewhere later.

namespace obvr::vr::openvr {

// --------------------------------------------------------------- Constants

// k_unTrackedDeviceIndex_Hmd (openvr_capi.h, line 80)
constexpr UInt32 kTrackedDeviceIndexHmd = 0;

// k_unMaxTrackedDeviceCount (openvr_capi.h, line 81)
constexpr UInt32 kMaxTrackedDeviceCount = 64;

// EVRApplicationType_VRApplication_Background (openvr_capi.h, line 1236)
//
// Deliberately Background rather than Scene: OBVR only reads poses.
// As a scene application it would claim the compositor and take the scene
// away from whatever SteamVR is already showing.
constexpr int kApplicationBackground = 3;

// ETrackingUniverseOrigin_TrackingUniverseSeated (openvr_capi.h, line 525)
//
// Seated matches 3DoF without roomscale: the origin sits where the user sits,
// and OBVR works relative to its own recenter reference anyway.
constexpr int kTrackingUniverseSeated = 0;

// EVRInitError_VRInitError_None (openvr_capi.h, line 1284)
constexpr int kInitErrorNone = 0;

// ----------------------------------------------------------------- Structs

// HmdMatrix34_t (openvr_capi.h, line 2046): float m[3][4], row major. The
// left three columns hold the rotation, the fourth holds the position.
struct HmdMatrix34 {
	float m[3][4];
};

// HmdVector3_t (openvr_capi.h, line 2061)
struct HmdVector3 {
	float v[3];
};

// TrackedDevicePose_t (openvr_capi.h, line 2218)
struct TrackedDevicePose {
	HmdMatrix34 deviceToAbsoluteTracking;
	HmdVector3 velocity;
	HmdVector3 angularVelocity;
	int trackingResult;  // ETrackingResult, not evaluated here
	bool poseIsValid;
	bool deviceIsConnected;
};

// These sizes have to match exactly, otherwise OBVR reads the pose field at
// an offset. That would show up in the headset as a wild camera and be hard
// to attribute - so it should fail at compile time instead.
static_assert(sizeof(HmdMatrix34) == 48, "HmdMatrix34 must be 48 bytes");
static_assert(sizeof(HmdVector3) == 12, "HmdVector3 must be 12 bytes");
static_assert(sizeof(TrackedDevicePose) == 80, "TrackedDevicePose must be 80 bytes");

// ----------------------------------------------------------------- FnTable

// Excerpt from VR_IVRSystem_FnTable (openvr_capi.h, from line 2937).
//
// GetDeviceToAbsoluteTrackingPose sits at position 13 there, so index 12.
// The entries before it, in order:
//
//    0 GetRecommendedRenderTargetSize   6 GetTimeSinceLastVsync
//    1 GetProjectionMatrix              7 GetD3D9AdapterIndex
//    2 GetProjectionRaw                 8 GetDXGIOutputInfo
//    3 ComputeDistortion                9 GetOutputDevice
//    4 ComputeDistortionSet            10 IsDisplayOnDesktop
//    5 GetEyeToHeadTransform           11 SetDisplayVisibility
//
// ComputeDistortionSet at position 4 is the trap: older revisions of the
// interface do not have it, so anyone working from an outdated listing ends
// up one entry short.
//
// The leading entries are never called and stay untyped. Every function
// pointer is four bytes on x86, so the struct still has the right layout.
struct IVRSystemFnTable {
	void* unused[12];

	void(__stdcall* GetDeviceToAbsoluteTrackingPose)(int origin,
	                                                 float predictedSecondsFromNow,
	                                                 TrackedDevicePose* poseArray,
	                                                 UInt32 poseArrayCount);
};

// --------------------------------------------------------- DLL exports

// Names are undecorated because VR_CALLTYPE is __cdecl.
using VR_InitInternalFn = UInt32(__cdecl*)(int* error, int applicationType);
using VR_ShutdownInternalFn = void(__cdecl*)();
using VR_GetGenericInterfaceFn = void*(__cdecl*)(const char* interfaceVersion, int* error);
using VR_IsHmdPresentFn = bool(__cdecl*)();
using VR_IsRuntimeInstalledFn = bool(__cdecl*)();
using VR_GetVRInitErrorAsEnglishDescriptionFn = const char*(__cdecl*)(int error);

// The "FnTable:" prefix asks for the C flavour of the interface, meaning a
// struct of function pointers instead of a C++ object with a vtable. That is
// the safe route here: a hand-written vtable would depend on the ABI of
// whichever compiler builds OBVR, this struct does not.
constexpr const char* kIVRSystemFnTableVersion = "FnTable:IVRSystem_026";

}  // namespace obvr::vr::openvr
