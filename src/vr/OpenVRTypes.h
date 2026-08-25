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
	// Index 0, and the size every eye texture has to be created at. Asking
	// the headset rather than assuming is the difference between a picture
	// the compositor takes as it is and one it rescales every frame.
	void(__stdcall* GetRecommendedRenderTargetSize)(UInt32* width, UInt32* height);

	void* unused[11];

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

// -------------------------------------------------------------- Compositor
//
// Everything below is for writing to VR rather than reading from it, which
// OBVR does not do yet. It is declared ahead of the code that will use it
// because the values were read out of the header in one sitting, and a value
// read is worth more than a value remembered.
//
// The versions are a matched pair: the same openvr_capi.h that declares
// IVRSystem_026 - the interface OBVR is already talking to successfully in
// the game - declares IVRCompositor_029 twelve lines later.

// IVRCompositor_Version (openvr_capi.h, line 130)
constexpr const char* kIVRCompositorFnTableVersion = "FnTable:IVRCompositor_029";

// EVRApplicationType_VRApplication_Scene (openvr_capi.h, line 1234)
//
// The counterpart to kApplicationBackground above, and the two cannot both be
// right. Reading poses wants Background, so as not to take the scene away
// from whatever SteamVR is showing; submitting frames requires Scene, because
// taking the scene is the point. Submit answers anything else with
// kCompositorErrorIsNotSceneApplication, and WaitGetPoses does the same.
constexpr int kApplicationScene = 1;

// EVREye (openvr_capi.h)
constexpr int kEyeLeft = 0;
constexpr int kEyeRight = 1;

// ETextureType (openvr_capi.h)
//
// There is deliberately no D3D9 entry here, because there is none in the
// header: kTextureTypeDirectX is an ID3D11Texture, not an IDirect3DTexture9.
// That absence is the reason 0.1.0 needs a route out of Direct3D 9 at all -
// see section 13 of HANDOFF.md.
constexpr int kTextureTypeDirectX = 0;  // ID3D11Texture
constexpr int kTextureTypeVulkan = 2;   // VRVulkanTextureData_t*

// EColorSpace (openvr_capi.h)
constexpr int kColorSpaceAuto = 0;
constexpr int kColorSpaceGamma = 1;
constexpr int kColorSpaceLinear = 2;

// EVRSubmitFlags_Submit_Default (openvr_capi.h, line 819)
constexpr int kSubmitDefault = 0;

// EVRCompositorError (openvr_capi.h). Only the ones worth telling apart in a
// log: the rest are reported by number.
constexpr int kCompositorErrorNone = 0;
constexpr int kCompositorErrorDoNotHaveFocus = 101;
constexpr int kCompositorErrorIsNotSceneApplication = 103;
constexpr int kCompositorErrorTextureIsOnWrongDevice = 104;
constexpr int kCompositorErrorTextureUsesUnsupportedFormat = 105;

// Texture_t (openvr_capi.h, line 2138)
struct Texture {
	void* handle;
	int type;        // ETextureType
	int colorSpace;  // EColorSpace
};

// VRTextureBounds_t (openvr_capi.h, line 2145)
//
// Which part of the texture is one eye. Submitting both eyes from a single
// texture is a matter of handing over the same handle twice with different
// bounds.
struct VRTextureBounds {
	float uMin;
	float vMin;
	float uMax;
	float vMax;
};

// Same reasoning as for TrackedDevicePose: a size that does not match means
// reading a field at the wrong offset, and that should fail here rather than
// in a headset.
//
// Written as a sum rather than as a number on purpose. Texture_t holds a
// pointer, so its size is 12 bytes in the 32-bit DLL and 16 in the native
// 64-bit test build - and the tests do compile this header, through
// OpenVRBackend.cpp. A literal 12 would be correct for the thing that ships
// and would break the build that checks it. The sum says what is actually at
// risk, which is padding between the fields, and it says it on either
// architecture.
static_assert(sizeof(Texture) == sizeof(void*) + 2 * sizeof(int),
              "Texture_t must pack without padding");
static_assert(sizeof(VRTextureBounds) == 4 * sizeof(float),
              "VRTextureBounds_t must be four floats");

// Excerpt from VR_IVRCompositor_FnTable (openvr_capi.h, from line 3089), in
// order from the top:
//
//    0 SetTrackingSpace                  4 GetLastPoseForTrackedDeviceIndex
//    1 GetTrackingSpace                  5 GetSubmitTexture
//    2 WaitGetPoses                      6 Submit
//    3 GetLastPoses                      7 SubmitWithArrayIndex
//
// GetSubmitTexture at position 5 is this table's version of the
// ComputeDistortionSet trap in IVRSystem: it is absent from older listings,
// so anyone working from one lands on SubmitWithArrayIndex instead of Submit
// and pushes the wrong number of arguments.
//
// Untyped entries stay void*. Every function pointer is four bytes on x86, so
// the layout is right regardless.
struct IVRCompositorFnTable {
	void(__stdcall* SetTrackingSpace)(int origin);

	void* getTrackingSpace;

	// Blocks until it is time to render the next frame, and hands back the
	// poses to render it with. It is the compositor's clock, not OBVR's -
	// which is the awkward part, because the camera hook runs on Oblivion's.
	int(__stdcall* WaitGetPoses)(TrackedDevicePose* renderPoseArray,
	                             UInt32 renderPoseArrayCount,
	                             TrackedDevicePose* gamePoseArray,
	                             UInt32 gamePoseArrayCount);

	void* getLastPoses;
	void* getLastPoseForTrackedDeviceIndex;
	void* getSubmitTexture;

	int(__stdcall* Submit)(int eye, const Texture* texture, const VRTextureBounds* bounds,
	                       int submitFlags);
};

}  // namespace obvr::vr::openvr
