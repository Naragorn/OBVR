#pragma once

#include "core/Rotation.h"
#include "game/NiMath.h"
#include "vr/Quaternion.h"

namespace obvr::game {

// Use vr namespace's quaternion type and conversion functions for coordinate conversions.
using Quaternion = vr::Quaternion;
using vr::ToMatrix;

// The arithmetic of holding a skeleton bone where a controller is.
//
// The hand-tracked mode knows each controller relative to the head: a
// rotation and an offset, in the game's axes, the way the arms are placed.
// The camera node's world transform says where the head is in the world, so
// the bone's wanted WORLD pose is the controller's pose carried through the
// camera. A bone is only written in its parent's space, though - the world
// transform is recomputed from parent * local by the engine's update pass -
// so the wanted world pose is taken back under the parent.
//
// Pure, so every step is checked by a test with made-up frames.

struct BonePose {
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
};

// Where a hand bone belongs in the world: the controller's rotation and
// offset relative to the head, carried through the camera's world transform,
// with the calibration turned on last - the fixed rotation between the
// controller's axes (x right, y forward, z up) and the bone's own (x along
// the fingers on a Bip01 skeleton).
inline BonePose HandBoneWorld(const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                              const NiMatrix33& relativeRot, const NiPoint3& offsetUnits,
                              const NiMatrix33& calibration) {
	BonePose pose;
	pose.rot = cameraRot * relativeRot * calibration;
	pose.pos = cameraPos + cameraRot * offsetUnits;
	return pose;
}

// The local transform that puts a node at `wanted` under a parent whose
// world transform is given. A parent scale of zero or less is refused as
// identity scale: dividing by it would put the bone at infinity.
inline BonePose LocalUnderParent(const NiMatrix33& parentRot, const NiPoint3& parentPos,
                                 float parentScale, const BonePose& wanted) {
	const NiMatrix33 inverse = InverseRotation(parentRot);
	BonePose local;
	local.rot = inverse * wanted.rot;
	const float divisor = parentScale > 0.0f ? parentScale : 1.0f;
	local.pos = (inverse * (wanted.pos - parentPos)) * (1.0f / divisor);
	return local;
}

// The calibration from three angles in degrees: the roll about the bone's
// own axis first, then pitch, then yaw - EulerToMatrix's Z * Y * X order
// with X the roll. A Bip01 hand bone runs along x and the controller points
// along y, so ninety degrees of yaw with no roll is the starting point.
inline NiMatrix33 HandCalibration(float rollDegrees, float pitchDegrees, float yawDegrees) {
	return EulerToMatrix(rollDegrees, pitchDegrees, yawDegrees);
}

// Compose a child pose (relative to some parent frame) into world space given
// the parent's world transform. The child's rotation is applied after the
// parent's, and its position is translated by the parent's orientation then
// added to the parent's position. Used for finger bones relative to hand bone.
inline BonePose ComposeWorld(const NiMatrix33& parentRot, const NiPoint3& parentPos,
                             const NiMatrix33& childRelRot, const NiPoint3& childRelPos) {
	BonePose pose;
	pose.rot = parentRot * childRelRot;
	pose.pos = parentPos + parentRot * childRelPos;
	return pose;
}

// Convert a position from OpenVR convention (X right, Y up, -Z forward) to
// Oblivion game convention (X right, Y forward, Z up). The mapping is:
// x_game = x_vr, y_game = -z_vr, z_game = y_vr.
inline NiPoint3 PositionFromOpenVR(float vx, float vy, float vz) {
	return NiPoint3{vx, -vz, vy};
}

// Convert a quaternion from OpenVR convention to Oblivion game convention.
// Same axis mapping as FromOpenXR: x_obl = x_vr, y_obl = -z_vr, z_obl = y_vr.
inline Quaternion QuatFromOpenVR(float vx, float vy, float vz, float vw) {
	return Quaternion{vx, -vz, vy, vw};
}

}  // namespace obvr::game
