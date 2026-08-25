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
};

class HeadTracker {
public:
	void Configure(const TrackerSettings& settings);

	// Once per frame. For the simulated head, frameIndex takes the place of
	// time - OBVR needs no real clock for that, and without one the behaviour
	// stays reproducible.
	void Update(UInt32 frameIndex);

	// Takes the current head pose as the new zero. Everything after that is
	// reported relative to it.
	void Recenter();

	// Rotation relative to the zero pose, ready in Oblivion's camera space.
	const NiMatrix33& GetCameraRotation() const { return m_cameraRotation; }

	// The orientation last read, still in OpenXR convention. Mostly for
	// diagnostics in the log.
	const Quaternion& GetRawOrientation() const { return m_rawOrientation; }

private:
	Quaternion ReadSource(UInt32 frameIndex) const;

	TrackerSettings m_settings;
	Quaternion m_rawOrientation = Quaternion::Identity();
	Quaternion m_reference = Quaternion::Identity();
	NiMatrix33 m_cameraRotation = NiMatrix33::Identity();

	// Only used for TrackerSource::OpenVR, but it belongs here regardless:
	// the connection to SteamVR has to persist across frames rather than be
	// rebuilt for every query.
	OpenVRBackend m_openVR;
};

}  // namespace obvr::vr
