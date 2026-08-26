#pragma once

#include "core/Types.h"
#include "vr/OpenVRTypes.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// Strips pitch and roll from a tracking pose, keeping heading and position.
//
// The same reasoning as vr::YawOnly and a different place to apply it: that
// one levels the camera's reference, this one levels the pose a flat picture
// is anchored to. Both were reported from the headset as the same complaint -
// press the key with the head tilted and the thing you were looking at is
// tilted from then on.
//
// A menu hanging at an angle is worse than a tilted world, not better: there
// is a horizon in the picture and a horizon in the inner ear, and they
// disagree with nothing to reconcile them.
//
// The matrix's columns are the axes in tracking space - right, up, backwards -
// and the fourth is the position. Rebuilding the first three from a levelled
// heading is what removes the tilt without going near an Euler angle, which
// has no answer when the head points straight up.
void LevelPose(openvr::HmdMatrix34& pose);


// Reads the head orientation from SteamVR through OpenVR.
//
// Why OpenVR and not OpenXR: Oblivion.exe is 32 bit, so OBVR.dll is too.
// openvr_api.dll has always shipped for x86, and OpenVR is out-of-process by
// design (vrclient against vrserver.exe) - Valve already built the bridge
// between 32 and 64 bit there. SteamVR's OpenXR only supports 32-bit
// applications from beta 2.17.2 onwards, and under Proton wineopenxr is not
// built for i386 at all.
//
// The library is loaded at runtime rather than linked. That is not a matter
// of taste: OBVR has to load cleanly on machines without SteamVR, otherwise
// starting Oblivion would fail for every user who does not own a headset. If
// any step fails, the vanilla camera stays in place and OBVR says so in the
// log.
class OpenVRBackend {
public:
	// Loads openvr_api.dll, registers OBVR with SteamVR and fetches the
	// system interface. Calling it more than once is harmless.
	//
	// wantScene decides which kind of application OBVR registers as, and the
	// two are mutually exclusive:
	//
	//   false - VRApplication_Background. Reads poses and leaves the
	//           compositor alone, so whatever SteamVR is showing keeps
	//           showing. This is what 0.0.1 to 0.0.4 do.
	//   true  - VRApplication_Scene. Required before a frame can be
	//           submitted: Submit answers a background application with
	//           VRCompositorError_IsNotSceneApplication and WaitGetPoses
	//           throttles itself to 10 Hz. It also means taking the scene
	//           away from whatever was using it.
	//
	// If the scene attempt gets as far as registering but the compositor
	// interface cannot be had, OBVR retreats to background rather than
	// failing: losing the picture is not a reason to lose head tracking too.
	// IsSceneApplication then reports false and the caller must not submit.
	//
	// Returns false when SteamVR cannot be reached - a normal state, not an
	// error.
	bool Start(bool wantScene);

	// Whether OBVR registered as a scene application and holds the
	// compositor. False means submitting is not possible, whatever the
	// configuration asked for.
	bool IsSceneApplication() const { return m_compositor != nullptr; }

	// Unregisters OBVR from SteamVR. Deliberately not called from DllMain:
	// the loader holds its lock there, and VR_ShutdownInternal loads
	// libraries of its own.
	void Stop();

	bool IsRunning() const { return m_system != nullptr; }

	// Current head pose in OpenVR convention (X right, Y up, -Z forward),
	// which is the same as OpenXR's. The caller still has to pass both parts
	// through the change of basis.
	//
	// The position is in metres and relative to the seated origin the user
	// set in SteamVR. That origin means nothing to OBVR - only the difference
	// against a stored reference does.
	//
	// Returns false while no valid pose is available - for instance because
	// tracking has not picked up yet. The caller should then keep the last
	// valid pose instead of letting the camera jump.
	bool ReadHeadPose(Quaternion& orientation, NiPoint3& position) const;

	// Blocks until the compositor wants the next frame, and returns its error
	// code - kCompositorErrorNone means go.
	//
	// This is what puts Oblivion on the compositor's clock, and that is
	// deliberate: it is how the picture stays in step with the headset. It is
	// also why the return value must be acted on rather than ignored. Without
	// focus the call throttles itself to 10 Hz, and a game thread blocking on
	// that runs at ten frames a second with nothing on screen to say why.
	// render::SubmitPolicy is what decides when to give up.
	//
	// The render poses it fills are kept, and using them is not an
	// optimisation - it is what the compositor assumes was done.
	//
	// From Valve, on ValveSoftware/openvr issue #518: "Reprojection
	// corrections are applied based on the poses returned by WaitGetPoses. We
	// assume you render the frames passed to Submit using the poses returned
	// by the previous WaitGetPoses. Rendering using other poses will result in
	// incorrect behavior. [...] We don't have any interface to allow the
	// application to specify which poses were used to render the scene
	// textures passed to Submit, and instead always assume the values returned
	// by WaitGetPoses were used."
	//
	// OBVR used to discard them and ask GetDeviceToAbsoluteTrackingPose for an
	// unpredicted pose instead. That was wrong twice over: the compositor
	// reprojected against a pose the picture had not been drawn with, and the
	// pose asked for was the head's position *now* rather than when the image
	// would be lit - about 25 ms earlier than it should have been. It was
	// reported from a headset as a world that morphed when the head moved, and
	// as motion sickness.
	int WaitGetPoses();

