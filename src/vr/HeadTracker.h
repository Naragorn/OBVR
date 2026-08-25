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
	// there are two eyes. 1.7 is what a headset settled on, not a round number
	// picked in advance.
	float movementScale = 1.7f;

	// The conversion actually applied to head movement, taste included.
	float EffectiveUnitsPerMetre() const { return unitsPerMetre * movementScale; }

	// How far the camera may be displaced from where the game put it, in
	// Oblivion units. 0 removes the limit.
	//
	// Roughly 80 units is 114 cm of camera travel, which at a movementScale of
	// 2 is 57 cm of real leaning - enough to lean without letting a tracking
	// glitch or someone standing up push the camera through a wall.
	float maxOffsetUnits = 80.0f;

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

	// The orientation last read, still in OpenXR convention. Mostly for
	// diagnostics in the log.
	const Quaternion& GetRawOrientation() const { return m_rawOrientation; }

	// Whether a real headset is actually delivering poses. The look controls
	// are only taken away from the player when this is true - without a
	// headset there is nothing to hand them to.
	bool IsHeadsetConnected() const {
		return m_settings.source == TrackerSource::OpenVR && m_openVR.IsRunning();
	}

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

	// Only used for TrackerSource::OpenVR, but it belongs here regardless:
	// the connection to SteamVR has to persist across frames rather than be
	// rebuilt for every query.
	OpenVRBackend m_openVR;
};

}  // namespace obvr::vr
