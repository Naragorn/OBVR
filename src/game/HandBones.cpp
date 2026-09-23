#include "game/HandBones.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/BonePin.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "vr/ControllerActions.h"

namespace obvr::game {
namespace {

constexpr unsigned kMaxFingerBones = vr::input::HandBoneCount;

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

// Finger bones for one hand: each SteamVR bone index maps to a game bone by
// name. The root is remembered so a new model starts a fresh search.
struct FingerBoneCache {
	const NiAVObject* root = nullptr;
	struct Entry {
		NiAVObject* bone = nullptr;
		bool searched = false;
	} entries[kMaxFingerBones]{};
};

FingerBoneCache g_fingers[2];  // [0] right, [1] left
bool g_fingerReported = false;

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

// Ensure the finger bone cache is valid for this root, searching bones by
// name as needed. Returns a pointer to the Entry for the given SteamVR index,
// or nullptr if no game bone name was provided for that index.
FingerBoneCache::Entry* FindFingerBone(FingerBoneCache& cache, const NiAVObject* root,
                                       unsigned steamvrIndex, const char* boneName) {
	if (steamvrIndex >= kMaxFingerBones || boneName == nullptr || boneName[0] == '\0') {
		return nullptr;
	}

	if (cache.root != root) {
		cache = FingerBoneCache{};
		cache.root = root;
	}

	FingerBoneCache::Entry& entry = cache.entries[steamvrIndex];
	if (!entry.searched) {
		entry.searched = true;
		entry.bone = FindFirstPersonNode(boneName);
	}
	return entry.bone != nullptr ? &entry : nullptr;
}

// Pin one finger bone given its world pose. Returns true if written.
bool WriteFingerBone(NiAVObject* cameraNode, NiAVObject* bone, const BonePose& wanted) {
	if (bone == nullptr || !LooksLikeObject(cameraNode)) {
		return false;
	}
	NiAVObject* const parent = bone->parent;
	if (!LooksLikeObject(parent)) {
		return false;
	}

	const BonePose local = LocalUnderParent(parent->worldTransform.rot,
	                                        parent->worldTransform.pos,
	                                        parent->worldTransform.scale, wanted);
	bone->localTransform.rot = local.rot;
	bone->localTransform.pos = local.pos;
	UpdateNodeTransforms(bone);
	return true;
}

}  // namespace

void ForgetHandBones() {
	g_hands[0] = HandBone{};
	g_hands[1] = HandBone{};
	g_fingers[0] = FingerBoneCache{};
	g_fingers[1] = FingerBoneCache{};
	g_fingerReported = false;
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

unsigned PinFingerBones(bool rightHand, const NiMatrix33& handRelativeRot,
                        const NiPoint3& handOffsetUnits, const NiMatrix33& calibration,
                        const vr::input::VRBoneTransform* skeletalBones, unsigned boneCount,
                        const char* fingerBoneNames[vr::input::HandBoneCount]) {
	if (skeletalBones == nullptr || fingerBoneNames == nullptr || boneCount == 0) {
		return 0;
	}

	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr) {
		return 0;
	}

	FingerBoneCache& cache = g_fingers[rightHand ? 0 : 1];
	NiAVObject* const cameraNode = root->parent;
	if (!LooksLikeObject(cameraNode)) {
		return 0;
	}

	// Compute the hand bone's world pose - same as PinHandBone does. This is
	// the frame each finger bone's model-space transform composes with.
	const BonePose handWorld = HandBoneWorld(cameraNode->worldTransform.rot,
	                                         cameraNode->worldTransform.pos,
	                                         handRelativeRot, handOffsetUnits, calibration);

	unsigned pinned = 0;
	for (unsigned i = 0; i < boneCount && i < kMaxFingerBones; ++i) {
		const char* const name = fingerBoneNames[i];
		if (name == nullptr || name[0] == '\0') {
			continue;
		}

		FingerBoneCache::Entry* entry = FindFingerBone(cache, root, i, name);
		if (entry == nullptr) {
			continue;
		}

		NiAVObject* const bone = entry->bone;
		const vr::input::VRBoneTransform& svr = skeletalBones[i];

		// Convert SteamVR model-space transform to game convention. The position
		// is in metres relative to the controller grip origin, OpenVR axes:
		// X right, Y up, -Z forward. We need game units and game axes:
		// X right, Y forward, Z up. Scale by 70 (same as HandBoneWorld).
		const float unitsPerMetre = 70.0f;
		const NiPoint3 relPosGame = PositionFromOpenVR(svr.position[0], svr.position[1],
		                                               svr.position[2]) * unitsPerMetre;

		// Convert quaternion from OpenVR convention to game convention, then to
		// a rotation matrix. SteamVR stores quaternions as (w,x,y,z).
		const Quaternion relQuatGame = QuatFromOpenVR(svr.qx, svr.qy, svr.qz, svr.qw);
		const NiMatrix33 relRotGame = ToMatrix(relQuatGame);

		// Compose with hand world pose: each finger bone's transform is relative
		// to the controller grip origin (skeleton root), so we compose it into
		// world space using the hand bone's calibrated world pose as parent.
		const BonePose wanted = ComposeWorld(handWorld.rot, handWorld.pos, relRotGame, relPosGame);

		if (WriteFingerBone(cameraNode, bone, wanted)) {
			++pinned;
		}
	}

	if (!g_fingerReported && pinned > 0) {
		g_fingerReported = true;
		OBVR_LOG("Hand bones: finger tracking active for %s hand (%u bones pinned this frame)",
		         rightHand ? "right" : "left", pinned);
	}

	return pinned;
}

}  // namespace obvr::game
