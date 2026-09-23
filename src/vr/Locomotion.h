#pragma once

#include "core/Config.h"
#include "core/MathFns.h"
#include "game/NiMath.h"
#include "vr/HandInput.h"
#include "vr/OpenVRTypes.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

// Manages composable locomotion features layered on top of each other:
// - Room-scale walking is always active as the base (head deltas move camera)
// - Teleportation can be enabled alongside it for reaching further distances
// - Joystick movement with room-scale turning can also run alongside for fine control
//
// The manager computes room-scale offsets every frame from head pose, handles
// teleportation input and commits when enabled, and signals joystick turning mode.

struct LocomotionFrame {
	// Head tracking data.
	bool headsetConnected = false;
	bool positionalTracking = true;
	float unitsPerMetre = 70.0f;

	// Current head pose in OpenVR convention (seated space).
	NiPoint3 headPosition{0.0f, 0.0f, 0.0f};
	Quaternion headOrientation = Quaternion::Identity();

	// Camera rotation as the game built it, before OBVR adds head tracking.
	NiMatrix33 baseRotation;

	// Player's world position and heading for room-scale turning mode.
	bool playerPositionValid = false;
	NiPoint3 playerWorldPos{0.0f, 0.0f, 0.0f};
	float playerHeadingRadians = 0.0f; // angle in horizontal plane, 0 = -Y axis

	// Controller input for teleportation and joystick mode.
	bool rightValid = false;
	bool leftValid = false;
	NiPoint3 rightPosition{0.0f, 0.0f, 0.0f};
	NiPoint3 leftPosition{0.0f, 0.0f, 0.0f};
	Quaternion rightOrientation = Quaternion::Identity();
	Quaternion leftOrientation = Quaternion::Identity();
	float rightTrigger = 0.0f;
	float leftTrigger = 0.0f;
	float leftThumbX = 0.0f;
	float leftThumbY = 0.0f;

	// Frame timing.
	float deltaSeconds = 0.0f;

	// Whether the game is in a menu (locomotion disabled during menus).
	bool inMenu = false;
};

struct LocomotionResult {
	// Offset to add to camera position from room-scale walking, in Oblivion units and world space.
	NiPoint3 cameraOffset{0.0f, 0.0f, 0.0f};

	// For room-scale walking: the eased target position in tracking space (metres).
	bool hasEasedPosition = false;
	NiPoint3 easedPositionMetres{0.0f, 0.0f, 0.0f};

	// Teleportation state (only valid when teleportEnabled is true).
	bool teleportActive = false;       // laser is being aimed
	bool teleportCommitThisFrame = false; // trigger pulled: commit the teleport
	bool teleportShowArc = false;      // draw arc from feet to target
	NiPoint3 teleportTargetWorld{0.0f, 0.0f, 0.0f};

	// For logging.
	float rawOffsetUnits = 0.0f;
};

class LocomotionManager {
public:
	void Configure(const Config::LocomotionSettings& settings);

	// Once per frame, before the head offset is applied to the camera.
	LocomotionResult Update(const LocomotionFrame& frame);

	// Resets all state for when locomotion mode changes or headset disconnects.
	void Reset();

private:
	Config::LocomotionSettings m_settings;

	// Room-scale walking state.
	bool m_hasReferencePosition = false;
	NiPoint3 m_referencePositionMetres{0.0f, 0.0f, 0.0f}; // where the player started in tracking space
	NiPoint3 m_easedPositionMetres{0.0f, 0.0f, 0.0f};     // eased current position for smooth movement

	// Teleportation state.
	bool m_teleportAiming = false;
	bool m_teleportTriggerWasDown = false;
	NiPoint3 m_teleportLaserTarget{0.0f, 0.0f, 0.0f};

	// Compute the room-scale walking offset from head position deltas.
	LocomotionResult ComputeRoomScale(const LocomotionFrame& frame);

	// Handle teleportation input and compute laser target.
	LocomotionResult ComputeTeleport(const LocomotionFrame& frame);

	// Commit a teleport: move the reference position to the target.
	void CommitTeleport(const NiPoint3& targetWorldUnits, const LocomotionFrame& frame);

	// Get which hand is used for teleportation based on settings.
	bool IsTeleportHandRight() const {
		return m_settings.teleportHand == Config::LocomotionSettings::TeleportHand::Right;
	}

	// Convert a direction in OpenVR convention to Oblivion world space using base rotation.
	NiPoint3 ToWorldDirection(const NiPoint3& vrDir, const NiMatrix33& rot) const {
		return NiPoint3{
			vrDir.x * rot.m[0][0] + vrDir.y * rot.m[1][0] + vrDir.z * rot.m[2][0],
			vrDir.x * rot.m[0][1] + vrDir.y * rot.m[1][1] + vrDir.z * rot.m[2][1],
			vrDir.x * rot.m[0][2] + vrDir.y * rot.m[1][2] + vrDir.z * rot.m[2][2],
		};
	}

	// Ray cast from controller to find teleport target. Returns false if no valid hit.
	bool FindTeleportTarget(const LocomotionFrame& frame, NiPoint3& outTargetWorld) const;
};

}  // namespace obvr::vr
