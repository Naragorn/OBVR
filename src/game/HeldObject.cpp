#include "game/HeldObject.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/BonePin.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/GrabPhysics.h"
#include "game/HandBones.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

// The hold being followed: which reference, its node, and how it sits in
// the hand. `attached` false for a hold that is not placed on the hand.
struct Hold {
	UInt32 ref = 0;
	NiAVObject* node = nullptr;
	HeldAttachment attachment;
	bool attached = false;
	// The float to the hand: where the held point lay, and how far along.
	NiPoint3 floatFrom{0.0f, 0.0f, 0.0f};
	bool floatStarted = false;
	float floatSeconds = 0.0f;
	float floatElapsed = 0.0f;
};

Hold g_hold;
UInt32 g_reportsLeft = 6;

}  // namespace

void StepHeldObject(bool enabled, bool holding, const HeldHand& hand, bool haveTouched,
                    const NiPoint3& touched, bool attachAll, float dtSeconds) {
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
	if (!ReadHandBoneWorld(hand.rightHand, handRot, handPos)) {
		return;
	}
	if (g_hold.ref != ref) {
		// A new hold: placed or not, and how it sits against the hand.
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
		const NiTransform& world = g_hold.node->worldTransform;
		if (AttachesInHand(attachAll, small, haveTouched)) {
			// Turned against the hand as it lay in the world when the grip
			// closed; the side the marker showed - the point the pick touched -
			// in the grip, the middle when there is no such point.
			g_hold.attachment =
				CaptureAttachment(handRot, world.rot, world.pos, world.scale,
				                  haveTouched ? touched : g_hold.node->worldBound.center);
			g_hold.attached = true;
			g_hold.floatFrom = haveTouched ? touched : g_hold.node->worldBound.center;
		}
		if (g_reportsLeft > 0) {
			--g_reportsLeft;
			OBVR_LOG("Hands: holding %08X (form type %02X, bound radius %.1f units) - %s", ref,
			         type, static_cast<double>(radius),
			         g_hold.attached ? (haveTouched ? "in the hand as it lay, the marked side in "
			                                          "the grip, turning with the wrist"
			                                        : "in the hand as it lay, its middle in the "
			                                          "grip, turning with the wrist")
			         : small         ? "small, but no touched point: on the spring"
			                         : "not small: on the spring");
		}
	}
	if (!g_hold.attached || !LooksLikeObject(reinterpret_cast<UInt32>(g_hold.node))) {
		return;
	}
	NiAVObject* const node = g_hold.node;
	NiAVObject* const parent = node->parent;
	const float scale = node->worldTransform.scale;
	const NiPoint3 grip =
		hand.haveGripPoint ? hand.gripPoint : PalmPoint(handRot, handPos, hand.palmAlongUnits);
	// Floating in: the held point from where it lay to the grip (FloatWeight),
	// timed from the first frame it is placed.
	if (!g_hold.floatStarted) {
		g_hold.floatStarted = true;
		g_hold.floatSeconds = FloatSeconds(math::Sqrt((grip - g_hold.floatFrom).LengthSquared()));
		g_hold.floatElapsed = 0.0f;
	} else if (dtSeconds > 0.0f && dtSeconds < 0.5f) {
		g_hold.floatElapsed += dtSeconds;
	}
	const NiPoint3 held = FloatPoint(g_hold.floatFrom, grip,
	                                 FloatWeight(g_hold.floatElapsed, g_hold.floatSeconds));
	const HeldPose pose = AttachedPose(handRot, held, g_hold.attachment, scale);
	// The node's world transform is written directly and only its children
	// are updated from it: running the node's own update puts a Havok-driven
	// object back where its rigid body is, which is why the object followed
	// the spring's position but never turned with the wrist (2026-09-26).
	node->worldTransform.rot = pose.rot;
	node->worldTransform.pos = pose.pos;
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
	UpdateChildTransforms(node);
	NoteHeldPose(g_hold.ref, pose.rot, pose.pos);
}

}  // namespace obvr::game
