#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/OpenVRBackend.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// Supplies the head rotation that is laid onto the vanilla camera.
//
// The source is interchangeable because bitness is the problem here:
// Oblivion.exe is 32 bit, so OBVR.dll is too. OpenVR has always supported
// 32-bit applications, OpenXR only on some runtimes - SteamVR for instance
// only from beta 2.17.2, so not on the stable branch yet. Under Proton
// OpenXR drops out entirely, because wineopenxr is only built for 64 bit
// there.
//
// Hence several sources instead of one fixed binding. The simulated head also
// makes it possible to check the whole chain from quaternion to camera matrix
// in the running game without a headset.
//
// Every source delivers its orientation in OpenXR convention (Y up,
// -Z forward), the OpenVR backend included. The conversion to Oblivion
// happens in exactly one place, so that every source takes the same route.
enum class TrackerSource {
	None,       // no additional rotation
	Fixed,      // fixed test angles from the configuration
	Simulated,  // slow head movement, for checking without an HMD
	OpenVR,     // real headset through SteamVR; covers 32 bit reliably
	OpenXR,     // real headset through OpenXR; only with a 32-bit capable runtime
};

enum class StereoMode {
	// One image to both eyes. Oblivion is in the headset, and flat: there is
	// only one picture, so there is nothing between the eyes to see depth in.
	None,

	// Alternate eye rendering. The camera steps to one eye, the frame is drawn
	// once from there, and both eyes are submitted - the drawn one with this
	// frame's picture, the other with its own last one, kept in a copy OBVR
	// owns. Both every frame is not an optimisation: the compositor counts a
	// frame as delivered only when both eyes have arrived.
	AlternateEyes,

	// Drawing the world twice per tick, once per eye. The honest way, and the
	// only one without a time disparity between the eyes. Not built: it needs
	// Gamebryo to render twice without advancing the simulation twice, and
	// whether it will is an open question rather than a matter of wiring.
	DualPass,
};

struct TrackerSettings {
	TrackerSource source = TrackerSource::Fixed;

	// For TrackerSource::Fixed. Degrees, in Oblivion axes.
	float fixedPitch = 0.0f;
	float fixedRoll = 0.0f;
	float fixedYaw = 0.0f;

	// For TrackerSource::Simulated. Degrees, and period length in frames.
	float simulatedYawAmplitude = 25.0f;
	float simulatedPitchAmplitude = 12.0f;
	UInt32 simulatedPeriodFrames = 600;

	// Whether head movement through space reaches the camera as well, not
	// only head rotation. Only TrackerSource::OpenVR delivers a position; the
	// other sources leave the camera where the game put it.
	bool positionalTracking = true;

	// Whether OBVR renders to the headset rather than only reading poses from
	// it.
	//
	// False here and 1 in the shipped INI, and the two do not disagree: this
	// value applies when there is no INI at all, which means a broken
	// installation rather than a configured one. Claiming the VR scene from
	// one of those turns the headset black with no file present to say why.
	//
	// It decides which kind of application OBVR registers as, and the two are
	// exclusive: submitting a frame requires VRApplication_Scene, which takes
	// the headset away from whatever else was showing something. Reading
	// poses wants VRApplication_Background, which does not.
	bool renderToHeadset = false;

	// Whether the picture in the headset is Oblivion's own frame rather than
	// the generated test pattern. Needs DXVK, and falls back to the pattern
	// with a line in the log if that is missing.
	//
	// Off until it has been seen to work. The pattern is what proved the
	// compositor path, and keeping it reachable is what makes a fault in the
	// game frame path attributable rather than merely visible.
	bool submitGameFrame = false;

	// How the two eyes are given different views of the world.
	//
	// None: one image, both eyes, no depth between them. What 0.1.0 reached
	// first, and still the honest fallback.
	//
	// AlternateEyes: the camera is offset to one eye per frame and the frame
	// goes to that eye alone, alternating. Depth without drawing the world
	// twice, at the price of the two eyes seeing pictures a frame apart -
	// which is a disparity in time rather than distance, and shows on
	// anything moving quickly.
	//
	// The offset uses unitsPerMetre and NOT EffectiveUnitsPerMetre. The
	// distance between two eyes is a fact about a head, not a matter of taste,
	// and scaling it would change the apparent size of the world rather than
	// how far leaning moves. Keeping those two apart is why unitsPerMetre was
	// never allowed to absorb movementScale.
	StereoMode stereo = StereoMode::None;

	// Oblivion's own horizontal field of view, in degrees. fDefaultFOV out of
	// Oblivion.ini, which is 75 unless it has been changed.
	//
	// Read from OBVR's own file rather than the game's, because the console
	// command "fov" overrides the setting at runtime and OBVR has no way to
	// see that it did. A figure that disagrees with the game shows up as a
	// world that is the wrong size, so the log prints what was used.
	//
	// It is needed because the game's picture has to be laid into the eye's
	// view at the angle it actually covers. Without it the world is magnified
	// by whatever ratio two unrelated frustums happen to have - measured at
	// 1.61 times across and 2.31 times down on the headset this was built
	// against, which is both far too large and visibly stretched.
	float gameFovDegrees = 75.0f;

