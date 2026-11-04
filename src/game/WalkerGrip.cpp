#include "game/WalkerGrip.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/BonePin.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "game/WalkerGripLogic.h"

namespace obvr::game {
namespace {

// LookupFormByID (xOBSE obse/GameAPI.cpp: 0x0046B250, cdecl, the form or null).
constexpr UInt32 kLookupFormById = 0x0046B250;
// The reference's form type at +0x04. Measured 2026-11-04 (walker-grip-on): a
// walker's reference reads 51. DeepWiki's reading of xOBSE's GameForms.h
// gave ACHR 49 and ACRE 50 - one below the measurement, so the character
// reference is taken as 50 (derived from that offset, not measured: no NPC
// holds the player in the harness).
constexpr UInt8 kFormTypeCharacterRef = 50;
constexpr UInt8 kFormTypeCreatureRef = 51;
// [Debug] TestWalkersHold's pretend holder.
constexpr UInt32 kTestHolder = 0xFFFFFFFFu;

struct GripMemory {
	UInt32 refId = 0;
	WalkerHand hand = kWalkerNoHand;
	float weight = 0.0f;
	bool reported = false;
	const char* lastWhy = "";
};

// A reason the grip was not laid on, logged when it changes.
bool Refused(GripMemory& m, const char* why);

// The two arms' grips and the log's budget, kept across frames.
struct GripState {
	GripMemory arm[2];
	UInt32 linesLeft = 24;
	float sendSince = kForearmSendSeconds;
	UInt32 sendsTaken = 0;
	UInt32 sendsRefused = 0;
};

GripState& State() {
	static GripState s_state;
	return s_state;
}

bool Readable(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }

UInt32 Slot(const void* object, UInt32 offset) {
	if (!Readable(object)) {
		return 0;
	}
	const UInt32 vtable = *static_cast<const UInt32*>(object);
	if (!mem::LooksLikeObjectAddress(vtable)) {
		return 0;
	}
	return *reinterpret_cast<const UInt32*>(vtable + offset);
}

void* ActorOf(UInt32 refId) {
	using LookupFn = UInt8*(__cdecl*)(UInt32 id);
	UInt8* const form = reinterpret_cast<LookupFn>(kLookupFormById)(refId);
	if (!Readable(form)) {
		return nullptr;
	}
	const UInt8 type = form[addr::kFormTypeOffset];
	return type == kFormTypeCharacterRef || type == kFormTypeCreatureRef ? form : nullptr;
}

NiAVObject* RootOf(void* actor) {
	const UInt32 getNode = Slot(actor, addr::kActorVtableGetNiNodeOffset);
	if (getNode == 0) {
		return nullptr;
	}
	using NodeFn = NiAVObject*(__thiscall*)(void* actor);
	NiAVObject* const root = reinterpret_cast<NodeFn>(getNode)(actor);
	return Readable(root) ? root : nullptr;
}

NiAVObject* BoneOf(NiAVObject* root, const char* name) {
	const UInt32 getObject = Slot(root, addr::kNiAVObjectGetObjectVtableOffset);
	if (getObject == 0) {
		return nullptr;
	}
	using GetObjectFn = NiAVObject*(__fastcall*)(void* self, void* edx, const char* name);
	NiAVObject* const node = reinterpret_cast<GetObjectFn>(getObject)(root, nullptr, name);
	return Readable(node) && Readable(node->parent) ? node : nullptr;
}

struct WalkerArm {
	NiAVObject* upper = nullptr;
	NiAVObject* fore = nullptr;
	NiAVObject* hand = nullptr;
	bool found() const { return upper != nullptr && fore != nullptr && hand != nullptr && fore->parent == upper; }
};

WalkerArm ArmOf(NiAVObject* root, WalkerHand side) {
	const GripBones names = GripBoneNames(side);
	WalkerArm arm;
	arm.upper = BoneOf(root, names.upper);
	arm.fore = BoneOf(root, names.fore);
	arm.hand = BoneOf(root, names.hand);
	return arm;
}

NiPoint3 NotANumber() {
	const float nan = 0.0f / 0.0f;
	return NiPoint3{nan, nan, nan};
}

bool Refused(GripMemory& m, const char* why) {
	if (m.lastWhy != why && State().linesLeft > 0) {
		--State().linesLeft;
		OBVR_LOG("Walker grip: %08X not laid on - %s (weight %.2f)", m.refId, why, static_cast<double>(m.weight));
	}
	m.lastWhy = why;
	return false;
}

// One of the player's arms: false when nothing was laid on this frame.
bool GripOne(UInt32 arm, GripMemory& m, bool held, float dtSeconds, const HeldHands& hands) {
	m.weight = StepGripWeight(m.weight, held, dtSeconds);
	if (!(m.weight > 0.0f)) {
		if (!held) {
			m = GripMemory{};
		}
		return false;
	}
	void* const actor = ActorOf(m.refId);
	if (actor == nullptr) {
		return Refused(m, "the reference is no actor");
	}
	NiAVObject* const root = RootOf(actor);
	if (root == nullptr) {
		return Refused(m, "the actor has no 3D");
	}
	const UInt32 at = arm == kGripArmRight ? 1 : 0;
	if (!hands.valid[at]) {
		return Refused(m, "the player's controller is not tracked");
	}
	const NiPoint3 handPos = hands.pos[at];
	const NiPoint3 axis = hands.forward[at];
	const NiPoint3 grip = handPos - axis * kGripBackUnits;
	if (m.hand == kWalkerNoHand) {
		const WalkerArm left = ArmOf(root, kWalkerLeftHand);
		const WalkerArm right = ArmOf(root, kWalkerRightHand);
		m.hand = WalkerHandFor(left.found() ? left.hand->worldTransform.pos : NotANumber(),
		                       right.found() ? right.hand->worldTransform.pos : NotANumber(), grip);
		if (m.hand == kWalkerNoHand) {
			if (State().linesLeft > 0) {
				--State().linesLeft;
				OBVR_LOG("Walker grip: %08X has no Bip01 arm to hold with - its animation stays", m.refId);
			}
			m.weight = 0.0f;
			return false;
		}
	}
	const WalkerArm bones = ArmOf(root, m.hand);
	if (!bones.found()) {
		return Refused(m, "the holding arm's bones not found");
	}
	GripInput in;
	in.shoulder = bones.upper->worldTransform.pos;
	in.upperLength = ik::Length(bones.fore->worldTransform.pos - in.shoulder);
	in.foreLength = ik::Length(bones.hand->worldTransform.pos - bones.fore->worldTransform.pos);
	const NiMatrix33& rootRot = root->worldTransform.rot;
	in.walkerRight = NiPoint3{rootRot.data[0][0], rootRot.data[1][0], rootRot.data[2][0]};
	in.side = GripSideSign(m.hand);
	in.playerWrist = handPos;
	in.playerHandAxis = axis;
	in.animElbow = bones.fore->worldTransform.pos;
	in.animWrist = bones.hand->worldTransform.pos;
	in.weight = m.weight;
	const GripPlan plan = PlanGrip(in);
	if (!plan.use) {
		if (State().linesLeft > 0 && m.lastWhy != plan.why) {
			OBVR_LOG("Walker grip: %08X - shoulder to forearm %.1f units, the arm %.1f + %.1f; shoulder %.1f %.1f %.1f, "
			         "the walker's root %.1f %.1f %.1f, the forearm %.1f %.1f %.1f",
			         m.refId, static_cast<double>(ik::Length(grip - in.shoulder)), static_cast<double>(in.upperLength),
			         static_cast<double>(in.foreLength), static_cast<double>(in.shoulder.x),
			         static_cast<double>(in.shoulder.y), static_cast<double>(in.shoulder.z),
			         static_cast<double>(root->worldTransform.pos.x), static_cast<double>(root->worldTransform.pos.y),
			         static_cast<double>(root->worldTransform.pos.z), static_cast<double>(grip.x),
			         static_cast<double>(grip.y), static_cast<double>(grip.z));
		}
		return Refused(m, plan.why);
	}
	const LimbSolve s = SolveLimb(plan.limb);
	if (!s.valid) {
		return Refused(m, "no solve");
	}
	// Written parent first, the hand keeping its own pose under the forearm.
	const NiAVObject* const parent = bones.upper->parent;
	const BonePose upperLocal = LocalUnderParent(parent->worldTransform.rot, parent->worldTransform.pos,
	                                             parent->worldTransform.scale, BonePose{s.upperRot, in.shoulder});
	bones.upper->localTransform.rot = upperLocal.rot;
	bones.upper->localTransform.pos = upperLocal.pos;
	const BonePose foreLocal = LocalUnderParent(s.upperRot, in.shoulder, bones.upper->worldTransform.scale,
	                                            BonePose{s.lowerRot, s.joint});
	bones.fore->localTransform.rot = foreLocal.rot;
	bones.fore->localTransform.pos = foreLocal.pos;
	UpdateNodeTransforms(bones.upper);
	if (!m.reported && m.weight >= 1.0f && State().linesLeft > 0) {
		--State().linesLeft;
		m.reported = true;
		OBVR_LOG("Walker grip: %08X holds the player's %s arm with its %s - arm %.1f + %.1f units, its wrist at "
		         "%.1f %.1f %.1f for the forearm at %.1f %.1f %.1f (%s)",
		         m.refId, arm == kGripArmRight ? "right" : "left", m.hand == kWalkerLeftHand ? "left" : "right",
		         static_cast<double>(in.upperLength), static_cast<double>(in.foreLength),
		         static_cast<double>(bones.hand->worldTransform.pos.x),
		         static_cast<double>(bones.hand->worldTransform.pos.y),
		         static_cast<double>(bones.hand->worldTransform.pos.z), static_cast<double>(grip.x),
		         static_cast<double>(grip.y), static_cast<double>(grip.z), s.reached ? "reached" : "short of it");
	}
	return true;
}

// Both forearms to Walkers, while it listens, on the send clock.
void SendForearms(bool linked, float dtSeconds, const HeldHands& hands) {
	if (!linked || !ForearmSendDue(State().sendSince, dtSeconds)) {
		return;
	}
	for (UInt32 arm = kGripArmLeft; arm <= kGripArmRight; ++arm) {
		const UInt32 at = arm == kGripArmRight ? 1 : 0;
		if (!hands.valid[at]) {
			continue;
		}
		const NiPoint3 grip = hands.pos[at] - hands.forward[at] * kGripBackUnits;
		const GripForearmMessage msg = ForearmMessageFor(arm, grip);
		const bool taken = SendWalkersForearm(msg.arm, msg.x, msg.y, msg.z);
		++(taken ? State().sendsTaken : State().sendsRefused);
	}
	// The stream's evidence: the first sends, then a count every 100 (10 s).
	const UInt32 total = State().sendsTaken + State().sendsRefused;
	if ((total <= 2 || total % 200 == 0) && State().linesLeft > 0) {
		--State().linesLeft;
		OBVR_LOG("Walker grip: 'WGHP' to Walkers - %u taken, %u refused so far", State().sendsTaken,
		         State().sendsRefused);
	}
}

}  // namespace

void StepWalkerGrips(const WalkersNow& walkers, float dtSeconds, bool enabled, const HeldHands& hands) {
	const vr::WalkersHolds& holds = walkers.holds;
	if (enabled) {
		SendForearms(walkers.linked, dtSeconds, hands);
	}
	for (UInt32 arm = kGripArmLeft; arm <= kGripArmRight; ++arm) {
		GripMemory& m = State().arm[arm - 1];
		const UInt32 holder = vr::HolderOf(holds, arm);
		const bool held = enabled && holder != 0 && holder != kTestHolder;
		if (held && holder != m.refId) {
			m = GripMemory{};
			m.refId = holder;
		}
		if (m.refId == 0) {
			continue;
		}
		GripOne(arm, m, held, dtSeconds, hands);
	}
}

}  // namespace obvr::game
