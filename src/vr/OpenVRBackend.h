#pragma once

#include "core/Types.h"
#include "vr/OpenVRTypes.h"
#include "vr/HandInput.h"
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

// Keeps the heading AND the pitch, and takes out only the roll.
//
// For anything the recenter key is meant to bring in front of the wearer's
// eyes rather than square with the world: a flat picture, a film. LevelPose
// drops pitch deliberately, which is right for the world camera and wrong
// here - a wearer looking down when they press the key is asking for the
// picture down there. See the comment at the definition for the measurement
// that separated the two.
void LevelRollOnly(openvr::HmdMatrix34& pose);

// Where the HUD quad hangs when it is anchored in the room rather than to
// the head: the given pose, moved forward along its own facing by the given
// distance.
//
// Forward is the negative third column, the same convention LevelPose reads
// its heading from and the same one the head-relative transform used with
// m[2][3] = -distance. The rotation is carried over untouched, so a pose
// that was levelled before it got here stays level.
//
// Kept apart from the anchoring itself so the arithmetic can be checked
// without a headset: this is what decides where the wearer finds their HUD
// after turning away from it, and getting it wrong puts it behind them.
openvr::HmdMatrix34 OverlayPoseAhead(const openvr::HmdMatrix34& pose, float distanceMetres);

// Squared distance between two poses' positions, in metres squared. What the
// room anchor's self-heal compares: an anchor further from the head than a
// person leans is an anchor taken where the wearer no longer is.
float PoseDistanceSq(const openvr::HmdMatrix34& a, const openvr::HmdMatrix34& b);


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

	// The controller in one hand: its pose in the same seated space the head
	// is read in, converted the same way, plus the legacy button and axis
	// state from the same moment (GetControllerStateWithPose). False, with
	// out.valid false, when no controller holds that role or its pose is not
	// valid - the caller keeps its last reading or does nothing, as with the
	// head. The hand-tracked mode's only source of hands.
	bool ReadHand(bool rightHand, HandPose& out) const;

	// The tracked-device index of the controller in one hand, or
	// openvr::kTrackedDeviceIndexInvalid. What an overlay is hung on to ride
	// a wrist.
	UInt32 HandDeviceIndex(bool rightHand) const;

	// Hangs an overlay on any tracked device - a controller for the wrist
	// menus - with the given device-to-overlay transform. The HMD case above
	// is this with the HMD's index.
	int SetOverlayTransformDeviceRelative(openvr::VROverlayHandle handle, UInt32 deviceIndex,
	                                      const openvr::HmdMatrix34& deviceToOverlay) const;

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

	// ------------------------------------------------------------- Overlay
	//
	// The 2D layer's way into the room: a quad the compositor places and
	// reprojects itself, fed with the same Vulkan texture description the
	// eyes use. The interface is fetched on first use rather than at Start,
	// because most of what the backend does never needs it.
	//
	// All of these return the overlay error code - kOverlayErrorNone means
	// done - except CreateOverlay, which reports through its return value
	// because a failure there leaves nothing to hold an error against.

	// Makes an overlay and hands back its handle. key must be unique per
	// process; name is what SteamVR shows in its own UI.
	bool CreateOverlay(const char* key, const char* name, openvr::VROverlayHandle& handle);

	// Gives the overlay this frame's picture. vulkanData points at a
	// dxvk::VRVulkanTextureData, exactly as SubmitEye takes for
	// kTextureTypeVulkan - the header declares both to accept the same
	// Texture_t, Vulkan included.
	int SetOverlayTexture(openvr::VROverlayHandle handle, const void* vulkanData) const;

	// Hangs the overlay relative to the head: hmdToOverlay is where the quad
	// sits in the headset's own frame, typically a translation straight
	// ahead. The compositor then carries it with the head itself, every
	// frame, which is what makes a HUD read as attached to the wearer rather
	// than to the room.
	int SetOverlayTransformHmdRelative(openvr::VROverlayHandle handle,
	                                   const openvr::HmdMatrix34& hmdToOverlay) const;

	// The other anchoring: the quad is placed in the tracking space itself
	// and stays there while the head turns, so the wearer can look away from
	// it and back at it.
	//
	// The same tracking universe the poses are read in - seated - so a
	// transform built from a pose read here means in the room what it says.
	int SetOverlayTransformAbsolute(openvr::VROverlayHandle handle,
	                                const openvr::HmdMatrix34& trackingToOverlay) const;

	// The quad's width in the world, in metres. Height follows from the
	// texture's aspect ratio; there is no separate control for it.
	int SetOverlayWidthInMetres(openvr::VROverlayHandle handle, float metres) const;

	// Pixels from memory, RGBA, eight bits each: the picture for an overlay
	// small enough to draw on the CPU. The runtime copies the buffer.
	int SetOverlayRaw(openvr::VROverlayHandle handle, const void* rgba, UInt32 width,
	                  UInt32 height) const;

	// A tint over the overlay's texture, 0..1 per channel.
	int SetOverlayColor(openvr::VROverlayHandle handle, float red, float green,
	                    float blue) const;

	// The overlay's opacity, 0..1.
	int SetOverlayAlpha(openvr::VROverlayHandle handle, float alpha) const;

	// Which part of the texture the overlay shows. Without this the overlay
	// shows all of it - which is wrong the moment the texture is bigger than
	// the picture, as it is when the game lays its 2D into one corner of an
	// eye-sized frame. The overlay's on-screen aspect follows the bounds, so
	// cropping to a 16:9 corner also makes the quad 16:9 again.
	int SetOverlayTextureBounds(openvr::VROverlayHandle handle,
	                            const openvr::VRTextureBounds& bounds) const;

	int ShowOverlay(openvr::VROverlayHandle handle) const;
	int HideOverlay(openvr::VROverlayHandle handle) const;
	int DestroyOverlay(openvr::VROverlayHandle handle);

private:
	// Fetches the overlay interface if it has not been, and reports whether
	// it is usable. Said once in the log either way.
	bool EnsureOverlayInterface();
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
	void* m_overlay = nullptr;     // IVROverlayFnTable*, fetched on first use
	bool m_overlayTried = false;
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

	// The last answer WaitGetPoses gave, logged on change of answer: the
	// error code, and the HMD pose's validity, connection and tracking
	// result. This exists for the recenter key doing nothing during the
	// intro films (task #25): the render pose is unusable for the first
	// seconds and the flat picture rides the head until it is not, and
	// what the log lacked was WHY - no focus yet, a headset still settling,
	// or a compositor not ready. See OpenVR's WaitGetPoses documentation for
	// the codes: 101 is DoNotHaveFocus, 103 IsNotSceneApplication.
	int m_lastWaitResult = -1;
	int m_lastTrackingResult = -1;
	bool m_lastPoseValid = false;
	bool m_lastDeviceConnected = false;
	bool m_waitAnswerLogged = false;

	// The same pose as OpenVR gave it, unconverted. Submit wants the matrix
	// back in its own form, and converting to a quaternion and out again would
	// be two chances to introduce a difference in something whose whole purpose
	// is to be identical.
	openvr::HmdMatrix34 m_renderPoseMatrix{};
};

}  // namespace obvr::vr