	// How to read that number when the frame is not 4:3. See PlacePicture: the
	// two readings differ by a third, evenly in both axes, so the wrong one
	// makes the world too large or too small with no stretching to show it.
	bool gameFovIsFor4x3 = false;

	// Whether the picture is handed to the compositor after Oblivion has drawn
	// it rather than before. Off is every version up to now: one whole frame of
	// latency, which in a headset is felt rather than seen.
	bool submitAtFrameEnd = false;

	// Whether menus and loading screens reach the headset. They have no camera,
	// so they arrive flat - but the alternative is a headset showing nothing
	// while the main menu is up.
	bool showMenus = true;

	// Overrides Oblivion's own field of view, in degrees. 0 leaves it alone.
	//
	// Written into the camera's frustum rather than into fDefaultFOV, because
	// that setting also resizes the menu layer and leaves mouse clicks landing
	// somewhere other than where the buttons appear.
	float gameFovOverride = 0.0f;

	// Whether the game is made to render the headset's own field of view rather
	// than an angle. Outranks gameFovOverride.
	//
	// This is the answer to "why 16:9 at all" - nothing forces it once OBVR
	// writes the frustum, and the eye is nearly square. What it does not fix by
	// itself is the frame's shape: 2560x1440 pixels spread over a square view
	// are stretched vertically, so iSize W and H in Oblivion.ini want to be
	// closer to equal for the resolution to follow the geometry.
	bool matchHeadsetFov = false;

	// Oblivion's own render resolution, written into its INI at startup and
	// picked up on the next run. 0 means the size the headset asks for.
	//
	// It is here because a headset wants a shape a monitor never does. OBVR can
	// write the frustum every frame, but not the frame's shape - Direct3D fixes
	// that when the device is made, long before any of this runs.
	UInt32 renderWidth = 0;
	UInt32 renderHeight = 0;
	bool setRenderSize = false;

	// How large a flat picture - menu, video, loading screen - is drawn, as a
	// fraction of the world's own placement. Below 1 puts it at arm's length
	// with black around it instead of pressed against the face.
	float menuScale = 0.7f;

	// Oblivion units per metre, for converting the head offset.
	//
	// The Construction Set wiki gives "21.3 units to a foot ... 64 units per
	// yard [~70 units per metre]", the Creation Kit wiki the exact engine
	// figure hk * 69.99125 = wu with 1 unit = 1.428 cm. Both agree.
	//
	// It is configurable regardless, because the number that matters is not
	// the documented one but the one that makes a real head feel right in a
	// real headset - and that can only be found by trying it.
	float unitsPerMetre = 69.99125f;

	// How much further than life the head is allowed to move. 1 is one to one,
	// 2 moves the camera twice as far as the head really went.
	//
	// A separate number rather than an inflated unitsPerMetre, and the reason
	// is not tidiness: unitsPerMetre is the engine's documented figure and
	// stereo rendering will need it for the eye separation. Overloading it
	// with taste now would silently double the eye separation later, and the
	// world would look like a model railway with no obvious cause.
	//
	// Above 1 the world moves further than the head that moved it, which is
	// the very mismatch VR comfort rests on avoiding. It is a knob because
	// OBVR still renders one image rather than two: without stereo, parallax
	// is the only depth cue there is, and it reads weaker than it will once
	// there are two eyes.
	//
	// 3 is where the headset is now. The path was 1, 2, 1.7, 2, 3 across four
	// sessions, and the path is the point: a number that keeps climbing is
	// chasing something the scale cannot supply. What is actually missing is
	// the second eye - so this should be tried at 1 again once stereo renders,
	// and the expectation is that it will want to come back down a long way.
	float movementScale = 3.0f;

	// The conversion actually applied to head movement, taste included.
	float EffectiveUnitsPerMetre() const { return unitsPerMetre * movementScale; }

	// How far the camera may be displaced from where the game put it, in
	// Oblivion units. 0 removes the limit.
	//
	// It has to track movementScale, or it quietly becomes the thing deciding
	// how far a lean reaches. 120 units at a scale of 3 is 57 cm of real
	// leaning - the same physical headroom 80 gave at a scale of 2, which is
	// the figure worth holding constant. The limit exists for the tracking
	// glitch and the person standing up and walking off, not for the lean.
	float maxOffsetUnits = 120.0f;

};

class HeadTracker {
public:
	void Configure(const TrackerSettings& settings);

	// Once per frame. For the simulated head, frameIndex takes the place of
	// time - OBVR needs no real clock for that, and without one the behaviour
	// stays reproducible.
	//
	// The head is deliberately never smoothed. Easing a tracked head would
	// show the user where their head was rather than where it is, and that
	// latency is felt directly in a headset. What OBVR does ease is the
	// camera motion it generates itself, in camera/LookControl.
	void Update(UInt32 frameIndex);

	// Takes the current head pose as the new zero. Everything after that is
	// reported relative to it.
	void Recenter();

