// Checks game/BoneMirror.h: a bone's partner on the other side from the
// skin's bind poses, for the first-person arms laid onto the body's other
// arm when the skeleton is drawn mirrored (left-handed play).

#include <cstdio>

#include "core/Rotation.h"
#include "game/BoneMirror.h"

namespace {

using obvr::EulerToMatrix;
using obvr::NiMatrix33;
using obvr::NiPoint3;
using obvr::NiTransform;
using namespace obvr::game;

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

bool NearTransform(const NiTransform& a, const NiTransform& b) {
	return NearMatrix(a.rot, b.rot) && NearPoint(a.pos, b.pos) && Near(a.scale, b.scale);
}

NiTransform Make(const NiMatrix33& rot, const NiPoint3& pos, float scale = 1.0f) {
	NiTransform t;
	t.rot = rot;
	t.pos = pos;
	t.scale = scale;
	return t;
}

NiPoint3 Apply(const NiTransform& t, const NiPoint3& v) { return t.pos + t.rot * (v * t.scale); }

void TestCompose() {
	std::printf("Composing transforms\n");
	const NiTransform a = Make(EulerToMatrix(10.0f, -20.0f, 35.0f), NiPoint3{1.0f, 2.0f, 3.0f}, 1.1f);
	const NiTransform b = Make(EulerToMatrix(-5.0f, 40.0f, -70.0f), NiPoint3{-4.0f, 0.5f, 2.0f}, 0.8f);
	const NiPoint3 v{3.0f, -1.0f, 2.0f};
	Check(NearPoint(Apply(ComposeTransforms(a, b), v), Apply(a, Apply(b, v))), "a after b applies b first, scale included");
	const NiTransform back = ComposeTransforms(InverseTransform(a), a);
	Check(NearMatrix(back.rot, NiMatrix33::Identity()) && NearPoint(back.pos, NiPoint3{0.0f, 0.0f, 0.0f}) &&
	          Near(back.scale, 1.0f),
	      "the inverse undoes it");
	NiTransform mirrored = a;
	mirrored.rot = obvr::MirrorX() * a.rot;
	Check(NearPoint(Apply(ComposeTransforms(InverseTransform(mirrored), mirrored), v), v),
	      "a mirrored transform inverts too (the true inverse, not the transpose)");
}

// A symmetric bind: the left bone's bind pose is the right's through the
// skin's mirror, then the left frame's own convention K (an improper turn
// in the bone's frame, as Bip01's left bones have).
void TestPartner() {
	std::printf("The partner through the bind pose\n");
	const NiTransform bindRight = Make(EulerToMatrix(15.0f, -30.0f, 50.0f), NiPoint3{12.0f, 3.0f, 40.0f});
	NiTransform convention = Make(EulerToMatrix(180.0f, 0.0f, 0.0f), NiPoint3{0.0f, 0.0f, 0.0f});
	convention.rot = convention.rot * obvr::MirrorX();  // turned about x, then mirrored: a proper left frame
	const NiTransform bindLeft = ComposeTransforms(ComposeTransforms(SkinMirror(), bindRight), convention);
	Check(Near(obvr::Determinant(bindLeft.rot), 1.0f), "the made-up left bind pose is a rotation");
	Check(NearPoint(bindLeft.pos, NiPoint3{-12.0f, 3.0f, 40.0f}), "and sits at the mirrored place");
	const NiTransform skinToBoneRight = InverseTransform(bindRight);
	const NiTransform skinToBoneLeft = InverseTransform(bindLeft);
	// From the body's left bone to the first-person right bone laid on it,
	// and the other way round for the hand's target.
	const NiTransform leftToRight = MirrorPartner(skinToBoneLeft, skinToBoneRight);
	const NiTransform rightToLeft = MirrorPartner(skinToBoneRight, skinToBoneLeft);
	// A point of the right arm in its bone's frame, and its twin in the
	// left bone's frame: the twin is where the mirror of the first is.
	const NiPoint3 v{5.0f, 2.0f, -1.0f};
	const NiPoint3 worldRight = Apply(bindRight, v);
	const NiPoint3 twinWorld{-worldRight.x, worldRight.y, worldRight.z};
	const NiPoint3 u = Apply(InverseTransform(bindLeft), twinWorld);
	Check(NearPoint(Apply(PartnerWorld(bindLeft, leftToRight), v), twinWorld),
	      "the right bone laid on the left arm's bind pose draws the right-bone point at its twin's place");
	Check(Near(obvr::Determinant(PartnerWorld(bindLeft, leftToRight).rot), -1.0f),
	      "the first-person right bone laid on the left arm is turned over");
	// Any pose of the left arm: the partner follows it.
	const NiTransform leftNow = Make(EulerToMatrix(-40.0f, 10.0f, 120.0f), NiPoint3{-20.0f, 15.0f, 35.0f});
	const NiTransform fpRight = PartnerWorld(leftNow, leftToRight);
	Check(NearPoint(Apply(fpRight, v), Apply(leftNow, u)),
	      "in that pose the right-bone point drawn mirrored lies on the left bone's twin point");
	Check(NearTransform(PartnerWorld(fpRight, rightToLeft), leftNow),
	      "carried back with the other side's partner it is the left arm's pose again (the hand's target)");
	// The table for both arms.
	NiTransform skinToBone[2 * kArmPartCount];
	bool have[2 * kArmPartCount];
	for (unsigned i = 0; i < 2 * kArmPartCount; ++i) {
		skinToBone[i] = i < kArmPartCount ? skinToBoneRight : skinToBoneLeft;
		have[i] = true;
	}
	ArmBonePartners partners;
	Check(ArmPartnersFrom(skinToBone, have, partners) && partners.valid, "all twelve: the table is valid");
	Check(NearTransform(partners.toOther[0][3], rightToLeft) && NearTransform(partners.toOther[1][3], leftToRight),
	      "right to left and left to right per part");
	have[kArmPartCount + 2] = false;  // the left upper arm's twist missing from the skin
	Check(!ArmPartnersFrom(skinToBone, have, partners) && !partners.valid && !partners.found[1][2] &&
	          partners.found[0][2],
	      "one bone missing: not valid, the missing one named");
}

}  // namespace

int main() {
	TestCompose();
	TestPartner();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
