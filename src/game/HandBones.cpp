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
                 const NiPoint3& offsetUnits, const NiMatrix33& calibration,
                 const NiMatrix33& cameraRot, const NiPoint3& cameraPos) {
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

	// The hand is carried by its parent, the forearm: the forearm is placed so
	// that the hand, with the local transform the animation gave it, lands
	// where the controller is - see ParentForChildAt for why not the hand
	// alone. The world transforms read are current because the arm placement
	// just ran the update pass over the whole tree.
	NiAVObject* const parent = bone->parent;
	NiAVObject* const grandparent = LooksLikeObject(parent) ? parent->parent : nullptr;
	if (!LooksLikeObject(parent) || !LooksLikeObject(grandparent)) {
		return false;
	}

	const BonePose wanted = HandBoneWorld(cameraRot, cameraPos, relativeRot, offsetUnits,
	                                      calibration);
	const BonePose parentWorld = ParentForChildAt(wanted, bone->localTransform.rot,
	                                              bone->localTransform.pos,
	                                              parent->worldTransform.scale);
	const BonePose local = LocalUnderParent(grandparent->worldTransform.rot,
	                                        grandparent->worldTransform.pos,
	                                        grandparent->worldTransform.scale, parentWorld);
	parent->localTransform.rot = local.rot;
	parent->localTransform.pos = local.pos;
	UpdateNodeTransforms(parent);

	if (!hand.reported) {
		hand.reported = true;
		OBVR_LOG("Hand bones: the %s hand is \"%s\" at %08X under \"%s\", written each frame "
		         "to where the controller is - at (%.1f, %.1f, %.1f), the camera at "
		         "(%.1f, %.1f, %.1f)",
		         rightHand ? "right" : "left", NameOf(bone), reinterpret_cast<UInt32>(bone),
		         NameOf(parent), static_cast<double>(wanted.pos.x),
		         static_cast<double>(wanted.pos.y), static_cast<double>(wanted.pos.z),
		         static_cast<double>(cameraPos.x), static_cast<double>(cameraPos.y),
		         static_cast<double>(cameraPos.z));
	}
	return true;
}

}  // namespace obvr::game
