#include "vr/HeadTracker.h"

#include "core/MathFns.h"
#include "core/Rotation.h"
#include "vr/HeadOffset.h"

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

		m_rawPosition = NiPoint3{0.0f, 0.0f, 0.0f};
		m_referencePosition = NiPoint3{0.0f, 0.0f, 0.0f};
		m_hasReferencePosition = false;
		m_cameraOffset = NiPoint3{0.0f, 0.0f, 0.0f};
	}

	if (m_settings.source == TrackerSource::OpenVR) {
		// Calling this repeatedly is harmless, and it makes switching to
		// openvr while the game is running take effect.
		m_openVR.Start();
	}
}

bool HeadTracker::ReadSource(UInt32 frameIndex, Quaternion& orientation,
                             NiPoint3& position) const {
	switch (m_settings.source) {
		case TrackerSource::None:
			orientation = Quaternion::Identity();
			return false;

		case TrackerSource::Fixed: {
			// The fixed angles are already meant in Oblivion axes. So that they
			// take the same route as a real HMD orientation, they are
			// converted back into OpenXR convention here: Oblivion pitch sits
			// on X there, yaw on Y, roll on -Z.
			const Quaternion pitch = FromAxisAngle(1.0f, 0.0f, 0.0f, m_settings.fixedPitch);
			const Quaternion yaw = FromAxisAngle(0.0f, 1.0f, 0.0f, m_settings.fixedYaw);
			const Quaternion roll = FromAxisAngle(0.0f, 0.0f, 1.0f, -m_settings.fixedRoll);
			orientation = (yaw * pitch * roll).Normalized();
			return false;
		}

		case TrackerSource::Simulated: {
			// Slow looking around, so that without a headset it becomes
			// visible that the camera follows a continuous orientation. Pitch
			// runs at a slightly different frequency, otherwise the motion
			// would be a straight line rather than a figure.
			const UInt32 period =
				m_settings.simulatedPeriodFrames > 0 ? m_settings.simulatedPeriodFrames : 600;
			const float phase =
				static_cast<float>(frameIndex % period) / static_cast<float>(period) * kTwoPi;

			const float yawDegrees = m_settings.simulatedYawAmplitude * math::Sin(phase);
			const float pitchDegrees =
				m_settings.simulatedPitchAmplitude * math::Sin(phase * 2.0f);

			const Quaternion yaw = FromAxisAngle(0.0f, 1.0f, 0.0f, yawDegrees);
			const Quaternion pitch = FromAxisAngle(1.0f, 0.0f, 0.0f, pitchDegrees);
			orientation = (yaw * pitch).Normalized();
			return false;
		}

		case TrackerSource::OpenVR: {
			if (m_openVR.ReadHeadPose(orientation, position)) {
				return true;
			}
			// No valid pose - because SteamVR is not running, or tracking has
			// not picked up yet. Keep the last orientation instead of letting
			// the camera snap back to rest. Without a connection that is the
			// identity, which means the vanilla camera.
			orientation = m_rawOrientation;
			return false;
		}

		case TrackerSource::OpenXR:
			// Not wired up yet. Until then the camera stays unchanged rather
			// than being fed an invented orientation.
			orientation = Quaternion::Identity();
			return false;
	}

	orientation = Quaternion::Identity();
	return false;
}

void HeadTracker::Update(UInt32 frameIndex, float deltaSeconds) {
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	const bool hasPosition = ReadSource(frameIndex, m_rawOrientation, position);

	// Factor out the reference first, change coordinate system second. Doing
	// both in OpenXR convention keeps the conversion in one place.
	const Quaternion relative = (m_reference.Conjugate() * m_rawOrientation).Normalized();
	m_cameraRotation = ToMatrix(FromOpenXR(relative));

	// The raw position is kept even when positional tracking is switched off,
	// so that switching it on through the hot reload starts from where the
	// head actually is rather than from a stale reading.
	if (hasPosition) {
		m_rawPosition = position;
		if (!m_hasReferencePosition) {
			m_referencePosition = position;
			m_hasReferencePosition = true;
		}
	}

	if (!hasPosition || !m_settings.positionalTracking) {
		m_cameraOffset = NiPoint3{0.0f, 0.0f, 0.0f};
		return;
	}

	const NiPoint3 target =
		ClampOffset(OffsetFromPose(m_reference, m_rawPosition, m_referencePosition,
		                           m_settings.unitsPerMetre),
		            m_settings.maxOffsetUnits);

	m_cameraOffset =
		m_settings.smoothPosition
			? Approach(m_cameraOffset, target, m_settings.smoothingSpeed, deltaSeconds)
			: target;
}

void HeadTracker::Recenter() {
	m_reference = m_rawOrientation;
	m_referencePosition = m_rawPosition;
	m_hasReferencePosition = true;

	// The new zero is the pose being held right now, so the offset is zero by
	// definition. Setting it here rather than letting the smoothing walk it
	// down means recentering takes effect at once, which is the whole point
	// of pressing the key.
	m_cameraOffset = NiPoint3{0.0f, 0.0f, 0.0f};
}

}  // namespace obvr::vr
