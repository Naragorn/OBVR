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
	// The bare wrist (BonePin.h): whether the forearm is shrunk now, and the
	// local scales it and the hand had before.
	bool tapered = false;
	float savedForearmScale = 1.0f;
	float savedHandScale = 1.0f;
	bool taperReported = false;
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

// The bare wrists, decided once a frame (SetBareWristTaper).
bool g_taperWanted = false;
bool g_handsBare = false;
UInt32 g_bareLinesLeft = 6;

// LookupFormByID (xOBSE obse/GameAPI.cpp: 0x0046B250, cdecl, the form or
// null).
constexpr UInt32 kLookupFormById = 0x0046B250;

// Whether the first-person hands are bare: a node "Hand  (<id>)" whose form
// is a race. A glove's or gauntlet's form is armour or clothing; a robe with
// hands brings them under its UpperBody node, and no Hand node is loaded.
bool HandsAreBare() {
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr) {
		return false;
	}
	NiAVObject* nodes[4] = {};
	const UInt32 found = CollectNodesContaining(root, "Hand  (", nodes, 4);
	for (UInt32 i = 0; i < found; ++i) {
		UInt32 id = 0;
		if (!FormIdInNodeName(NameOf(nodes[i]), id)) {
			continue;
		}
		using LookupFn = const UInt8*(__cdecl*)(UInt32 id);
		const UInt8* const form = reinterpret_cast<LookupFn>(kLookupFormById)(id);
		if (LooksLikeObject(form) && form[addr::kFormTypeOffset] == kFormTypeRace) {
			return true;
		}
	}
	return false;
}

// Whether anything but bones hangs on this forearm's twist bone - a shield
// hangs on the left one, and would shrink with it.
bool SomethingOnForearm(const NiAVObject* forearm) {
	const UInt8* const bytes = reinterpret_cast<const UInt8*>(forearm);
	UInt8* const* const children =
		*reinterpret_cast<UInt8* const* const*>(bytes + addr::kNiChildrenOffset);
	const UInt16 count = *reinterpret_cast<const UInt16*>(bytes + addr::kNiChildCountOffset);
	if (!LooksLikeObject(children) || count > 64) {
		return true;  // cannot be told: left as it is
	}
	for (UInt16 i = 0; i < count; ++i) {
		const NiAVObject* const child = reinterpret_cast<const NiAVObject*>(children[i]);
		if (!LooksLikeObject(child)) {
			continue;
		}
		const char* const name = NameOf(child);
		bool twist = false;
		for (const char* at = name; *at != '\0' && !twist; ++at) {
			twist = at[0] == 'T' && at[1] == 'w' && at[2] == 'i' && at[3] == 's' && at[4] == 't';
		}
		if (!twist || !NiClassIsNode(NiClassNameOf(child))) {
			continue;
		}
		const UInt8* const twistBytes = reinterpret_cast<const UInt8*>(child);
		UInt8* const* const hung =
			*reinterpret_cast<UInt8* const* const*>(twistBytes + addr::kNiChildrenOffset);
		const UInt16 hungCount = *reinterpret_cast<const UInt16*>(twistBytes + addr::kNiChildCountOffset);
		if (!LooksLikeObject(hung) || hungCount > 64) {
			return true;
		}
		for (UInt16 k = 0; k < hungCount; ++k) {
			const NiAVObject* const thing = reinterpret_cast<const NiAVObject*>(hung[k]);
			if (!LooksLikeObject(thing)) {
				continue;
			}
			const char* const thingName = NameOf(thing);
			const bool bone = thingName[0] == 'B' && thingName[1] == 'i' && thingName[2] == 'p';
			if (!bone) {
				return true;
			}
		}
	}
	return false;
}

}  // namespace

bool FirstPersonHandsBare() { return HandsAreBare(); }

void SetBareWristTaper(bool enabled) {
	g_taperWanted = enabled;
	const bool bare = enabled && HandsAreBare();
	if (bare != g_handsBare && g_bareLinesLeft > 0) {
		--g_bareLinesLeft;
		OBVR_LOG("Hand bones: the hands are %s - their wrists %s", bare ? "bare" : "covered",
		         bare ? "closed (the forearm drawn into the cuff)" : "as the mesh has them");
	}
	g_handsBare = bare;
}

void RestoreHandBoneScales() {
	for (HandBone& hand : g_hands) {
		if (!hand.tapered) {
			continue;
		}
		hand.tapered = false;
		NiAVObject* const bone = hand.bone;
		if (hand.root != FirstPersonArmsNode() || !LooksLikeObject(bone) ||
		    !LooksLikeObject(bone->parent)) {
			continue;  // the tree it was written to is gone
		}
		bone->parent->localTransform.scale = hand.savedForearmScale;
		bone->localTransform.scale = hand.savedHandScale;
		UpdateNodeTransforms(bone->parent);
	}
}

void ForgetHandBones() {
	g_hands[0] = HandBone{};
	g_hands[1] = HandBone{};
}

bool PinHandBone(bool rightHand, const char* boneName, const NiMatrix33& relativeRot,
                 const NiPoint3& offsetUnits, const NiMatrix33& calibration,
                 const NiMatrix33& cameraRot, const NiPoint3& cameraPos,
                 const NiPoint3& gripUnits) {
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

	// The bare wrist (BonePin.h): the forearm shrunk into the cuff and the
	// hand grown back, or both as they were.
	const float scale =
		ForearmScaleFor(g_taperWanted, g_handsBare, SomethingOnForearm(parent));
	const float grandScale =
		grandparent->worldTransform.scale > 0.0f ? grandparent->worldTransform.scale : 1.0f;
	if (scale < 1.0f) {
		if (!hand.tapered) {
			hand.tapered = true;
			hand.savedForearmScale = parent->localTransform.scale;
			hand.savedHandScale = bone->localTransform.scale;
		}
		parent->localTransform.scale = scale / grandScale;
		bone->localTransform.scale = 1.0f / scale;
		if (!hand.taperReported) {
			hand.taperReported = true;
			OBVR_LOG("Hand bones: the %s forearm drawn to %.2f of its size, the hand grown back - "
			         "the bare wrist closed", rightHand ? "right" : "left",
			         static_cast<double>(scale));
		}
	} else if (hand.tapered) {
		hand.tapered = false;
		parent->localTransform.scale = hand.savedForearmScale;
		bone->localTransform.scale = hand.savedHandScale;
	}
	const float forearmWorldScale =
		hand.tapered ? scale : grandScale * parent->localTransform.scale;

	const BonePose wanted = HandBoneWorld(cameraRot, cameraPos, relativeRot, offsetUnits,
	                                      calibration, gripUnits);
	const BonePose parentWorld = ParentForChildAt(wanted, bone->localTransform.rot,
	                                              bone->localTransform.pos, forearmWorldScale);
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

bool ReadHandBoneWorld(bool rightHand, NiMatrix33& rot, NiPoint3& pos) {
	const HandBone& hand = g_hands[rightHand ? 0 : 1];
	if (!hand.reported || !LooksLikeObject(hand.bone) || hand.root != FirstPersonArmsNode()) {
		return false;
	}
	rot = hand.bone->worldTransform.rot;
	pos = hand.bone->worldTransform.pos;
	return true;
}

}  // namespace obvr::game
