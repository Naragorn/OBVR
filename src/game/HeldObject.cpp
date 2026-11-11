#include "game/HeldObject.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/BonePin.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/GrabPhysics.h"
#include "game/HandBones.h"
#include "game/PullTurnLogic.h"

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
	// Said once per hold: in the hand (HeldObjectArrived).
	bool arrived = false;
	// A weapon turns on the way to the grip's turn (PullTurnLogic.h).
	bool weaponRead = false;
	bool isWeapon = false;
	PullTurn turn;
	PullTurnWatch watch;
	UInt32 turnLines = 0;
};

Hold g_hold;
bool g_arrivedEvent = false;
bool g_arrivedRightHand = true;
UInt32 g_reportsLeft = 6;

// The weapon's turn against the hand this frame (PullTurnLogic.h): begun at
// the first placed frame from its lay, towards the grip's turn while the grip
// is known; the angle to the grip logged on the way and at the catch.
const HeldAttachment& TurnHeldWeapon(Hold& h, const NiMatrix33& handRot, const HeldHand& hand, float floated,
                                     float dt) {
	if (!h.turn.active && !h.weaponRead) {
		h.weaponRead = true;
		const UInt32 base = *reinterpret_cast<const UInt32*>(h.ref + addr::kRefBaseFormOffset);
		h.isWeapon = LooksLikeObject(base) && *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset) ==
		                                          addr::kFormTypeWeapon;
	}
	if (!h.isWeapon || !hand.haveGripPoint) {
		return h.attachment;
	}
	if (!h.turn.active) {
		BeginPullTurn(h.turn, h.attachment.relativeRot);
		h.turnLines = 70;
	}
	const NiMatrix33 gripRelative = InverseRotation(handRot) * hand.grip.rot;
	h.attachment.relativeRot = StepPullTurn(h.turn, gripRelative, floated, dt);
	const float angle = AngleBetweenRotations(h.turn.relative, gripRelative);
	if (h.turnLines > 0) {
		WatchPullTurn(h.watch, angle);
		--h.turnLines;
		const bool caught = floated >= 1.0f;
		OBVR_LOG("Hands: the pulled weapon %08X %.0f%% of the way, %.1f degrees from the grip's turn%s%s", h.ref,
		         static_cast<double>(floated * 100.0f), static_cast<double>(angle * 57.29578f),
		         !caught ? "" : PullTurnCaughtClose(angle) ? " - at the catch, close to the grip's turn"
		                                                   : " - at the catch, STILL OFF the grip's turn",
		         !caught ? "" : PullTurnNeverTurnedAway(h.watch) ? ", it never turned away" : ", it TURNED AWAY on the way");
		h.turnLines = caught ? 0 : h.turnLines;
	}
	return h.attachment;
}

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
		// Not placed on the hand (the spring holds it): in the hand at once.
		if (!g_hold.attached) {
			g_hold.arrived = true;
			g_arrivedEvent = true;
			g_arrivedRightHand = hand.rightHand;
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
		hand.haveGripPoint ? hand.grip.pos : PalmPoint(handRot, handPos, hand.palmAlongUnits);
	// Floating in: the held point from where it lay to the grip (FloatWeight),
	// timed from the first frame it is placed.
	if (!g_hold.floatStarted) {
		g_hold.floatStarted = true;
		g_hold.floatSeconds = FloatSeconds(math::Sqrt((grip - g_hold.floatFrom).LengthSquared()),
		                                   hand.floatUnitsPerSecond);
		g_hold.floatElapsed = 0.0f;
	} else if (dtSeconds > 0.0f && dtSeconds < 0.5f) {
		g_hold.floatElapsed += dtSeconds;
	}
	const float floated = FloatWeight(g_hold.floatElapsed, g_hold.floatSeconds);
	const NiPoint3 held = FloatPoint(g_hold.floatFrom, grip, floated);
	if (!g_hold.arrived && floated >= 1.0f) {
		g_hold.arrived = true;
		g_arrivedEvent = true;
		g_arrivedRightHand = hand.rightHand;
	}
	const HeldPose pose = AttachedPose(handRot, held, TurnHeldWeapon(g_hold, handRot, hand, floated, dtSeconds), scale);
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

bool HeldObjectArrived() { return g_hold.arrived; }

bool TakeHeldObjectArrival(bool& rightHand) {
	if (!g_arrivedEvent) {
		return false;
	}
	g_arrivedEvent = false;
	rightHand = g_arrivedRightHand;
	return true;
}

}  // namespace obvr::game
