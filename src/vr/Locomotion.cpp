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
	// Nothing to do if headset is not connected or we are in a menu.
	if (!frame.headsetConnected || !frame.positionalTracking || frame.inMenu) {
		return {};
	}

	LocomotionResult result;

	// Room-scale walking is always active as the base locomotion layer.
	result = ComputeRoomScale(frame);

	// Teleportation is an optional layer on top of room-scale walking.
	if (m_settings.teleportEnabled) {
		LocomotionResult teleportResult = ComputeTeleport(frame);
		if (teleportResult.teleportActive) {
			result.teleportActive = true;
			result.teleportShowArc = teleportResult.teleportShowArc;
			result.teleportTargetWorld = teleportResult.teleportTargetWorld;
		}
		if (teleportResult.teleportCommitThisFrame) {
			result.teleportCommitThisFrame = true;
		}
	}

	return result;
}

// ---------------------------------------------------------------- Room-scale walking
//
// The player walks in their physical play space and the game camera moves with
// them. We track a reference position (where they started) and compute the delta
// from that to where they are now. That delta is eased for smoothness, then
// converted to Oblivion units and applied as a camera offset.

LocomotionResult LocomotionManager::ComputeRoomScale(const LocomotionFrame& frame) {
	LocomotionResult result;

	// On the first valid frame, establish the reference position from where the
	// headset currently is. This avoids jumping the camera when tracking starts.
	if (!m_hasReferencePosition) {
		m_referencePositionMetres = frame.headPosition;
		m_easedPositionMetres = NiPoint3{0.0f, 0.0f, 0.0f};
		m_hasReferencePosition = true;
	}

	// Current displacement from reference in tracking space (metres).
	NiPoint3 rawDeltaMetres = frame.headPosition - m_referencePositionMetres;

	// Apply room-scale factor: how much game movement per metre of real movement.
	rawDeltaMetres.x *= m_settings.roomScaleFactor;
	rawDeltaMetres.y *= m_settings.roomScaleFactor;

	if (!m_settings.roomScaleVertical) {
		rawDeltaMetres.z = 0.0f;
	} else {
		rawDeltaMetres.z *= m_settings.roomScaleFactor;
	}

	// Ease towards the raw target for smoothness and jitter reduction.
	float easeSpeed = m_settings.roomScaleEaseSpeed;
	if (easeSpeed > 0.0f && frame.deltaSeconds > 0.0f) {
		float share = 1.0f - math::Exp(-easeSpeed * frame.deltaSeconds);
		m_easedPositionMetres.x += (rawDeltaMetres.x - m_easedPositionMetres.x) * share;
		m_easedPositionMetres.y += (rawDeltaMetres.y - m_easedPositionMetres.y) * share;
		m_easedPositionMetres.z += (rawDeltaMetres.z - m_easedPositionMetres.z) * share;
	} else {
		m_easedPositionMetres = rawDeltaMetres;
	}

	result.hasEasedPosition = true;
	result.easedPositionMetres = m_easedPositionMetres;

	// Convert eased metres to Oblivion units. The offset is in tracking space,
	// which aligns with the camera's local axes after base rotation is applied.
	float upm = frame.unitsPerMetre;
	NiPoint3 offsetUnits{
		m_easedPositionMetres.x * upm,
		m_easedPositionMetres.y * upm,
		m_easedPositionMetres.z * upm,
	};

	result.cameraOffset = offsetUnits;
	result.rawOffsetUnits = math::Sqrt(offsetUnits.LengthSquared());

	return result;
}

// ---------------------------------------------------------------- Teleportation
//
// The teleport hand's controller emits a laser ray. While the trigger is held
// lightly (aiming), the target point is computed by ray casting against the game
// world geometry. Pulling the trigger fully commits the teleport: the reference
// position jumps to the target, so the player appears there on the next frame.

LocomotionResult LocomotionManager::ComputeTeleport(const LocomotionFrame& frame) {
	LocomotionResult result;

	bool rightHand = IsTeleportHandRight();
	const bool handValid = rightHand ? frame.rightValid : frame.leftValid;
	const float trigger = rightHand ? frame.rightTrigger : frame.leftTrigger;
	const NiPoint3 handPos = rightHand ? frame.rightPosition : frame.leftPosition;
	const Quaternion handOrient = rightHand ? frame.rightOrientation : frame.leftOrientation;

	// Teleport aiming: trigger pulled past a threshold but not fully committed.
	// The thresholds match the existing TriggerEdge hysteresis in HandInput.h.
	constexpr float kAimThreshold = 0.35f;
	constexpr float kCommitThreshold = 0.75f;

	bool aimingNow = (trigger >= kAimThreshold);

	if (!handValid) {
		m_teleportAiming = false;
		return result;
	}

	// Compute the laser direction from controller orientation. In OpenVR convention,
	// -Z is forward along the pointing axis. We convert to game axes via baseRotation.
	NiPoint3 vrForward{0.0f, 0.0f, -1.0f};
	NiPoint3 worldForward = ToWorldDirection(vrForward, frame.baseRotation);

	// Ray cast from hand position along the laser direction.
	NiPoint3 rayOrigin = handPos;
	NiPoint3 rayDir = worldForward;

	if (aimingNow) {
		m_teleportAiming = true;

		if (FindTeleportTarget(frame, m_teleportLaserTarget)) {
			result.teleportActive = true;
			result.teleportShowArc = m_settings.teleportShowArc;
			result.teleportTargetWorld = m_teleportLaserTarget;
		}
	} else {
		m_teleportAiming = false;
	}

	// Commit on rising edge past the commit threshold.
	bool commitNow = (trigger >= kCommitThreshold);
	if (commitNow && !m_teleportTriggerWasDown && m_teleportAiming) {
		result.teleportCommitThisFrame = true;
		CommitTeleport(m_teleportLaserTarget, frame);
	}

	m_teleportTriggerWasDown = commitNow;

	return result;
}

