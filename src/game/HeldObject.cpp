#include "game/HeldObject.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/BonePin.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "game/GrabPhysics.h"
#include "game/HandBones.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

// The hold being followed: which reference, its node, and how it sits in
// the hand. `attached` false for a hold that is not small, or not readable.
struct Hold {
	UInt32 ref = 0;
	NiAVObject* node = nullptr;
	HeldAttachment attachment;
	bool attached = false;
};

Hold g_hold;
UInt32 g_reportsLeft = 6;

}  // namespace

void StepHeldObject(bool enabled, bool holding, bool rightHand, bool haveTouched,
                    const NiPoint3& touched, float palmAlongUnits, bool attachAll) {
	const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	const UInt32 ref = holding && LooksLikeObject(player)
	                       ? *reinterpret_cast<const UInt32*>(player + addr::kPlayerGrabbedRefOffset)
	                       : 0;
	if (!enabled || !LooksLikeObject(ref)) {
		g_hold = Hold{};
		return;
	}
	NiMatrix33 handRot;
	NiPoint3 handPos;
	if (!ReadHandBoneWorld(rightHand, handRot, handPos)) {
		return;
	}
	if (g_hold.ref != ref) {
		// A new hold: small or not, and how it sits against the hand now.
		g_hold = Hold{};
		g_hold.ref = ref;
		const UInt32 node = *reinterpret_cast<const UInt32*>(ref + addr::kRefNiNodeOffset);
		const UInt32 base = *reinterpret_cast<const UInt32*>(ref + addr::kRefBaseFormOffset);
		if (!LooksLikeObject(node) || !LooksLikeObject(base)) {
			return;
		}
		g_hold.node = reinterpret_cast<NiAVObject*>(node);
		const UInt8 type = *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset);
		const float radius = g_hold.node->worldBound.radius;
		const bool small = IsSmallHeldObject(type, radius);
		if ((small || attachAll) && haveTouched) {
			const NiTransform& world = g_hold.node->worldTransform;
			g_hold.attachment =
				CaptureAttachment(handRot, world.rot, world.pos, world.scale, touched);
			g_hold.attached = true;
		}
		if (g_reportsLeft > 0) {
			--g_reportsLeft;
			OBVR_LOG("Hands: holding %08X (form type %02X, bound radius %.1f units) - %s", ref,
			         type, static_cast<double>(radius),
			         g_hold.attached ? (attachAll ? "in the hand, turning with the wrist"
			                                      : "small: fixed in the palm, turning with the wrist")
			                         : (small ? "small, but no touched point: on the spring"
			                                  : "not small: on the spring"));
		}
	}
	if (!g_hold.attached || !LooksLikeObject(reinterpret_cast<UInt32>(g_hold.node))) {
		return;
	}
	NiAVObject* const node = g_hold.node;
	NiAVObject* const parent = node->parent;
	const float scale = node->worldTransform.scale;
	const HeldPose pose =
		AttachedPose(handRot, PalmPoint(handRot, handPos, palmAlongUnits), g_hold.attachment, scale);
	if (LooksLikeObject(reinterpret_cast<UInt32>(parent))) {
		BonePose wanted;
		wanted.rot = pose.rot;
		wanted.pos = pose.pos;
		const BonePose local = LocalUnderParent(parent->worldTransform.rot,
		                                        parent->worldTransform.pos,
		                                        parent->worldTransform.scale, wanted);
		node->localTransform.rot = local.rot;
		node->localTransform.pos = local.pos;
	} else {
		node->localTransform.rot = pose.rot;
		node->localTransform.pos = pose.pos;
	}
	UpdateNodeTransforms(node);
	NoteHeldPose(g_hold.ref, pose.rot, pose.pos);
}

}  // namespace obvr::game
