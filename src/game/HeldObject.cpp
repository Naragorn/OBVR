#include "game/HeldObject.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/AnchoredRef.h"
#include "game/BonePin.h"
#include "game/BowHand.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/GrabPhysics.h"
#include "game/HandBones.h"
#include "game/NearbyItems.h"
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
	// A weapon in the weapon hand, a torch in the other, turns on the way to
	// the grip's turn (PullTurnLogic.h, PullTurnsToGrip); read at the new hold.
	bool turns = false;
	bool isTorch = false;
	bool isBow = false;
	PullTurn turn;
	PullTurnWatch watch;
	UInt32 turnLines = 0;
};

Hold g_hold;
bool g_arrivedEvent = false;
bool g_arrivedRightHand = true;
UInt32 g_reportsLeft = 6;

// What the new hold is - a torch, a bow - and whether it turns to the grip on the way.
void ReadKindOfHold(Hold& h, UInt32 ref, UInt8 type, bool weaponHand) {
	const bool isWeapon = type == addr::kFormTypeWeapon;
	h.isTorch = RefIsTorch(ref);
	h.isBow = isWeapon && IsBowType(RefWeaponType(ref));
	h.turns = PullTurnsToGrip(weaponHand, isWeapon, h.isTorch, h.isBow);
}

// The weapon's or torch's turn against the hand this frame (PullTurnLogic.h):
// begun at the first placed frame from its lay, towards the grip's turn while
// the grip is known; the angle to the grip logged on the way and at the catch.
const HeldAttachment& TurnHeldWeapon(Hold& h, const NiMatrix33& handRot, const HeldHand& hand, float floated,
                                     float dt) {
	if (!h.turns || !hand.haveGripPoint) {
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
		OBVR_LOG("Hands: the pulled %s %08X %.0f%% of the way, %.1f degrees from the grip's turn%s%s",
		         h.isTorch ? "torch" : h.isBow ? "bow" : "weapon", h.ref, static_cast<double>(floated * 100.0f),
		         static_cast<double>(angle * 57.29578f),
		         !caught ? "" : PullTurnCaughtClose(angle) ? " - at the catch, close to the grip's turn"
		                                                   : " - at the catch, STILL OFF the grip's turn",
		         !caught ? "" : PullTurnNeverTurnedAway(h.watch) ? ", it never turned away" : ", it TURNED AWAY on the way");
		h.turnLines = caught ? 0 : h.turnLines;
	}
	return h.attachment;
}

// Where the held point floats to (PullHoldTarget): what turns, to where the
// equip will put it; else the weapon hand's grip node or the other's palm.
NiPoint3 HoldTarget(const Hold& h, const HeldHand& hand, const NiMatrix33& handRot, const NiPoint3& handPos,
                    float scale) {
	return PullHoldTarget(hand.rightHand, h.turns, hand.haveGripPoint, hand.grip,
	                      PalmPoint(handRot, handPos, hand.palmAlongUnits), h.attachment.pivotLocal, scale);
}

// A new hold, for the log: placed in the hand or left on the engine's spring, and why.
void SayNewHold(UInt32 ref, UInt8 type, float radius, bool haveTouched, bool anchored, bool small) {
	if (g_reportsLeft == 0) {
		return;
	}
	--g_reportsLeft;
	OBVR_LOG("Hands: holding %08X (form type %02X, bound radius %.1f units) - %s", ref, type,
	         static_cast<double>(radius),
	         g_hold.attached ? (haveTouched ? "in the hand as it lay, the marked side in the grip, turning with the wrist"
	                                        : "in the hand as it lay, its middle in the grip, turning with the wrist")
	         : anchored      ? "a fixed part under it: on the spring, its frame left where it stands"
	         : small         ? "small, but no touched point: on the spring"
	                         : "not small: on the spring");
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
		ReadKindOfHold(g_hold, ref, type, hand.rightHand);
		const float radius = g_hold.node->worldBound.radius;
		const bool small = IsSmallHeldObject(type, radius);
		const NiTransform& world = g_hold.node->worldTransform;
		// A frame with parts hanging from it (game/AnchoredRef.h) is not put in the
		// hand: its root node is the frame. The engine's spring keeps the part it took.
		const bool anchored = !RefMayBeCarried(ref);
		if (!anchored && AttachesInHand(attachAll, small, haveTouched)) {
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
		SayNewHold(ref, type, radius, haveTouched, anchored, small);
	}
	if (!g_hold.attached || !LooksLikeObject(reinterpret_cast<UInt32>(g_hold.node))) {
		return;
	}
	NiAVObject* const node = g_hold.node;
	NiAVObject* const parent = node->parent;
	const float scale = node->worldTransform.scale;
	const NiPoint3 grip = HoldTarget(g_hold, hand, handRot, handPos, scale);
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
