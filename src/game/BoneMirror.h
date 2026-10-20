#pragma once

#include "game/NiMath.h"

// The partner of a bone on the other side of a symmetric skeleton, from the
// skin's bind poses - what lays the first-person arm bones onto the body's
// arm of the other side when the first-person skeleton is drawn mirrored
// (left-handed play, BonePin.h "The mirror"; BodyIk.cpp AlignFirstPersonArm).
//
// The body's two arms are mirror images of each other in the bind pose, but
// their bones' frames are not plain mirror images: a Bip01 left bone's axes
// are the right one's turned about x (HandMode.h, leftHandRoll), and OBVR
// has not read every bone's convention. The skin data carries it exactly:
// NiSkinData's "skin to bone" of each bone is the inverse of that bone's
// bind-pose transform in the skin's own space, and in that space the bind
// pose is symmetric about the yz plane (x to -x, the character facing +y).
// So a point v in the right bone's frame, mirrored, sits in the left bone's
// frame at inv(B_L) * S * B_R * v - the partner P = skinToBone_L * S *
// inv(skinToBone_R) carries right-bone coordinates to left-bone coordinates
// of the twin. For the first-person right-arm bones (a right-arm mesh) to be
// drawn, mirrored, exactly where the body's left arm is, their world
// transform must be W_L * P^-1 = W_L * skinToBone_L * S * inv(skinToBone_R):
// PartnerWorld(W_L, MirrorPartner(skinToBone_L, skinToBone_R)) - and the
// body's hand of that side reaches the first-person hand carried the same
// way back (its own side's skinToBone first).
//
// Pure, so the arithmetic is checked by bone_mirror_test with made-up bind
// poses.

namespace obvr::game {

// a after b: (a * b)(v) = a(b(v)), scale included.
inline NiTransform ComposeTransforms(const NiTransform& a, const NiTransform& b) {
	NiTransform out;
	out.rot = a.rot * b.rot;
	out.scale = a.scale * b.scale;
	out.pos = a.pos + a.rot * (b.pos * a.scale);
	return out;
}

// The transform that undoes t (its rotation may be a mirror, so the true
// inverse, never the transpose alone). A scale of zero or less reads as 1.
inline NiTransform InverseTransform(const NiTransform& t) {
	NiTransform out;
	const float scale = t.scale > 0.0f ? t.scale : 1.0f;
	out.rot = InverseMatrix(t.rot);
	out.scale = 1.0f / scale;
	out.pos = (out.rot * t.pos) * (-1.0f / scale);
	return out;
}

// The mirror of the skin's space: x to -x.
inline NiTransform SkinMirror() {
	NiTransform s;
	s.rot = MirrorX();
	s.pos = NiPoint3{0.0f, 0.0f, 0.0f};
	s.scale = 1.0f;
	return s;
}

// From one bone's frame to its partner's on the other side, through the
// bind pose: skinToBoneFrom * S * inv(skinToBoneTo). Carries a world pose
// of the `from` bone to the world pose a mirrored `to` bone must have to
// coincide with it (PartnerWorld).
inline NiTransform MirrorPartner(const NiTransform& skinToBoneFrom, const NiTransform& skinToBoneTo) {
	return ComposeTransforms(ComposeTransforms(skinToBoneFrom, SkinMirror()), InverseTransform(skinToBoneTo));
}

inline NiTransform PartnerWorld(const NiTransform& world, const NiTransform& partner) {
	return ComposeTransforms(world, partner);
}

// The six arm parts of each side, as BodyIk.cpp counts them: clavicle,
// upper arm, its twist, forearm, its twist, hand.
inline constexpr UInt32 kArmPartCount = 6;

// The partners of both arms: toOther[side][part] carries a world pose of
// that side's bone to the other side's mirrored bone. Read once per body
// (BoneMirrorRead.h).
struct ArmBonePartners {
	NiTransform toOther[2][kArmPartCount] = {};
	bool found[2][kArmPartCount] = {};
	bool valid = false;
};

// Both sides' partners from the twelve skin-to-bone transforms (right side
// first, then left, each clavicle to hand). False when any is missing.
inline bool ArmPartnersFrom(const NiTransform* skinToBone, const bool* have, ArmBonePartners& out) {
	out = ArmBonePartners{};
	bool all = true;
	for (UInt32 part = 0; part < kArmPartCount; ++part) {
		const bool both = have[part] && have[kArmPartCount + part];
		out.found[0][part] = have[part];
		out.found[1][part] = have[kArmPartCount + part];
		if (!both) {
			all = false;
			continue;
		}
		out.toOther[0][part] = MirrorPartner(skinToBone[part], skinToBone[kArmPartCount + part]);
		out.toOther[1][part] = MirrorPartner(skinToBone[kArmPartCount + part], skinToBone[part]);
	}
	out.valid = all;
	return all;
}

}  // namespace obvr::game
