// Checks the bone pin's arithmetic: a wanted world pose taken under a
// parent composes back to itself, the calibration turns the bone's axis
// onto the controller's, and the guards hold.

#include <cstdio>

#include "core/Rotation.h"
#include "game/BonePin.h"

namespace {

using obvr::EulerToMatrix;
using obvr::NiMatrix33;
using obvr::NiPoint3;
using obvr::game::BonePose;
using obvr::game::HandBoneWorld;
using obvr::game::HandCalibration;
using obvr::game::LocalUnderParent;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float eps = 0.01f) {
	return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

bool NearMatrix(const NiMatrix33& a, const NiMatrix33& b, float eps = 0.001f) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (!Near(a.data[r][c], b.data[r][c], eps)) {
				return false;
			}
		}
	}
	return true;
}

void TestWorldPose() {
	std::printf("The wanted world pose\n");
	const NiMatrix33 identity = NiMatrix33::Identity();
	const NiPoint3 camera{100.0f, 200.0f, 300.0f};
	const NiPoint3 offset{10.0f, 20.0f, -5.0f};

	BonePose pose = HandBoneWorld(identity, camera, identity, offset, identity);
	Check(NearPoint(pose.pos, NiPoint3{110.0f, 220.0f, 295.0f}),
	      "with an unturned camera the offset adds straight on");
	Check(NearMatrix(pose.rot, identity), "and the rotation is the identity");

	// A camera turned ninety degrees left carries the offset with it: the
	// controller's "forward" is now the world's left... in the game's axes
	// a yaw of 90 about z takes +y to -x.
	const NiMatrix33 yaw = EulerToMatrix(0.0f, 0.0f, 90.0f);
	pose = HandBoneWorld(yaw, camera, identity, NiPoint3{0.0f, 20.0f, 0.0f}, identity);
	Check(NearPoint(pose.pos, NiPoint3{80.0f, 200.0f, 300.0f}),
	      "a turned camera turns the offset with it");
	Check(NearMatrix(pose.rot, yaw), "and the bone turns with the camera");

	// The calibration is applied last, in the bone's own frame.
	const NiMatrix33 calibration = EulerToMatrix(0.0f, 0.0f, 90.0f);
	pose = HandBoneWorld(identity, camera, identity, offset, calibration);
	const NiPoint3 boneX = pose.rot * NiPoint3{1.0f, 0.0f, 0.0f};
	Check(NearPoint(boneX, NiPoint3{0.0f, 1.0f, 0.0f}),
	      "ninety degrees of calibration yaw lays the bone's x along the controller's y");
}

void TestLocalUnderParent() {
	std::printf("Back under the parent\n");
	const NiMatrix33 parentRot = EulerToMatrix(20.0f, -35.0f, 110.0f);
	const NiPoint3 parentPos{5.0f, -7.0f, 12.0f};
	BonePose wanted;
	wanted.rot = EulerToMatrix(-40.0f, 15.0f, 60.0f);
	wanted.pos = NiPoint3{30.0f, 40.0f, 50.0f};

	const BonePose local = LocalUnderParent(parentRot, parentPos, 1.0f, wanted);
	// parent * local must be the wanted world pose again.
	const NiMatrix33 worldRot = parentRot * local.rot;
	const NiPoint3 worldPos = parentPos + parentRot * local.pos;
	Check(NearMatrix(worldRot, wanted.rot), "the parent times the local is the wanted rotation");
	Check(NearPoint(worldPos, wanted.pos), "and the wanted position");

	const BonePose scaled = LocalUnderParent(parentRot, parentPos, 2.0f, wanted);
	const NiPoint3 scaledWorld = parentPos + (parentRot * scaled.pos) * 2.0f;
	Check(NearPoint(scaledWorld, wanted.pos), "a scaled parent is divided out");

	const BonePose guarded = LocalUnderParent(parentRot, parentPos, 0.0f, wanted);
	Check(NearPoint(guarded.pos, local.pos), "a zero scale is taken as one, not divided by");
	const BonePose negative = LocalUnderParent(parentRot, parentPos, -3.0f, wanted);
	Check(NearPoint(negative.pos, local.pos), "and so is a negative one");

	const BonePose plain = LocalUnderParent(NiMatrix33::Identity(), NiPoint3{0, 0, 0}, 1.0f, wanted);
	Check(NearMatrix(plain.rot, wanted.rot) && NearPoint(plain.pos, wanted.pos),
	      "under an identity parent the local is the world");
}

void TestCalibration() {
	std::printf("Calibration\n");
	const NiMatrix33 none = HandCalibration(0.0f, 0.0f, 0.0f);
	Check(NearMatrix(none, NiMatrix33::Identity()), "no angles is the identity");
	const NiMatrix33 yaw = HandCalibration(0.0f, 0.0f, 90.0f);
	Check(NearPoint(yaw * NiPoint3{1, 0, 0}, NiPoint3{0, 1, 0}), "yaw alone turns x onto y");
	// A roll about the bone's own axis comes first, so it does not move the
	// axis the yaw then lays along the controller.
	const NiMatrix33 rolled = HandCalibration(45.0f, 0.0f, 90.0f);
	Check(NearPoint(rolled * NiPoint3{1, 0, 0}, NiPoint3{0, 1, 0}),
	      "a roll before the yaw leaves the bone's axis where the yaw puts it");
	Check(!NearMatrix(rolled, yaw), "but is a different rotation");
}

}  // namespace

int main() {
	TestWorldPose();
	TestLocalUnderParent();
	TestCalibration();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
