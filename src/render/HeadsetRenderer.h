#pragma once

#include "render/EyeGeometry.h"
#include "render/EyeMirror.h"
#include "render/EyeTextures.h"
#include "render/GameDevice.h"
#include "render/SubmitPolicy.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::render {

// Puts a picture in the headset, once per frame.
//
// Everything here happens on Oblivion's thread, from inside the camera hook,
// and that is the deliberate part rather than an accident of where the hook
// happens to be. WaitGetPoses blocks until the compositor wants the next
// frame, so calling it there puts the game on the compositor's clock - which
// is how the picture ends up in step with the world rather than a frame
// behind it, and it is the arrangement 0.1.0 needs, since the texture
// submitted then has to be the one the game has just drawn.
//
// One thing this arrangement is not yet: correctly placed. The camera hook
// runs while the camera is being computed, which is before the frame is
// rendered rather than after. For a generated picture that makes no
// difference - the pattern is the same every frame. For Oblivion's own
// pixels it will, and a second hook point at the end of the frame is what
// 0.1.0 needs. Recorded rather than worked around, because a picture that is
// one frame stale looks exactly like a picture that is correct.
class HeadsetRenderer {
public:
	// Creates what is missing and submits both eyes. Safe to call every
	// frame, and safe to call when there is no headset, no compositor, or no
	// rendering wanted - it does nothing and says nothing in all three cases.
	// What this frame is and where its picture should come from.
	//
	// A struct rather than four arguments: the last two only mean anything
	// together, and a call site that passed the wrong eye would compile.
	struct FrameRequest {
		// Oblivion's IDirect3DDevice9, or null. Only used when submitGameFrame
		// is on and the device turns out to be DXVK.
		void* gameDevice = nullptr;
		bool submitGameFrame = false;

		// Whether this frame belongs to one eye rather than both, and which.
		// The camera hook has already moved the camera to that eye - these two
		// have to agree, which is why both read camera::IsLeftEyeFrame rather
		// than deciding for themselves.
		bool alternateEyes = false;
		bool isLeftEye = true;

		// Whether the back buffer already holds this frame's picture.
		//
		// From the camera hook it does not: that runs before the frame is
		// drawn, so the back buffer still holds the previous one - drawn from
		// the previous camera position, which under alternate eyes is the other
		// eye. From the Present hook it does.
		//
		// This is the flag that decides which eye a copy belongs to, and getting
		// it backwards is not a subtle fault: it gives each eye the other eye's
		// viewpoint, which is stereo the eyes cannot fuse. It was exactly that,
		// once, and it read from the headset as a picture that would not hold
		// still.
		bool backBufferIsThisFrame = false;

		// Oblivion's own horizontal field of view, in degrees - fDefaultFOV out
		// of Oblivion.ini, 75 unless it has been changed. Needed because a
		// picture cannot be placed at the right angular size without knowing
		// what angle it covers.
		float gameFovDegrees = 75.0f;
		bool gameFovIsFor4x3 = false;
	};

	// The frame in two halves, so the picture can be submitted after the game
	// has drawn it rather than before.
	//
	// BeginFrame waits on the compositor and returns whether a frame is owed;
	// EndFrame pays it. Between them Oblivion draws. Calling them from one
	// place - which is what Update does - is the old arrangement, where the
	// picture submitted is whatever the back buffer held from last time.
	bool BeginFrame(const vr::OpenVRBackend& backend);
	void EndFrame(const vr::OpenVRBackend& backend, const FrameRequest& request);

	// Both halves at once, from the camera hook. One frame stale by
	// construction, and kept because it is the arrangement that is known to
	// work.
	void Update(const vr::OpenVRBackend& backend, const FrameRequest& request);

	// Throws away the textures and the run of failures. For a change of
	// configuration, or shutdown.
	void Reset();

	// Whether a picture is actually going to the headset right now.
	bool IsActive() const { return m_textures.IsReady() && !m_policy.HasStopped(); }

private:
	// The two ways Oblivion's own picture reaches the headset. Separate
	// methods rather than two arms of one if, because they differ in what
	// they own: mono borrows the back buffer and hands the same borrowed
	// image to both eyes, while alternate eyes copies it into pictures OBVR
	// keeps. Both return whether a game frame actually went out, and both
	// write into left and right.
	bool SubmitMono(const vr::OpenVRBackend& backend, const FrameRequest& request, int& left,
	                int& right);
	bool SubmitAlternateEyes(const vr::OpenVRBackend& backend, const FrameRequest& request,
	                         int& left, int& right);

	// Everything after the first failure to set up. Without it a machine that
	// cannot create the device would retry once per frame for ever, and the
	// log would be the only thing rendering.
	bool m_setupAttempted = false;

	// Whether BeginFrame has waited on the compositor and not yet been paid.
	// Submit is only meaningful after WaitGetPoses, and Present runs on frames
	// the camera hook never saw.
	bool m_frameOpen = false;

	EyeTextures m_textures;
	SubmitPolicy m_policy;

	// Read once, when the game frame is first wanted. The device does not
	// change kind within a run, and asking every frame would be a
	// QueryInterface per frame for an answer that cannot have changed.
	bool m_gameFrameChecked = false;
	bool m_gameFrameUsable = false;
	VulkanContext m_vulkan;

	// The two pictures OBVR owns, for alternate eyes. Created on the first
	// frame that wants them, because the back buffer has to exist before its
	// size and format can be copied.
	EyeMirror m_mirror;
	bool m_mirrorChecked = false;
	bool m_mirrorUsable = false;
	bool m_copyFailureLogged = false;

	// What the headset said about each eye, kept because the picture's place
	// inside the eye texture is computed from it - and because the eye copies
	// are made later than the geometry is read.
	EyeProjection m_leftEye;
	EyeProjection m_rightEye;
	UInt32 m_eyeWidth = 0;
	UInt32 m_eyeHeight = 0;

	// Which part of Oblivion's frame each eye is shown. Computed once from
	// the optical axes, because they cannot change within a run, and stored
	// in OpenVR's own layout so the submit needs no conversion per frame.
	vr::openvr::VRTextureBounds m_boundsLeft;
	vr::openvr::VRTextureBounds m_boundsRight;
};

}  // namespace obvr::render