	// The pose the last WaitGetPoses handed back - the predicted pose for the
	// frame about to be drawn, and the one the compositor will reproject
	// against.
	//
	// False when there has been no WaitGetPoses this frame, or it reported no
	// valid pose. The caller then falls back to ReadHeadPose, which is what
	// happens when rendering is switched off entirely: head tracking still
	// works, it is simply less well timed.
	bool GetRenderPose(Quaternion& orientation, NiPoint3& position) const;

	// The same pose in OpenVR's own matrix form, for handing back to Submit.
	// False when there is none.
	bool GetRenderPoseMatrix(openvr::HmdMatrix34& out) const;

	// Hands one eye's texture to the compositor. Returns its error code.
	//
	// What the handle is depends on the type: an ID3D11Texture2D for
	// kTextureTypeDirectX, a pointer to a VRVulkanTextureData for
	// kTextureTypeVulkan. There is no entry for Direct3D 9 and never has been,
	// which is the whole reason the second of those exists here.
	//
	// bounds says which part of the texture belongs to this eye, or null for
	// all of it. Two eyes sharing one image need different bounds or they
	// disagree about where the picture is - see render::MonoBounds.
	// renderPose is the pose the picture was actually drawn with, or null to
	// let the compositor assume the one from WaitGetPoses. Under alternate
	// eyes the two eyes were drawn a frame apart, so assuming is wrong for one
	// of them by a whole frame of head motion - see kSubmitTextureWithPose.
	int SubmitEye(int eye, void* handle, int textureType,
	              const openvr::VRTextureBounds* bounds,
	              const openvr::HmdMatrix34* renderPose = nullptr) const;

	// The eye's frustum, as tangents of the angles from the view axis.
	//
	// Reported rather than interpreted: the sign convention is undocumented,
	// so OBVR hands the four numbers on as they arrive and the log is what
	// establishes what they mean on a real runtime.
	bool GetEyeProjection(int eye, float& left, float& right, float& top, float& bottom) const;

	// Where the eye sits relative to the head, in metres, still in OpenVR
	// convention. The two together give the distance between the eyes, which
	// is the number stereo rendering is built on.
	bool GetEyeOffset(int eye, NiPoint3& offsetMetres) const;

	// The size the headset wants each eye rendered at, in pixels.
	//
	// Worth asking rather than assuming: the compositor takes a texture of
	// this size as it is and rescales anything else, every frame, for as long
	// as the mod runs. Returns false without a connection, leaving the
	// arguments untouched.
	bool GetRecommendedRenderTargetSize(UInt32& width, UInt32& height) const;

private:
	// Logs a message once. The flag is diagnostic state rather than part of
	// the backend's state, which is why it is mutable and usable from a const
	// method.
	void LogOnce(bool& alreadyLogged, const char* message) const;

	// One attempt at registering with SteamVR as the given application type.
	// Leaves nothing behind on failure, which is what makes the retreat from
	// scene to background safe to attempt.
	bool Connect(int applicationType);

	void* m_module = nullptr;      // openvr_api.dll
	void* m_system = nullptr;      // IVRSystemFnTable*
	void* m_compositor = nullptr;  // IVRCompositorFnTable*, only when scene
	bool m_startAttempted = false;
	mutable bool m_loggedNoPose = false;

	// The pose from the last WaitGetPoses, and whether it was usable.
	//
	// Kept here rather than handed back through the return value because the
	// return value already carries the compositor error code, and the two
	// answers have different lifetimes: the error decides whether to keep
	// rendering at all, the pose is only good for this one frame.
	Quaternion m_renderOrientation;
	NiPoint3 m_renderPosition{0.0f, 0.0f, 0.0f};
	bool m_renderPoseValid = false;

	// The same pose as OpenVR gave it, unconverted. Submit wants the matrix
	// back in its own form, and converting to a quaternion and out again would
	// be two chances to introduce a difference in something whose whole purpose
	// is to be identical.
	openvr::HmdMatrix34 m_renderPoseMatrix{};
};

}  // namespace obvr::vr
