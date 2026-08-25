#pragma once

#include "core/Types.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

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
	// The render poses it fills are currently discarded. They are the
	// *predicted* poses for the frame about to be drawn, which is strictly
	// better than the unpredicted ones ReadHeadPose asks for separately - so
	// feeding the head tracker from here is an improvement worth making once
	// the frame loop is settled, rather than in the same change that
	// introduces it.
	int WaitGetPoses() const;

	// Hands one eye's texture to the compositor. Returns its error code.
	//
	// The texture must be an ID3D11Texture2D, because ETextureType has no
	// entry for Direct3D 9 and never has. bounds may be null, which means the
	// whole texture is that eye.
	int SubmitEye(int eye, void* texture) const;

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
};

}  // namespace obvr::vr
