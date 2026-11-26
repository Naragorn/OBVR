#include "game/Dismember.h"

// The necks, wrists and limb joints a blade is tested against, a part's bone
// by its names, and three reads of an actor's state (moved out of game/Dismember.cpp, 2026-11-26, the size
// ratchet). Declared in game/Dismember.h.

#include "core/AddressSpace.h"
#include "core/Memory.h"
#include "game/ActorRoster.h"
#include "game/CutSkeletonLogic.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/NeckReach.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(UInt32 p) { return mem::LooksLikeObjectAddress(p); }
UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }

using GetObjectFn = void*(__fastcall*)(void* self, void* edx, const char* name);

// The node of that name under `root` (the engine's GetObjectByName), or null.
NiAVObject* Named(UInt32 root, const char* name) {
	const UInt32 vtable = LooksLikeObject(root) ? Read(root) : 0;
	const UInt32 fn = LooksLikeObject(vtable) ? Read(vtable + addr::kNiAVObjectGetObjectVtableOffset) : 0;
	if (fn < addr::kTextStart || fn >= addr::kTextEnd) {
		return nullptr;
	}
	auto* const node =
		static_cast<NiAVObject*>(reinterpret_cast<GetObjectFn>(fn)(reinterpret_cast<void*>(root), nullptr, name));
	return LooksLikeObject(reinterpret_cast<UInt32>(node)) ? node : nullptr;
}

// A bone's parent when it reads, else null.
NiAVObject* ParentOf(const NiAVObject* node) {
	return node != nullptr && LooksLikeObject(reinterpret_cast<UInt32>(node->parent)) ? node->parent : nullptr;
}

}  // namespace

NiAVObject* PartBoneOf(UInt32 root, CutPart part) {
	if (part != CutPart::Head) {
		return Named(root, BonesOfCut(part).part);
	}
	NiAVObject* head = nullptr;
	for (UInt32 i = 0; head == nullptr && HeadBoneName(i) != nullptr; ++i) {
		head = Named(root, HeadBoneName(i));
	}
	return head;
}

bool ActorIsCreature(UInt32 actor) {
	const UInt32 base = LooksLikeObject(actor) ? Read(actor + addr::kRefBaseFormOffset) : 0;
	return LooksLikeObject(base) && IsCreatureFormType(*reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset));
}

bool LoadedActorIsDead(UInt32 actor) {
	RosterActor actors[64];
	const UInt32 n = CollectLoadedActors(actors, 64);
	for (UInt32 i = 0; i < n; ++i) {
		if (actors[i].ref == actor) {
			return actors[i].dead;
		}
	}
	return false;
}

bool ActorIsEssential(UInt32 actor) {
	const UInt32 base = LooksLikeObject(actor) ? Read(actor + addr::kRefBaseFormOffset) : 0;
	return LooksLikeObject(base) && ActorBaseFlagsEssential(Read(base + kEssentialFlagsOffset));
}

UInt32 CollectNecks(const NiPoint3& around, float within, NeckOf* out, UInt32 most, NeckParts parts) {
	RosterActor actors[64];
	const UInt32 n = CollectLoadedActors(actors, 64);
	NeckRule& rule = TheNeckRule();
	const bool check = BeginNeckCall(rule, n);
	UInt32 found = 0;
	for (UInt32 i = 0; i < n && found < most; ++i) {
		const UInt32 actor = actors[i].ref;
		const UInt32 root = LooksLikeObject(actor) ? Read(actor + addr::kRefNiNodeOffset) : 0;
		if (!LooksLikeObject(root)) {
			continue;
		}
		// By its 3D's bound first (NeckReachLogic.h): the names are a walk of
		// the whole 3D each.
		const NiBound bound = reinterpret_cast<const NiAVObject*>(root)->worldBound;
		const NeckAsk ask = AsksBones(bound.center, bound.radius, around, within, check);
		if (ask == NeckAsk::No) {
			continue;
		}
		++rule.asked;
		// The head first: most of those asked are out of reach by it, and
		// every name is a walk.
		NiAVObject* const head = PartBoneOf(root, CutPart::Head);  // a rat's Skull too
		if (head == nullptr || (head->worldTransform.pos - around).LengthSquared() > within * within) {
			continue;
		}
		// The neck's low end: Spine2, else two bones up from the head (an
		// ogre's Spine1; CutSkeletonLogic.h NeckLowFrom).
		NiAVObject* const spine2 = Named(root, "Bip01 Spine2");
		NiAVObject* const grandparent = spine2 == nullptr ? ParentOf(ParentOf(head)) : nullptr;
		const NeckLow low = NeckLowFrom(spine2 != nullptr, grandparent != nullptr);
		NiAVObject* const spine = low == NeckLow::Spine2 ? spine2 : (low == NeckLow::HeadGrandparent ? grandparent : nullptr);
		if (spine == nullptr) {
			continue;
		}
		NeckTaken(rule, actor, bound, head->worldTransform.pos, ask == NeckAsk::YesToCheck);
		NeckOf& k = out[found++];
		k = NeckOf{};
		k.actor = actor;
		k.dead = actors[i].dead;
		k.low = spine->worldTransform.pos;
		k.high = head->worldTransform.pos;
		if (parts == NeckParts::NeckOnly) {
			continue;
		}
		for (UInt32 p = 0; p < kCutPartCount; ++p) {
			const CutPartBones bones = BonesOfCut(static_cast<CutPart>(p));
			NiAVObject* const joint = IsLimbCut(static_cast<CutPart>(p)) ? Named(root, bones.part) : nullptr;
			NiAVObject* const next = joint != nullptr ? Named(root, bones.next) : nullptr;
			k.hasLimb[p] = next != nullptr;
			if (k.hasLimb[p]) {
				k.limbJoint[p] = joint->worldTransform.pos;
				k.limbNext[p] = next->worldTransform.pos;
			}
		}
		for (UInt32 side = 0; side < 2; ++side) {
			NiAVObject* const forearm = Named(root, side == 0 ? "Bip01 L Forearm" : "Bip01 R Forearm");
			NiAVObject* const hand = Named(root, side == 0 ? "Bip01 L Hand" : "Bip01 R Hand");
			k.hasWrist[side] = forearm != nullptr && hand != nullptr;
			if (k.hasWrist[side]) {
				k.wristLow[side] = forearm->worldTransform.pos;
				k.wristHigh[side] = hand->worldTransform.pos;
			}
		}
	}
	SayNeckRule(rule);
	return found;
}

}  // namespace obvr::game
