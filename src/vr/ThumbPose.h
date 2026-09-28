#pragma once

// The thumb, joint by joint, the way Half-Life: Alyx shows it (the tester,
// 2026-09-28: "Daumen machen wir wie alyx dann soweit es geht").
//
// SteamVR's summary gives the thumb one curl: how far it is wrapped around a
// fist (VRSkeletalSummaryData_t.flFingerCurl, openvr wiki "SteamVR Skeletal
// Input"). On an Index that is on or off - the thumb on a button reads as a
// fist's thumb - so the hand showed a thumb either straight or folded across
// the fingers.
//
// The full skeleton says more (openvr wiki "Hand Skeleton": 31 bones, thumb
// eBone_Thumb0 2, Thumb1 3, Thumb2 4, Thumb3 5 the tip; parent-space
// rotations from GetSkeletalBoneData). With the controller's range of motion
// (VRSkeletalMotionRange_WithController) a thumb resting on a button bends
// at its joints the way it really does: its base turned in, the rest little.
//
// Each of the three thumb joints becomes a share of the way from SteamVR's
// open hand to its fist (GetSkeletalReferenceTransforms, OpenHand and Fist):
// the joint's rotation from the open hand, as a rotation vector, projected
// onto the axis the fist turns it about, over the fist's angle - 0 open, 1
// as in the fist, clamped. The game's thumb links (Finger0, 01, 02) are then
// blended by their own joint's share instead of by the one curl
// (game::TrackedLinkRotation). What does not lie along the fist's path - a
// sideways turn - is dropped: the game's hand has only its two poses to
// blend between.
//
// Pure, covered by thumb_pose_test.

#include "core/MathFns.h"
#include "core/Types.h"

namespace obvr::vr {

// A bone's rotation as openvr writes it: HmdQuaternionf_t, w first.
struct BoneRotation {
	float w = 1.0f;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

inline constexpr int kSkeletonBoneCount = 31;
inline constexpr int kSkeletonThumbFirst = 2;  // eBone_Thumb0; Thumb1 and Thumb2 follow
inline constexpr int kThumbJoints = 3;

// q's conjugate times r: the rotation from q to r in q's own frame.
inline BoneRotation RotationBetween(const BoneRotation& q, const BoneRotation& r) {
	const float w = q.w, x = -q.x, y = -q.y, z = -q.z;
	return BoneRotation{w * r.w - x * r.x - y * r.y - z * r.z, w * r.x + x * r.w + y * r.z - z * r.y,
	                    w * r.y - x * r.z + y * r.w + z * r.x, w * r.z + x * r.y - y * r.x + z * r.w};
}

// A rotation as its rotation vector (axis times angle, radians), the short
// way round.
inline void RotationVector(const BoneRotation& q, float out[3]) {
	float w = q.w, x = q.x, y = q.y, z = q.z;
	if (w < 0.0f) {
		w = -w;
		x = -x;
		y = -y;
		z = -z;
	}
	const float s = math::Sqrt(x * x + y * y + z * z);
	if (!(s > 1e-7f)) {
		out[0] = out[1] = out[2] = 0.0f;
		return;
	}
	const float angle = 2.0f * math::Atan2(s, w);
	out[0] = x / s * angle;
	out[1] = y / s * angle;
	out[2] = z / s * angle;
}

// How far one joint is from the open hand towards the fist: 0 to 1, or -1
// when the fist does not turn this joint (nothing to measure against).
inline float JointShare(const BoneRotation& open, const BoneRotation& fist, const BoneRotation& now) {
	float toFist[3];
	float toNow[3];
	RotationVector(RotationBetween(open, fist), toFist);
	RotationVector(RotationBetween(open, now), toNow);
	const float fistAngle =
		math::Sqrt(toFist[0] * toFist[0] + toFist[1] * toFist[1] + toFist[2] * toFist[2]);
	if (!(fistAngle > 1e-3f)) {
		return -1.0f;
	}
	const float along = (toNow[0] * toFist[0] + toNow[1] * toFist[1] + toNow[2] * toFist[2]) / fistAngle;
	const float share = along / fistAngle;
	return share < 0.0f ? 0.0f : (share > 1.0f ? 1.0f : share);
}

// The three thumb joints' shares from a whole skeleton and its two reference
// poses. False when any joint cannot be measured; `out` is then left alone.
inline bool ThumbShares(const BoneRotation* open, const BoneRotation* fist, const BoneRotation* now,
                        float out[kThumbJoints]) {
	float shares[kThumbJoints];
	for (int j = 0; j < kThumbJoints; ++j) {
		const int bone = kSkeletonThumbFirst + j;
		shares[j] = JointShare(open[bone], fist[bone], now[bone]);
		if (shares[j] < 0.0f) {
			return false;
		}
	}
	for (int j = 0; j < kThumbJoints; ++j) {
		out[j] = shares[j];
	}
	return true;
}

// Values between 0 and 1 in quarter steps, packed into one number, so a log
// line can be written only when one of them has moved a step: each clamped,
// rounded to 0..4, base 5. Not a number counts as 0.
inline int QuarterSteps(const float* values, int count) {
	int packed = 0;
	for (int i = 0; i < count; ++i) {
		const float v = values[i] > 0.0f ? (values[i] < 1.0f ? values[i] : 1.0f) : 0.0f;
		packed = packed * 5 + static_cast<int>(v * 4.0f + 0.5f);
	}
	return packed;
}

}  // namespace obvr::vr
