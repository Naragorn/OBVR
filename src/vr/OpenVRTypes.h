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

// VRControllerAxis_t and VRControllerState_t (openvr_capi.h). On Windows the
// state struct sits outside the header's pack(4) regions, so the two 64-bit
// masks take their natural eight-byte alignment: 4 bytes of packet number,
// 4 of padding, 16 of masks, five axes of 8 - 64 bytes, and that size is
// what GetControllerState is told.
struct VRControllerAxis {
	float x;
	float y;
};

struct VRControllerState {
	UInt32 packetNumber;
	UInt64 buttonPressed;
	UInt64 buttonTouched;
	VRControllerAxis axis[5];
};

static_assert(sizeof(VRControllerAxis) == 8, "VRControllerAxis_t is two floats");
static_assert(sizeof(VRControllerState) == 64,
              "VRControllerState_t is 64 bytes with natural alignment on Windows");

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

	void* getProjectionMatrix;

	// Index 2. The components of the eye's frustum, as tangents of the angles
	// from the view axis. Void with out-parameters, so it is safe to call in
	// the ordinary way.
	//
	// The sign convention is NOT documented in the header. Valve's wiki holds
	// that "top" and "bottom" are named backwards - that pfTop is the tangent
	// to the *bottom* plane and negative, pfBottom the tangent to the top and
	// positive - but that is a claim to check against a real runtime rather
	// than to build on. OBVR logs the four numbers as they arrive, and the
	// log settles it.
	void(__stdcall* GetProjectionRaw)(int eye, float* left, float* right, float* top,
	                                  float* bottom);

	void* computeDistortion;
	void* computeDistortionSet;

	// Index 5, and the awkward one: it returns HmdMatrix34_t **by value**.
	//
	// Forty-eight bytes do not fit in a register, so on x86 the caller passes
	// a hidden pointer to a return buffer as an implicit first argument, and
	// under __stdcall the callee pops it. Declaring the return type as the
	// struct lets the compiler generate exactly that, which is the whole
	// reason it is written this way rather than as a pointer out-parameter -
	// hand-rolling the hidden argument would be inventing an ABI instead of
	// using the one both sides already agree on. Getting it wrong does not
	// return a wrong matrix, it unbalances the stack.
	HmdMatrix34(__stdcall* GetEyeToHeadTransform)(int eye);

	void* unused[6];

	void(__stdcall* GetDeviceToAbsoluteTrackingPose)(int origin,
	                                                 float predictedSecondsFromNow,
	                                                 TrackedDevicePose* poseArray,
	                                                 UInt32 poseArrayCount);

	// Indices 13 to 17: ResetSeatedZeroPose, GetSeatedZeroPoseToStanding...,
	// GetRawZeroPoseToStanding..., GetSortedTrackedDeviceIndicesOfClass,
	// GetTrackedDeviceActivityLevel. Counted in openvr_capi.h's
	// VR_IVRSystem_FnTable (IVRSystem_026), from GetDeviceToAbsoluteTrackingPose
	// at 12 onwards, for the hand-tracked mode.
	void* unusedAfterPoses[5];

	// Index 18: which device holds a hand - kControllerRoleLeftHand or
	// RightHand - or kTrackedDeviceIndexInvalid when none does.
	UInt32(__stdcall* GetTrackedDeviceIndexForControllerRole)(int role);

	// Index 19 and 20, kept so the table stays honest about its layout.
	int(__stdcall* GetControllerRoleForTrackedDeviceIndex)(UInt32 deviceIndex);
	int(__stdcall* GetTrackedDeviceClass)(UInt32 deviceIndex);

	// Index 21.
	bool(__stdcall* IsTrackedDeviceConnected)(UInt32 deviceIndex);

	// Indices 22 to 36: the property getters, the event and hidden-area
	// queries. Fifteen entries.
	void* unusedBeforeControllerState[15];

	// Index 37 and 38: the legacy controller state - buttons and axes - and
	// the same together with the device's pose in one call, which is what a
	// hand wants: where it is and what it is pressing, from one moment.
	bool(__stdcall* GetControllerState)(UInt32 deviceIndex, VRControllerState* state,
	                                    UInt32 stateSize);
	bool(__stdcall* GetControllerStateWithPose)(int origin, UInt32 deviceIndex,
	                                            VRControllerState* state, UInt32 stateSize,
	                                            TrackedDevicePose* pose);
};

