#include "vr/Locomotion.h"

#include "core/Log.h"

namespace obvr::vr {

void LocomotionManager::Configure(const Config::LocomotionSettings& settings) {
	m_settings = settings;
}

void LocomotionManager::Reset() {
	m_hasReferencePosition = false;
	m_referencePositionMetres = NiPoint3{0.0f, 0.0f, 0.0f};
	m_easedPositionMetres = NiPoint3{0.0f, 0.0f, 0.0f};

	m_teleportAiming = false;
	m_teleportTriggerWasDown = false;
	m_teleportLaserTarget = NiPoint3{0.0f, 0.0f, 0.0f};
}

LocomotionResult LocomotionManager::Update(const LocomotionFrame& frame) {
	LocomotionResult result{};

	if (!frame.headsetConnected || !frame.positionalTracking || frame.inMenu) {
		return result;
	}

	result = ComputeRoomScale(frame);

	if (m_settings.teleportEnabled && frame.rightValid && frame.leftValid) {
		auto teleportResult = ComputeTeleport(frame);
		result.teleportActive = teleportResult.teleportActive;
		result.teleportCommitThisFrame = teleportResult.teleportCommitThisFrame;
		result.teleportShowArc = teleportResult.teleportShowArc;
		result.teleportTargetWorld = teleportResult.teleportTargetWorld;
	}

	return result;
}

LocomotionResult LocomotionManager::ComputeRoomScale(const LocomotionFrame& frame) {
	LocomotionResult result{};

	const float scale = m_settings.roomScaleFactor;
	if (scale <= 0.0f || scale > 10.0f) {
		return result;
	}

	// Initialize reference position on first valid frame.
	if (!m_hasReferencePosition) {
		m_referencePositionMetres = frame.headPosition;
		m_easedPositionMetres = NiPoint3{0.0f, 0.0f, 0.0f};
		m_hasReferencePosition = true;
	}

	// Current raw offset from reference in tracking space (metres).
	NiPoint3 rawOffsetMetres = frame.headPosition - m_referencePositionMetres;

	// Apply scale factor to horizontal movement only for the base calculation.
	rawOffsetMetres.x *= scale;
	rawOffsetMetres.z *= scale;

	// Vertical is optional and uses its own scaling.
	if (!m_settings.roomScaleVertical) {
		rawOffsetMetres.y = 0.0f;
	} else {
		rawOffsetMetres.y *= scale * 0.5f; // dampen vertical movement for comfort
	}

	// Ease towards the raw offset to smooth out jitter and sudden movements.
	const float easeSpeed = m_settings.roomScaleEaseSpeed;
	if (easeSpeed > 0.0f && frame.deltaSeconds > 0.0f) {
		const float t = 1.0f - math::Pow(0.01f, easeSpeed * frame.deltaSeconds);
		m_easedPositionMetres.x += (rawOffsetMetres.x - m_easedPositionMetres.x) * t;
		m_easedPositionMetres.y += (rawOffsetMetres.y - m_easedPositionMetres.y) * t;
		m_easedPositionMetres.z += (rawOffsetMetres.z - m_easedPositionMetres.z) * t;
	} else {
		m_easedPositionMetres = rawOffsetMetres;
	}

	result.hasEasedPosition = true;
	result.easedPositionMetres = m_easedPositionMetres;

	// Convert eased metres to Oblivion units for the camera offset.
	const float upm = frame.unitsPerMetre > 0.0f ? frame.unitsPerMetre : 70.0f;
	result.cameraOffset.x = m_easedPositionMetres.x * upm;
	result.cameraOffset.y = m_easedPositionMetres.y * upm;
	result.cameraOffset.z = m_easedPositionMetres.z * upm;

	// Report raw offset magnitude for logging.
	result.rawOffsetUnits = math::Sqrt(
		rawOffsetMetres.x * rawOffsetMetres.x +
		rawOffsetMetres.z * rawOffsetMetres.z) * upm;

	return result;
}

LocomotionResult LocomotionManager::ComputeTeleport(const LocomotionFrame& frame) {
	LocomotionResult result{};

	const bool isRightHand = IsTeleportHandRight();
	if (isRightHand && !frame.rightValid) return result;
	if (!isRightHand && !frame.leftValid) return result;

	// Use the correct hand's data.
	const float trigger = isRightHand ? frame.rightTrigger : frame.leftTrigger;
	const Quaternion& controllerRot = isRightHand ? frame.rightOrientation : frame.leftOrientation;

	constexpr float kTriggerThreshold = 0.7f;
	const bool triggerDown = trigger > kTriggerThreshold;

	// Compute laser target direction from controller orientation.
	// In OpenVR convention, -Z is forward from the controller.
	NiPoint3 localForward{0.0f, 0.0f, -1.0f};
	NiPoint3 forwardDir = Rotate(controllerRot, localForward);

	// Ray cast to find teleport target.
	NiPoint3 laserTarget{};
	bool foundTarget = FindTeleportTarget(frame, laserTarget);

	if (foundTarget) {
		m_teleportLaserTarget = laserTarget;

		// Teleport aiming is active when trigger is held past threshold.
		m_teleportAiming = triggerDown;

		// Commit on rising edge of trigger pull while aiming.
		if (triggerDown && !m_teleportTriggerWasDown) {
			result.teleportCommitThisFrame = true;
			result.teleportTargetWorld = m_teleportLaserTarget;
			OBVR_LOG("Locomotion: teleport committed to (%.1f, %.1f, %.1f)",
			         static_cast<double>(m_teleportLaserTarget.x),
			         static_cast<double>(m_teleportLaserTarget.y),
			         static_cast<double>(m_teleportLaserTarget.z));

			// Move reference position so room-scale continues from new location.
			m_referencePositionMetres = frame.headPosition;
		}

		result.teleportActive = m_teleportAiming;
		if (result.teleportActive && m_settings.teleportShowArc) {
			result.teleportShowArc = true;
			result.teleportTargetWorld = m_teleportLaserTarget;
		}
	}

	m_teleportTriggerWasDown = triggerDown;
	return result;
}

bool LocomotionManager::FindTeleportTarget(const LocomotionFrame& frame, NiPoint3& outTargetWorld) const {
	// Simplified teleport target: project forward from controller position.
	// A full implementation would ray cast against game geometry.

	const bool isRightHand = IsTeleportHandRight();
	const NiPoint3& controllerPos = isRightHand ? frame.rightPosition : frame.leftPosition;
	const Quaternion& controllerRot = isRightHand ? frame.rightOrientation : frame.leftOrientation;

	// Forward direction in OpenVR space (-Z).
	NiPoint3 localForward{0.0f, 0.0f, -1.0f};
	NiPoint3 forward = Rotate(controllerRot, localForward);

	// Clamp vertical component to keep teleport mostly horizontal.
	const float maxPitch = math::Sin(45.0f * math::kDegreesToRadians);
	if (forward.y > maxPitch) forward.y = maxPitch;
	if (forward.y < -maxPitch) forward.y = -maxPitch;

	// Normalize again after clamping.
	float len = math::Sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (len > 0.001f) {
		forward.x /= len;
		forward.y /= len;
		forward.z /= len;
	}

	// Apply max distance if configured.
	float distanceMetres = 10.0f; // default range in metres
	if (m_settings.teleportMaxDistanceUnits > 0.0f) {
		const float upm = frame.unitsPerMetre > 0.0f ? frame.unitsPerMetre : 70.0f;
		distanceMetres = m_settings.teleportMaxDistanceUnits / upm;
	}

	// Compute target in tracking space, then convert to world units.
	NiPoint3 targetTracking = controllerPos + forward * distanceMetres;

	const float upm = frame.unitsPerMetre > 0.0f ? frame.unitsPerMetre : 70.0f;
	outTargetWorld.x = targetTracking.x * upm;
	outTargetWorld.y = targetTracking.z * upm; // OpenVR Z -> Oblivion Y (forward)
	outTargetWorld.z = -targetTracking.y * upm; // OpenVR Y -> Oblivion Z (up, inverted)

	return true;
}

}  // namespace obvr::vr