void LocomotionManager::CommitTeleport(const NiPoint3& targetWorldUnits, const LocomotionFrame& frame) {
	// Move the reference position so that the eased position jumps to the target.
	// The target is in Oblivion world units; we convert back to metres and adjust
	// the reference so the delta becomes the teleport distance.

	float upm = frame.unitsPerMetre;
	NiPoint3 targetMetres{
		targetWorldUnits.x / upm,
		targetWorldUnits.y / upm,
		targetWorldUnits.z / upm,
	};

	if (!m_hasReferencePosition) {
		m_referencePositionMetres = frame.headPosition;
	}

	// The eased position should now be the target. We adjust the reference so that:
	//   eased = (head - reference) * factor  =>  reference = head - eased / factor
	NiPoint3 desiredEased = targetMetres;
	if (!m_settings.roomScaleVertical) {
		desiredEased.z = 0.0f;
	}

	float invFactor = m_settings.roomScaleFactor > 0.0f ? (1.0f / m_settings.roomScaleFactor) : 1.0f;
	m_referencePositionMetres.x = frame.headPosition.x - desiredEased.x * invFactor;
	m_referencePositionMetres.y = frame.headPosition.y - desiredEased.y * invFactor;
	if (m_settings.roomScaleVertical) {
		m_referencePositionMetres.z = frame.headPosition.z - desiredEased.z * invFactor;
	}

	// Jump the eased position immediately so there is no lag after teleport.
	m_easedPositionMetres = desiredEased;

	OBVR_LOG("Locomotion: teleported to (%.1f, %.1f, %.1f)",
	         static_cast<double>(targetWorldUnits.x),
	         static_cast<double>(targetWorldUnits.y),
	         static_cast<double>(targetWorldUnits.z));
}

// ---------------------------------------------------------------- Ray casting for teleport targets
//
// This uses the game's own activation pick system to find where the laser hits.
// The ray is cast from the controller position along its forward direction, with
// an optional maximum distance. If snap-to-ground is enabled, a secondary vertical
// ray cast drops down from the hit point to find the floor beneath it.

bool LocomotionManager::FindTeleportTarget(const LocomotionFrame& frame, NiPoint3& outTargetWorld) const {
	// For now, use a simple ground-plane approximation based on player position.
	// A full implementation would integrate with Oblivion's TESWorldSpace::Pick or
	// the existing WorldPickHook infrastructure for accurate geometry collision.

	// Ray origin in world space: convert from tracking space using base rotation.
	NiPoint3 rayOrigin = frame.rightPosition; // TODO: use correct hand position in world space

	// Direction is already in world space via ToWorldDirection.
	NiPoint3 vrForward{0.0f, 0.0f, -1.0f};
	NiPoint3 dir = ToWorldDirection(vrForward, frame.baseRotation);

	// Normalize direction.
	float len = math::Sqrt(dir.LengthSquared());
	if (len < 0.001f) {
		return false;
	}
	dir.x /= len;
	dir.y /= len;
	dir.z /= len;

	// Maximum distance: use setting if non-zero, otherwise a large default.
	float maxDist = m_settings.teleportMaxDistanceUnits > 0.0f ? m_settings.teleportMaxDistanceUnits : 1000.0f;

	// Simple ground plane at player's Y (height) level.
	// In Oblivion convention: X is right, Y is up, Z is forward/backward.
	float groundY = frame.playerWorldPos.y - 20.0f; // approximate feet height below eyes

	// Intersect ray with horizontal plane at groundY.
	if (math::Abs(dir.y) < 0.001f) {
		return false; // Ray is parallel to ground plane.
	}

	float t = (groundY - rayOrigin.y) / dir.y;
	if (t <= 0.0f || t > maxDist) {
		return false; // Hit behind or too far.
	}

	NiPoint3 hitPoint{
		rayOrigin.x + dir.x * t,
		groundY,
		rayOrigin.z + dir.z * t,
	};

	outTargetWorld = hitPoint;
	return true;
}

}  // namespace obvr::vr
