#include "game/HandBones.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/BonePin.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(const void* pointer) {
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer));
}

// One hand's bone, found under one root. The root is remembered so a new
// model - a race change, a load - starts a new search rather than writing to
// a bone that went away with the old tree.
struct HandBone {
	const NiAVObject* root = nullptr;
	NiAVObject* bone = nullptr;
	bool searched = false;
	bool reported = false;
};

HandBone g_hands[2];
UInt32 g_missingReportsLeft = 4;

const char* NameOf(const NiAVObject* node) {
	const char* const name = node->name;
	if (!LooksLikeObject(name)) {
		return "";
	}
	for (UInt32 at = 0; at < 64; ++at) {
		if (name[at] == '\0') {
			return name;
		}
		if (name[at] < 0x20 || name[at] > 0x7E) {
			return "";
		}
	}
	return "";
}

}  // namespace

void ForgetHandBones() {
	g_hands[0] = HandBone{};
	g_hands[1] = HandBone{};
}

bool PinHandBone(bool rightHand, const char* boneName, const NiMatrix33& relativeRot,
                 const NiPoint3& offsetUnits, const NiMatrix33& calibration) {
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr) {
		return false;
	}
	HandBone& hand = g_hands[rightHand ? 0 : 1];
	if (hand.root != root) {
		hand = HandBone{};
		hand.root = root;
	}
	if (!hand.searched) {
		hand.searched = true;
		hand.bone = FindFirstPersonNode(boneName);
		if (hand.bone == nullptr) {
			if (g_missingReportsLeft > 0) {
				--g_missingReportsLeft;
				OBVR_LOG("Hand bones: no node named \"%s\" under the first-person root - the %s "
				         "hand stays with the animation (Debug.FirstPersonTreeProbe lists the "
				         "names)",
				         boneName, rightHand ? "right" : "left");
			}
			return false;
		}
	}
	NiAVObject* const bone = hand.bone;
	if (bone == nullptr) {
		return false;
	}

	// The camera's frame is the root's parent's, the space the arms are
	// placed in; the bone's parent's world transform is current because the
	// arm placement just ran the update pass over the whole tree.
	NiAVObject* const cameraNode = root->parent;
	NiAVObject* const parent = bone->parent;
	if (!LooksLikeObject(cameraNode) || !LooksLikeObject(parent)) {
		return false;
	}

	const BonePose wanted =
		HandBoneWorld(cameraNode->worldTransform.rot, cameraNode->worldTransform.pos,
		              relativeRot, offsetUnits, calibration);
	const BonePose local = LocalUnderParent(parent->worldTransform.rot,
	                                        parent->worldTransform.pos,
	                                        parent->worldTransform.scale, wanted);
	bone->localTransform.rot = local.rot;
	bone->localTransform.pos = local.pos;
	UpdateNodeTransforms(bone);

	if (!hand.reported) {
		hand.reported = true;
		OBVR_LOG("Hand bones: the %s hand is \"%s\" at %08X under \"%s\", written each frame "
		         "to where the controller is (camera frame \"%s\")",
		         rightHand ? "right" : "left", NameOf(bone), reinterpret_cast<UInt32>(bone),
		         NameOf(parent), NameOf(cameraNode));
	}
	return true;
}

}  // namespace obvr::game
