#pragma once

#include "core/Types.h"
#include "game/BoneMirror.h"
#include "game/GameTypes.h"

// The arm bones' partners (BoneMirror.h) read from the body's own skins:
// the skinned shapes under the body's root are walked, and from the first
// shape that carries each of the twelve arm bones its skin-to-bone
// transform is taken (NiSkinInstance +0x08 the NiSkinData, +0x14 the bone
// pointers; NiSkinData +0x40 the bone count, +0x44 the bones' data, 0x4C
// each with the skin-to-bone transform first - BodyView.cpp, ArmStump.cpp).
// Every body part's skin sits in the skeleton root's space, so one mirror
// serves them all.

namespace obvr::game {

struct ArmBonePartnersRead {
	ArmBonePartners partners;
	UInt32 shapesWalked = 0;
	UInt32 shapesSkinned = 0;
	char shapes[160] = "";  // the names of the shapes a bone was taken from
	const char* why = nullptr;  // when not valid
};

// `armBones` are the twelve body arm bones, right clavicle to right hand
// then left clavicle to left hand (BodyIk.cpp's order); a null bone is
// counted missing.
bool ReadArmBonePartners(const NiAVObject* bodyRoot, NiAVObject* const* armBones, ArmBonePartnersRead& out);

}  // namespace obvr::game