// ETrackedControllerRole (openvr_capi.h): which hand a controller is.
constexpr int kControllerRoleLeftHand = 1;
constexpr int kControllerRoleRightHand = 2;

// k_unTrackedDeviceIndexInvalid.
constexpr UInt32 kTrackedDeviceIndexInvalid = 0xFFFFFFFF;

// EVRButtonId, as bit positions in VRControllerState_t's masks:
// k_EButton_ApplicationMenu = 1, k_EButton_Grip = 2, k_EButton_A = 7,
// k_EButton_Axis0 = 32 (the touchpad or thumbstick),
// k_EButton_SteamVR_Trigger = 33 (the same value as Axis1).
constexpr UInt32 kButtonApplicationMenu = 1;
constexpr UInt32 kButtonGrip = 2;
constexpr UInt32 kButtonA = 7;
constexpr UInt32 kButtonAxis0 = 32;
constexpr UInt32 kButtonTrigger = 33;

// Which axis carries what, by the same numbering: rAxis[1] is the trigger's
// pull (x from 0 to 1), rAxis[0] the touchpad or thumbstick.
constexpr UInt32 kAxisThumb = 0;
constexpr UInt32 kAxisTrigger = 1;

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

// EVRSubmitFlags_Submit_TextureWithPose.
//
// Says that the texture pointer is a VRTextureWithPose rather than a plain
// Texture, and that the pose inside it - not the one from WaitGetPoses - is
// what the picture was drawn with.
//
// This is the flag alternate eye rendering cannot do without, and the reason
// is structural. Under AER the two eyes are drawn a frame apart, so they were
// drawn with two different poses; the compositor otherwise has one pose per
// frame and reprojects both eyes against it, which is right for one eye and
// out by a whole frame of head motion for the other. That shows as a picture
// that will not hold still when the head turns.
//
// The author of the GTA V VR mod, which uses the same technique, reported
// exactly this on ValveSoftware/openvr issue #1253: "The need for the poses to
// be distinct stems from the fact that the two eyes are rendered at different
// times." That issue also records a SteamVR bug where only the pose from the
// second Submit was honoured - and, further down, that it "has already been
// fixed with the lighthouse driver", which is what this headset tracks
// through. If the ghosting survives this change, that is where to look first.
constexpr int kSubmitTextureWithPose = 0x08;

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


// VRTextureWithPose_t (openvr.h): a Texture_t with the pose that was actually
// used to render it appended.
//
// Inheritance in the header, plain composition here, which is the same layout:
// the base class has no virtuals and no base of its own, so its fields simply
// come first. Writing it out avoids depending on a compiler's choices about
// empty-base and inheritance layout for something whose bytes have to match
// exactly.
struct VRTextureWithPose {
	Texture texture;
	HmdMatrix34 deviceToAbsoluteTracking;
};

static_assert(sizeof(VRTextureWithPose) == sizeof(Texture) + 48,
              "VRTextureWithPose_t is a Texture_t followed by a 3x4 matrix");

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

