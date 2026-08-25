#include "vr/HeadTracker.h"

#include "core/MathFns.h"
#include "core/Rotation.h"

namespace obvr::vr {
namespace {

constexpr float kTwoPi = 6.28318530718f;

}  // namespace

void HeadTracker::Configure(const TrackerSettings& settings) {
	// State is only reset on an actual change of source.
	//
	// The hot reload calls Configure again every ReloadEveryFrames, even when
	// nothing changed. If the reference fell back to identity each time, a
	// recenter with a real headset would be undone after two seconds at most.
	// With the existing sources this goes unnoticed because their reference
	// is the identity anyway - with OpenVR it would be a bug you feel in the
	// headset and can hardly attribute.
	const bool sourceChanged = m_settings.source != settings.source;
	m_settings = settings;

	if (sourceChanged) {
		m_reference = Quaternion::Identity();
		m_rawOrientation = Quaternion::Identity();
		m_cameraRotation = NiMatrix33::Identity();
	}

	if (m_settings.source == TrackerSource::OpenVR) {
		// Calling this repeatedly is harmless, and it makes switching to
		// openvr while the game is running take effect.
		m_openVR.Start();
	}
}

Quaternion HeadTracker::ReadSource(UInt32 frameIndex) const {
	switch (m_settings.source) {
		case TrackerSource::None:
			return Quaternion::Identity();

		case TrackerSource::Fixed: {
			// Die festen Winkel sind bereits in Oblivion-Achsen gedacht. Damit
			// sie denselben Weg nehmen wie eine echte HMD-Orientierung, werden
			// sie hier in die OpenXR-Konvention zurueckgerechnet:
			// Oblivion-Pitch liegt dort auf X, Yaw auf Y, Roll auf -Z.
			const Quaternion pitch = FromAxisAngle(1.0f, 0.0f, 0.0f, m_settings.fixedPitch);
			const Quaternion yaw = FromAxisAngle(0.0f, 1.0f, 0.0f, m_settings.fixedYaw);
			const Quaternion roll = FromAxisAngle(0.0f, 0.0f, 1.0f, -m_settings.fixedRoll);
			return (yaw * pitch * roll).Normalized();
		}

		case TrackerSource::Simulated: {
			// Langsames Umherschauen, damit ohne Headset sichtbar wird, dass
			// die Kamera einer fortlaufenden Orientierung folgt. Der Pitch
			// laeuft mit leicht anderer Frequenz, sonst waere die Bewegung
			// eine gerade Linie statt einer Figur.
			const UInt32 period =
				m_settings.simulatedPeriodFrames > 0 ? m_settings.simulatedPeriodFrames : 600;
			const float phase =
				static_cast<float>(frameIndex % period) / static_cast<float>(period) * kTwoPi;

			const float yawDegrees = m_settings.simulatedYawAmplitude * math::Sin(phase);
			const float pitchDegrees =
				m_settings.simulatedPitchAmplitude * math::Sin(phase * 2.0f);

			const Quaternion yaw = FromAxisAngle(0.0f, 1.0f, 0.0f, yawDegrees);
			const Quaternion pitch = FromAxisAngle(1.0f, 0.0f, 0.0f, pitchDegrees);
			return (yaw * pitch).Normalized();
		}

		case TrackerSource::OpenVR: {
			Quaternion orientation = Quaternion::Identity();
			if (m_openVR.ReadHeadOrientation(orientation)) {
				return orientation;
			}
			// No valid pose - because SteamVR is not running, or tracking has
			// not picked up yet. Keep the last orientation instead of letting
			// the camera snap back to rest. Without a connection that is the
			// identity, which means the vanilla camera.
			return m_rawOrientation;
		}

		case TrackerSource::OpenXR:
			// Noch nicht angebunden. Bis dahin bleibt die Kamera unveraendert,
			// statt eine erfundene Orientierung zu liefern.
			return Quaternion::Identity();
	}

	return Quaternion::Identity();
}

void HeadTracker::Update(UInt32 frameIndex) {
	m_rawOrientation = ReadSource(frameIndex);

	// Referenz herausrechnen, dann erst das Koordinatensystem wechseln. Beides
	// in OpenXR-Konvention zu erledigen haelt die Umrechnung an einer Stelle.
	const Quaternion relative = (m_reference.Conjugate() * m_rawOrientation).Normalized();
	m_cameraRotation = ToMatrix(FromOpenXR(relative));
}

void HeadTracker::Recenter() { m_reference = m_rawOrientation; }

}  // namespace obvr::vr