	// Rotation relative to the zero pose, ready in Oblivion's camera space.
	const NiMatrix33& GetCameraRotation() const { return m_cameraRotation; }

	// Head displacement relative to the zero pose, in Oblivion units and in
	// the camera's own space. The caller has to rotate it into the space the
	// camera's position lives in, exactly as it multiplies the rotation on.
	//
	// Zero whenever the source delivers no position or positional tracking is
	// switched off, which leaves the vanilla camera position untouched.
	//
	// One to one with the head, never eased. See Update.
	const NiPoint3& GetCameraOffset() const { return m_cameraOffset; }

	// How far the head asked to move, in Oblivion units, before maxOffsetUnits
	// had its say. Only for the log, and it earns its place there: it is the
	// one way to tell a lean that is genuinely small from one the limit cut
	// short, which otherwise look identical from inside the headset.
	float GetRawOffsetUnits() const { return m_rawOffsetUnits; }

	// Half the distance between the eyes, in Oblivion units. The caller adds
	// it along the camera's right axis for one eye and subtracts it for the
	// other.
	//
	// Converted with unitsPerMetre rather than EffectiveUnitsPerMetre, and
	// that is the point of having kept the two apart: how far leaning moves
	// the camera is a preference, how far apart two eyes are is not. Scaling
	// this would not make leaning stronger, it would make the world look
	// smaller.
	//
	// Zero until a headset has reported its eye transforms, which makes the
	// caller's fallback the flat picture rather than a guessed separation.
	float GetHalfEyeSeparationUnits() const { return m_halfEyeSeparation; }

	// The orientation last read, still in OpenXR convention. Mostly for
	// diagnostics in the log.
	const Quaternion& GetRawOrientation() const { return m_rawOrientation; }

	// Whether a real headset is actually delivering poses. The look controls
	// are only taken away from the player when this is true - without a
	// headset there is nothing to hand them to.
	bool IsHeadsetConnected() const {
		return m_settings.source == TrackerSource::OpenVR && m_openVR.IsRunning();
	}

	// Whether OBVR actually holds the compositor and could submit a frame.
	// False whenever rendering is switched off, could not be set up, or the
	// source is not a real headset - so a caller that checks this never has
	// to ask why it failed.
	bool IsRenderingToHeadset() const {
		return m_settings.source == TrackerSource::OpenVR && m_openVR.IsSceneApplication();
	}

	// The connection to SteamVR, for whatever else needs it.
	//
	// Handed out rather than wrapped in pass-through methods. There has to be
	// exactly one registration with SteamVR and this class owns it, so the
	// alternatives are to expose it or to grow a second set of methods here
	// for every part of OpenVR that has nothing to do with head tracking.
	// Exposing it is the smaller lie: it says plainly that ownership sits
	// here for historical reasons rather than good ones, and the day a second
	// caller needs to change the connection rather than read it, that is the
	// day this becomes a session object of its own.
	const OpenVRBackend& GetBackend() const { return m_openVR; }

	// The mutable one, for the frame loop. WaitGetPoses keeps the pose it is
	// given, so the call that makes it cannot be const - and that is the right
	// shape: waiting on the compositor changes what the tracker will report.
	OpenVRBackend& GetBackendForFrame() { return m_openVR; }

private:
	// Reads orientation and position from the configured source. Returns
	// false when the source delivers no position, which is every source
	// except a connected headset.
	bool ReadSource(UInt32 frameIndex, Quaternion& orientation, NiPoint3& position) const;

	TrackerSettings m_settings;
	Quaternion m_rawOrientation = Quaternion::Identity();
	Quaternion m_reference = Quaternion::Identity();
	NiMatrix33 m_cameraRotation = NiMatrix33::Identity();

	// Positions in metres, in the tracking universe.
	//
	// m_hasReferencePosition is what keeps the camera from being flung
	// upwards on the first frame. The reference orientation can start as the
	// identity because that simply means "no extra rotation", but there is no
	// such harmless value for a position: the seated origin sits on the floor,
	// so an uncalibrated reference would read the head as roughly 1.2 metres
	// above zero and displace the camera by some 85 units for good. The first
	// valid pose therefore becomes the reference on its own, without waiting
	// for the recenter key.
	NiPoint3 m_rawPosition{0.0f, 0.0f, 0.0f};
	NiPoint3 m_referencePosition{0.0f, 0.0f, 0.0f};
	bool m_hasReferencePosition = false;

	NiPoint3 m_cameraOffset{0.0f, 0.0f, 0.0f};
	float m_rawOffsetUnits = 0.0f;

	// Read once from the headset, not per frame. A person's eyes do not move
	// apart during a session, and the two vtable calls it costs would be two
	// per frame for an answer that cannot change.
	float m_halfEyeSeparation = 0.0f;
	bool m_eyeSeparationRead = false;

	// Only used for TrackerSource::OpenVR, but it belongs here regardless:
	// the connection to SteamVR has to persist across frames rather than be
	// rebuilt for every query.
	OpenVRBackend m_openVR;
};

}  // namespace obvr::vr