// ----------------------------------------------------------------- Overlay
//
// For the 2D layer: HUD, menus and videos redirected to a texture of OBVR's
// own and hung in the room, instead of being copied flat out of the back
// buffer. Declared ahead of the code that will use it, for the same reason
// the compositor table was: the values were read out of the header in one
// sitting, and a value read is worth more than a value remembered.
//
// The same openvr_capi.h that declares IVRSystem_026 and IVRCompositor_029 -
// both accepted by the runtime in the game, per the log line "connected as a
// scene application through FnTable:IVRSystem_026 and FnTable:IVRCompositor_029"
// - declares IVROverlay_028 five lines after the compositor.
//
// A warning that earned its place: an AI summary of this table listed the
// entries in a plausible order that was wrong. Every index below was counted
// out of the header text itself, entry 0 at line 3148, one entry per line,
// nothing skipped. A wrong index calls a different method with this method's
// arguments, and under __stdcall the callee pops what it expects, so the
// stack unbalances and the crash lands nowhere near the call.

// IVROverlay_Version (openvr_capi.h, line 135)
constexpr const char* kIVROverlayFnTableVersion = "FnTable:IVROverlay_028";

// VROverlayHandle_t (openvr_capi.h, line 2000): a 64-bit handle, passed by
// value - eight bytes on a 32-bit stack, which is why getting these
// signatures right matters twice over.
using VROverlayHandle = UInt64;

// k_ulOverlayHandleInvalid (openvr_capi.h, line 118)
constexpr VROverlayHandle kOverlayHandleInvalid = 0;

// EVROverlayError (openvr_capi.h, lines 1204ff). None is the one acted on;
// the rest are reported by number.
constexpr int kOverlayErrorNone = 0;

// Excerpt from VR_IVROverlay_FnTable (openvr_capi.h, struct at line 3146,
// entries from line 3148), in order from the top:
//
//    0 FindOverlay                      33 SetOverlayTransformAbsolute
//    1 CreateOverlay                    35 SetOverlayTransformTrackedDeviceRelative
//    3 DestroyOverlay                   43 ShowOverlay
//   11 SetOverlayFlag                   44 HideOverlay
//   14 SetOverlayColor                  60 SetOverlayTexture
//   16 SetOverlayAlpha                  61 ClearOverlayTexture
//   22 SetOverlayWidthInMeters          62 SetOverlayRaw
//   24 SetOverlayCurvature
//   30 SetOverlayTextureBounds
//
// (14 and 62 counted again on 2026-09-04 against the header as fetched from
// ValveSoftware/openvr master, IVROverlay_028: SetOverlayColor at line 3162,
// SetOverlayRaw at line 3210, right after ClearOverlayTexture.)
//
// The traps between them, for anyone recounting: CreateSubviewOverlay at 2
// and SetOverlayName at 6 are newer entries older listings lack;
// SetOverlayPreCurvePitch (26/27) and SetOverlayTransformCursor (39/40) sit
// mid-table; SetSubviewPosition at 42 comes right before ShowOverlay; and
// between HideOverlay and SetOverlayTexture lie fifteen input and cursor
// entries (45..59), so SetOverlayTexture is 60, not somewhere around 50.
//
// Untyped entries stay void*. Every function pointer is four bytes on x86,
// so the layout is right regardless.
struct IVROverlayFnTable {
	void* findOverlay;  // 0

	int(__stdcall* CreateOverlay)(const char* key, const char* name,
	                              VROverlayHandle* handle);  // 1

	void* createSubviewOverlay;  // 2

	int(__stdcall* DestroyOverlay)(VROverlayHandle handle);  // 3

	void* getOverlayKey;                // 4
	void* getOverlayName;               // 5
	void* setOverlayName;               // 6
	void* getOverlayImageData;          // 7
	void* getOverlayErrorNameFromEnum;  // 8
	void* setOverlayRenderingPid;       // 9
	void* getOverlayRenderingPid;       // 10

	int(__stdcall* SetOverlayFlag)(VROverlayHandle handle, int flag, bool enabled);  // 11

	void* getOverlayFlag;   // 12
	void* getOverlayFlags;  // 13

