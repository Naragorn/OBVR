#pragma once

#include "render/EyeGeometry.h"
#include "render/EyeMirror.h"
#include "render/EyeTextures.h"
#include "render/GameDevice.h"
#include "render/GameFrame.h"
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

		// Whether the world was drawn twice this frame, once per eye, and both
		// pictures already captured through CaptureEye. The submit then has
		// nothing to copy: both eyes carry a picture drawn this frame, from
		// this frame's pose, and the one-frame disparity that defines
		// alternate eyes does not exist.
		bool dualEyes = false;

		// No camera ran for this frame - a menu, or a loading screen. The
		// picture is whatever the game drew, given to both eyes flat.
		//
		// Without this the headset shows nothing at all while the main menu is
		// up, so loading a game means taking it off. Which is a fail, and was
		// reported as one.
		bool flatFrame = false;

		// Send the last captured pair out again, unchanged.
		//
		// For a menu delivered in the world on a frame where Oblivion did not
		// redraw what is behind it. The alternative - falling back to the
		// cinema screen for those frames - is the menu flicker: the two
		// deliveries alternating at frame rate.
		//
		// Nothing is stale about it. The world is paused, so the picture is
		// still true; what has moved is the head, and the compositor corrects
		// for exactly that, given the pose the pair was drawn from. Which is
		// why this path hands one over rather than letting the compositor
		// assume this frame's.
		bool heldEyes = false;

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

		// Oblivion's own frustum, read off its NiCamera at the start of this
		// frame. Zero when it could not be read.
		//
		// This outranks both of the settings above and outranks the projection
		// matrix as well, because it is the frustum the engine composes its
		// matrices from rather than one of the things it composes.
		float cameraTanHalfWidth = 0.0f;
		float cameraTanHalfHeight = 0.0f;

		// The paused world's menu dressing: the sepia tone with its strength
		// folded in as alpha (0 means no shade), and whether each eye's picture
		// is trimmed to the window both eyes show. Held pairs may use both;
		// freshly rendered live menu stereo uses the shade but deliberately not
		// the held-only border. See MenuDressingForFrame and MenuShade.h.
		UInt32 menuShadeColor = 0;
		bool menuSingleBorder = false;

		// How large a flat picture is drawn, as a fraction of the world's
		// placement.
		float menuScale = 0.7f;

		// The shape a flat picture is given, width over height. 0 keeps the
		// frame's own.
		float menuAspect = 1.7778f;
	};

	// The frame in two halves, so the picture can be submitted after the game
	// has drawn it rather than before.
	//
	// BeginFrame waits on the compositor and returns whether a frame is owed;
	// EndFrame pays it. Between them Oblivion draws. Calling them from one
	// place, one after the other, is the old arrangement, where the picture
	// submitted is whatever the back buffer held from last time. That is what
	// Render.SubmitAtFrameEnd=0 still does.
	bool BeginFrame(vr::OpenVRBackend& backend);
	void EndFrame(const vr::OpenVRBackend& backend, const FrameRequest& request);

	// Copies the back buffer into one eye's own picture, mid-frame, between
	// the two render passes of a dual-pass frame. The scene render hook calls
	// this with the finished first-eye picture still in the back buffer -
	// before the game's 2D layer has drawn on it, which is why the captures
	// happen here and not at Present.
	//
	// False when the mirror cannot be built or the copy fails; EndFrame then
	// falls back to the mono picture rather than submitting one stale eye.
	bool CaptureEye(const FrameRequest& request, bool isLeft);


	// Throws away the textures and the run of failures. For a change of
	// configuration, or shutdown.
	void Reset();

	// Whether a picture is actually going to the headset right now.
	bool IsActive() const { return m_textures.IsReady() && !m_policy.HasStopped(); }

	// The frustum that would cover both eyes completely, as tangents of the
	// half-angles. False before the headset has been asked.
	//
	// The union of the two eyes rather than either one: a single picture shared
	// between them has to reach the outer edge of both, and the eyes look
	// outwards in opposite directions. Symmetric, because Oblivion's frustum is
	// and writing an asymmetric one would be a different change.
	bool GetHeadsetFrustum(float& tanHalfWidth, float& tanHalfHeight) const;

	// Forgets where a flat picture was anchored, so the next one is placed
	// wherever the head is then looking.
	//
	// This is what the recenter key has to reach during a video or a menu. The
	// key is polled from the camera hook, which does not run there - so an
	// intro film that started while the head was tilted stayed tilted, and the
	// only way to watch it was to look up at it.
	void ResetFlatAnchor() { m_flatPoseValid = false; }

	// Whether a captured pair is standing by that a held submit could send
	// again. False until the first dual frame has been captured, which is what
	// keeps a menu opened before then off the held path. See DeliverFrame.
	bool HasHeldEyes() const { return m_mirrorUsable && m_heldPoseValid; }

	// The monitor copy of the 2D layer; see EyeMirror::BlendLayerOntoBackBuffer.
	bool MirrorLayerToMonitor(void* gameDevice, void* layerTexture) {
		return m_mirror.BlendLayerOntoBackBuffer(gameDevice, layerTexture);
	}

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

	// Submits the two pictures CaptureEye filled this frame. No copy, no kept
	// poses and no bounds: both eyes were drawn from this frame's WaitGetPoses
	// pose, which is exactly the pose the compositor assumes, so there is
	// nothing to tell it.
	bool SubmitDualEyes(const vr::OpenVRBackend& backend, const FrameRequest& request,
	                    int& left, int& right);

	// Submits the pair already in the mirror, without capturing anything new.
	//
	// Unlike the dual submit this one does hand over a pose, because the
	// pictures were not drawn from this frame's: they were drawn from
	// m_heldPose, one or more frames ago, and the compositor reprojects them
	// for the difference. Refuses when there is nothing held to send, so a
	// menu opened before the first world render falls back rather than
	// submitting whatever the mirror happens to contain.
	bool SubmitHeldEyes(const vr::OpenVRBackend& backend, const FrameRequest& request,
	                    int& left, int& right);

	// Builds or rebuilds the eye copies for the current request. Shared by the
	// alternate-eye submit and the dual-pass capture, which need the same
	// pictures at different moments of the frame.
	void EnsureMirror(const FrameRequest& request);

	// Everything after the first failure to set up. Without it a machine that
	// cannot create the device would retry once per frame for ever, and the
	// log would be the only thing rendering.
	bool m_setupAttempted = false;
	// Whether WaitGetPoses last ran under DXVK's queue lock, and whether
	// that has been said yet - one line per change, not per frame.
	bool m_poseLockReported = false;
	bool m_poseLockEverReported = false;

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

	// The single-sample copy a multisampled back buffer is submitted through
	// on the mono path; see FrameResolve.
	FrameResolve m_resolve;
	bool m_mirrorChecked = false;
	bool m_mirrorUsable = false;

	// Whether the mirror was built from Oblivion's own camera frustum rather
	// than from a fallback.
	//
	// It matters because menus reach the headset before any camera has run, so
	// the first build can happen with no frustum to hand - and a mirror built
	// that way keeps a placement that was only ever a guess. When the real
	// frustum turns up, it is rebuilt.
	bool m_mirrorUsedCamera = false;
	bool m_copyFailureLogged = false;
	bool m_flatCopyFailureLogged = false;

	// Which eyes CaptureEye has filled since the last submit, index 0 left.
	// Cleared at BeginFrame so a frame that turns flat mid-way - a menu
	// opening between the render and Present - cannot leave stale captures
	// for a later dual submit to mistake for its own.
	bool m_dualCaptured[2] = {false, false};
	bool m_dualReported = false;

	// How many dual submits are still traced stage by stage. Diagnostic for a
	// run that lost the GPU on its first dual frames; see the trace in
	// SubmitDualEyes.
	UInt32 m_dualTraceLeft = 3;

	// The pose each eye's picture was actually drawn with, index 0 left.
	//
	// Under alternate eyes the two are a frame apart, and the compositor has no
	// way to know that unless it is told - it assumes both were drawn with the
	// pose from WaitGetPoses, which is right for one eye and out by a frame of
	// head motion for the other. That is the ghosting the GTA V VR mod's author
	// reported on ValveSoftware/openvr issue #1253, from the same technique.
	vr::openvr::HmdMatrix34 m_eyePose[2] = {};
	bool m_eyePoseValid[2] = {false, false};

	// The pose a flat picture is anchored to, held from when it first appeared.
	//
	// Held rather than refreshed, because a menu that is told it was drawn from
	// wherever the head is now gives the compositor nothing to correct - and an
	// image with nothing to correct rides the head instead of staying put.
	vr::openvr::HmdMatrix34 m_flatPose = {};
	bool m_flatPoseValid = false;

	// The last answer the anchor read gave, so a run reports the change rather
	// than the state. See the call site: a recenter that drops the anchor but
	// never gets a new one looks, from the headset, like a key that does
	// nothing at all - and a budget spent on repeats of "no pose" says that it
	// failed without ever saying when it stopped failing.
	bool m_flatAnchorLastAnswer = false;
	bool m_flatAnchorEverAnswered = false;

	// The pose the last dual pair was drawn from, for the held submit.
	//
	// Not levelled, unlike the flat one: the flat picture is a screen, and a
	// screen hanging at the angle the head happened to be at is wrong. This is
	// the world, and the world was drawn looking exactly where the pose says,
	// pitch included. Levelling it would tell the compositor the wearer had
	// been looking level when they were not, and it would correct for a head
	// movement that never happened.
	vr::openvr::HmdMatrix34 m_heldPose = {};
	bool m_heldPoseValid = false;
	bool m_heldReported = false;

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
