#pragma once

#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/NiMath.h"

namespace obvr::game {

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
// the fingers on a Bip01 skeleton). The grip is where the hand sits from the
// controller's tracked origin, in the controller's own axes: the origin is
// on the tracking head, above the fingers that hold the handle.
inline BonePose HandBoneWorld(const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                              const NiMatrix33& relativeRot, const NiPoint3& offsetUnits,
                              const NiMatrix33& calibration,
                              const NiPoint3& gripUnits = NiPoint3{0.0f, 0.0f, 0.0f}) {
	BonePose pose;
	pose.rot = cameraRot * relativeRot * calibration;
	pose.pos = cameraPos + cameraRot * (offsetUnits + relativeRot * gripUnits);
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

// Where a bone's parent has to be for the bone, keeping its own local
// transform, to land at `childWanted`. The world of a child is
// parent.rot * local.rot and parent.pos + parent.rot * (parent.scale *
// local.pos); solved for the parent.
//
// For the hands: moving the hand bone alone leaves the forearm where the
// animation has it, and the skin between the two - the wrist, weighted to
// both - stretches across the gap, which the 2026-09-25 run saw as hands
// "stretching several centimetres past the hand". Placing the forearm this
// way carries the hand with it rigidly; the only stretch left is at the
// elbow, under the hidden arm mesh.
inline BonePose ParentForChildAt(const BonePose& childWanted, const NiMatrix33& childLocalRot,
                                 const NiPoint3& childLocalPos, float parentScale) {
	BonePose parent;
	parent.rot = childWanted.rot * InverseRotation(childLocalRot);
	const float scale = parentScale > 0.0f ? parentScale : 1.0f;
	parent.pos = childWanted.pos - parent.rot * (childLocalPos * scale);
	return parent;
}

// The calibration from three angles in degrees: the roll about the bone's
// own axis first, then pitch, then yaw - EulerToMatrix's Z * Y * X order
// with X the roll. A Bip01 hand bone runs along x and the controller points
// along y, so ninety degrees of yaw with no roll is the starting point.
inline NiMatrix33 HandCalibration(float rollDegrees, float pitchDegrees, float yawDegrees) {
	return EulerToMatrix(rollDegrees, pitchDegrees, yawDegrees);
}

// The three angles back out of a calibration: EulerToMatrix is Z(yaw) *
// Y(pitch) * X(roll), so element [2][0] is -sin(pitch), [2][1] and [2][2]
// carry the roll and [1][0] and [0][0] the yaw. At a pitch of +-90 degrees
// roll and yaw turn about the same axis; the roll is then taken as zero and
// the whole turn given to the yaw.
inline void CalibrationAngles(const NiMatrix33& m, float& rollDegrees, float& pitchDegrees,
                              float& yawDegrees) {
	float sinePitch = -m.data[2][0];
	if (sinePitch > 1.0f) {
		sinePitch = 1.0f;
	} else if (sinePitch < -1.0f) {
		sinePitch = -1.0f;
	}
	// math::Asin stops short of +-1 on purpose; at the pole the angle is exact.
	float pitch = math::Asin(sinePitch);
	float roll = 0.0f;
	float yaw = 0.0f;
	if (sinePitch > 0.9999f || sinePitch < -0.9999f) {
		pitch = sinePitch > 0.0f ? math::kHalfPi : -math::kHalfPi;
		yaw = math::Atan2(-m.data[0][1], m.data[1][1]);
	} else {
		roll = math::Atan2(m.data[2][1], m.data[2][2]);
		yaw = math::Atan2(m.data[1][0], m.data[0][0]);
	}
	const float toDegrees = 1.0f / math::kDegreesToRadians;
	rollDegrees = roll * toDegrees;
	pitchDegrees = pitch * toDegrees;
	yawDegrees = yaw * toDegrees;
}

// The calibration and grip that put a hand at `wanted` for the controller
// where it is now - HandBoneWorld solved for its last two arguments. What
// "grab the hand and let go where it belongs" stores: the hand is held
// still in the world while the controller moves to it, and on release the
// difference between the two is the calibration.
struct HandFit {
	NiMatrix33 calibration = NiMatrix33::Identity();
	NiPoint3 gripUnits{0.0f, 0.0f, 0.0f};
};

inline HandFit FitHandToPose(const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                             const NiMatrix33& relativeRot, const NiPoint3& offsetUnits,
                             const BonePose& wanted) {
	HandFit fit;
	const NiMatrix33 inverseCamera = InverseRotation(cameraRot);
	const NiMatrix33 inverseRelative = InverseRotation(relativeRot);
	fit.calibration = inverseRelative * (inverseCamera * wanted.rot);
	fit.gripUnits = inverseRelative * (inverseCamera * (wanted.pos - cameraPos) - offsetUnits);
	return fit;
}

// Adjusting the hands: while the option is on, closing a grip holds that
// hand still in the world, and opening it commits the fit.
struct HandAdjustState {
	bool held = false;
	BonePose frozen;
};

enum class HandAdjustStep {
	Follow,  // the hand follows its controller as always
	Hold,    // the hand stays at `frozen`
	Commit,  // the grip opened: fit the hand to where it was held
};

inline HandAdjustStep StepHandAdjust(HandAdjustState& s, bool adjusting, bool gripDown,
                                     const BonePose& current) {
	if (!adjusting) {
		s.held = false;
		return HandAdjustStep::Follow;
	}
	if (gripDown) {
		if (!s.held) {
			s.held = true;
			s.frozen = current;
		}
		return HandAdjustStep::Hold;
	}
	if (s.held) {
		s.held = false;
		return HandAdjustStep::Commit;
	}
	return HandAdjustStep::Follow;
}

}  // namespace obvr::game