	// A tint over the texture (openvr_capi.h line 3162: red, green, blue).
	int(__stdcall* SetOverlayColor)(VROverlayHandle handle, float red, float green,
	                                float blue);  // 14

	void* getOverlayColor;  // 15

	int(__stdcall* SetOverlayAlpha)(VROverlayHandle handle, float alpha);  // 16

	void* getOverlayAlpha;        // 17
	void* setOverlayTexelAspect;  // 18
	void* getOverlayTexelAspect;  // 19
	void* setOverlaySortOrder;    // 20
	void* getOverlaySortOrder;    // 21

	int(__stdcall* SetOverlayWidthInMeters)(VROverlayHandle handle, float metres);  // 22

	void* getOverlayWidthInMeters;  // 23

	int(__stdcall* SetOverlayCurvature)(VROverlayHandle handle, float curvature);  // 24

	void* getOverlayCurvature;         // 25
	void* setOverlayPreCurvePitch;     // 26
	void* getOverlayPreCurvePitch;     // 27
	void* setOverlayTextureColorSpace; // 28
	void* getOverlayTextureColorSpace; // 29

	int(__stdcall* SetOverlayTextureBounds)(VROverlayHandle handle,
	                                        const VRTextureBounds* bounds);  // 30

	void* getOverlayTextureBounds;  // 31
	void* getOverlayTransformType;  // 32

	int(__stdcall* SetOverlayTransformAbsolute)(
		VROverlayHandle handle, int trackingOrigin,
		const HmdMatrix34* trackingOriginToOverlay);  // 33

	void* getOverlayTransformAbsolute;  // 34

	int(__stdcall* SetOverlayTransformTrackedDeviceRelative)(
		VROverlayHandle handle, UInt32 trackedDevice,
		const HmdMatrix34* trackedDeviceToOverlay);  // 35

	void* getOverlayTransformTrackedDeviceRelative;   // 36
	void* setOverlayTransformTrackedDeviceComponent;  // 37
	void* getOverlayTransformTrackedDeviceComponent;  // 38
	void* setOverlayTransformCursor;                  // 39
	void* getOverlayTransformCursor;                  // 40
	void* setOverlayTransformProjection;              // 41
	void* setSubviewPosition;                         // 42

	int(__stdcall* ShowOverlay)(VROverlayHandle handle);  // 43
	int(__stdcall* HideOverlay)(VROverlayHandle handle);  // 44

	void* isOverlayVisible;                    // 45
	void* getTransformForOverlayCoordinates;   // 46
	void* waitFrameSync;                       // 47
	void* pollNextOverlayEvent;                // 48
	void* getOverlayInputMethod;               // 49
	void* setOverlayInputMethod;               // 50
	void* getOverlayMouseScale;                // 51
	void* setOverlayMouseScale;                // 52
	void* computeOverlayIntersection;          // 53
	void* isHoverTargetOverlay;                // 54
	void* setOverlayIntersectionMask;          // 55
	void* triggerLaserMouseHapticVibration;    // 56
	void* setOverlayCursor;                    // 57
	void* setOverlayCursorPositionOverride;    // 58
	void* clearOverlayCursorPositionOverride;  // 59

	// The same Texture_t the compositor takes, TextureType_Vulkan included -
	// so the DXVK route that carries the eyes carries the overlay too.
	int(__stdcall* SetOverlayTexture)(VROverlayHandle handle, const Texture* texture);  // 60

	int(__stdcall* ClearOverlayTexture)(VROverlayHandle handle);  // 61

	// Pixels handed over from memory, no graphics API involved - for a
	// picture small enough to draw on the CPU, like a laser beam.
	// openvr_capi.h line 3210: buffer, width, height, bytes per pixel.
	int(__stdcall* SetOverlayRaw)(VROverlayHandle handle, void* buffer, UInt32 width,
	                              UInt32 height, UInt32 bytesPerPixel);  // 62
};

}  // namespace obvr::vr::openvr
