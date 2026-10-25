#include "game/EnemyHealth.h"

#include "core/AddressSpace.h"
#include "core/MathFns.h"
#include "game/ActorLink.h"
#include "game/BladeBodies.h"
#include "game/BladeContactLogic.h"
#include "game/CombatRing.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/Shove.h"

namespace obvr::game {
namespace {

using obvr::NiAVObject;

constexpr UInt32 kGetActorValueFSlot = 0x288;  // float, current (PlayerTeleport.cpp reads fatigue through it)
constexpr UInt32 kGetBaseActorValue = 0x005F1910;
constexpr UInt32 kActorValueHealth = 8;

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

UInt32 Slot(UInt32 object, UInt32 offset) {
	const UInt32 vtable = LooksLikeObject(object) ? Read(object) : 0;
	if (!LooksLikeObject(vtable)) {
		return 0;
	}
	const UInt32 fn = Read(vtable + offset);
	return fn >= addr::kTextStart && fn < addr::kTextEnd ? fn : 0;
}

struct ListNode {
	void* data;
	ListNode* next;
};

bool IsActor(UInt32 actor) {
	if (!LooksLikeObject(actor)) {
		return false;
	}
	const UInt32 vtable = Read(actor);
	return vtable == addr::kVtblCharacter || vtable == addr::kVtblCreature;
}

bool IsDead(UInt32 actor) {
	using DeadFn = UInt8(__fastcall*)(void* self, void* edx, UInt32 arg);
	const UInt32 dead = Slot(actor, addr::kActorVtableIsDeadOffset);
	return dead == 0 || (reinterpret_cast<DeadFn>(dead)(reinterpret_cast<void*>(actor), nullptr, 0) & 1) != 0;
}

// Health now and base; false when the value functions do not read.
bool ReadHealth(UInt32 actor, float& now, float& base) {
	const bool actorLooks = LooksLikeObject(actor);
	const bool baseLooks = actorLooks && LooksLikeObject(Read(actor + addr::kRefBaseFormOffset));
	const UInt32 slot = ActorValuesReadable(actorLooks, baseLooks) ? Slot(actor, kGetActorValueFSlot) : 0;
	if (slot == 0) {
		return false;
	}
	using GetFloatFn = float(__thiscall*)(void* actor, UInt32 av);
	using GetBaseFn = SInt32(__thiscall*)(void* actor, UInt32 av);
	now = reinterpret_cast<GetFloatFn>(slot)(reinterpret_cast<void*>(actor), kActorValueHealth);
	base = static_cast<float>(
		reinterpret_cast<GetBaseFn>(kGetBaseActorValue)(reinterpret_cast<void*>(actor), kActorValueHealth));
	return now == now;
}

EnemyHealthBoneTier BoneTierOf(UInt32 bone) {
	switch (bone) {
		case kBoneNeck:
		case kBoneSpine2:
		case kBoneSpine1:
		case kBoneSpine:
		case kBonePelvis:
			return EnemyHealthBoneTier::Trunk;
		case kBoneLUpperArm:
		case kBoneRUpperArm:
		case kBoneLThigh:
		case kBoneRThigh:
			return EnemyHealthBoneTier::LimbRoot;
		default:
			return EnemyHealthBoneTier::Limb;
	}
}

// The bar's place over an actor's own top (EnemyHealthAnchorOf): the head
// bone when the skeleton has one; else the other bones the blade keeps
// (neck, spine, pelvis, the limbs - a rat's spine, a mudcrab's claws), read
// now; else the model's bound; else the feet.
EnemyHealthAnchor AnchorOf(UInt32 actor, float aboveUnits) {
	EnemyHealthBody body;
	const auto* const feet = reinterpret_cast<const float*>(actor + addr::kRefPositionOffset);
	body.feet = NiPoint3{feet[0], feet[1], feet[2]};
	using NodeFn = void*(__fastcall*)(void* self, void* edx);
	const UInt32 getNode = Slot(actor, addr::kActorVtableGetNiNodeOffset);
	if (getNode != 0) {
		const UInt32 root =
			reinterpret_cast<UInt32>(reinterpret_cast<NodeFn>(getNode)(reinterpret_cast<void*>(actor), nullptr));
		if (LooksLikeObject(root)) {
			const auto* const rootNode = reinterpret_cast<const NiAVObject*>(root);
			body.scale = rootNode->worldTransform.scale;
			body.haveBound = true;
			body.boundCentre = rootNode->worldBound.center;
			body.boundRadius = rootNode->worldBound.radius;
			const NiAVObject* const head = ActorBoneNode(actor, root, kBoneHead);
			if (head != nullptr) {
				body.haveHead = true;
				body.head = head->worldTransform.pos;
			} else {
				for (UInt32 b = kBoneHead + 1; b < kBladeBoneCount && body.boneCount < kEnemyHealthBodyBonesMax; ++b) {
					const NiAVObject* const bone = ActorBoneNode(actor, root, b);
					if (bone != nullptr) {
						body.tier[body.boneCount] = BoneTierOf(b);
						body.bone[body.boneCount++] = bone->worldTransform.pos;
					}
				}
			}
		}
	}
	return EnemyHealthAnchorOf(body, aboveUnits);
}

bool InKeep(UInt32 actor, const UInt32* keep, UInt32 keepCount) {
	for (UInt32 i = 0; i < keepCount; ++i) {
		if (keep[i] == actor) {
			return true;
		}
	}
	return false;
}

}  // namespace

UInt32 ReadFightersHealth(const NiPoint3& eyes, float unitsPerMetre, float rangeMetres, float aboveUnits,
                          const UInt32* keep, UInt32 keepCount, EnemyHealthSeen* out, UInt32 capacity) {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player) || out == nullptr || capacity == 0) {
		return 0;
	}
	const float perMetre = unitsPerMetre > 0.0f ? 1.0f / unitsPerMetre : 0.0f;
	auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
	using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
	auto* node = static_cast<ListNode*>(reinterpret_cast<ListFn>(addr::kActorListByLevel)(manager, nullptr, 0));
	UInt32 count = 0;
	for (UInt32 visited = 0; node != nullptr && visited < 512 && count < capacity; ++visited) {
		if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
			break;
		}
		const UInt32 actor = reinterpret_cast<UInt32>(node->data);
		node = node->next;
		if (actor == 0 || actor == player || !IsActor(actor)) {
			continue;
		}
		const bool kept = InKeep(actor, keep, keepCount);
		const bool dead = IsDead(actor);
		const bool fighting = !dead && ActorAliveInHighProcess(reinterpret_cast<void*>(actor)) &&
		                      ActorFightsPlayer(actor, player);
		if (!fighting && !kept) {
			continue;
		}
		const auto* const feet = reinterpret_cast<const float*>(actor + addr::kRefPositionOffset);
		const NiPoint3 d{feet[0] - eyes.x, feet[1] - eyes.y, feet[2] - eyes.z};
		const float metres = math::Sqrt(d.LengthSquared()) * perMetre;
		if (!kept && metres > rangeMetres) {
			continue;
		}
		float now = 0.0f;
		float base = 0.0f;
		const bool haveHealth = !dead && ReadHealth(actor, now, base);
		EnemyHealthSeen& s = out[count++];
		s = EnemyHealthSeen{};
		s.actor = actor;
		s.fighting = fighting;
		s.dead = dead;
		s.fraction = dead ? 0.0f : haveHealth ? EnemyHealthFraction(now, base) : 1.0f;
		s.metres = metres;
		const EnemyHealthAnchor anchor = AnchorOf(actor, aboveUnits);
		s.at = anchor.bar;
		s.look = anchor.look;
		s.from = anchor.from;
		s.overFeetUnits = anchor.bar.z - feet[2];
		const UInt32 baseForm = Read(actor + addr::kRefBaseFormOffset);
		s.baseForm = LooksLikeObject(baseForm) ? Read(baseForm + addr::kFormIdOffset) : 0;
	}
	return count;
}

}  // namespace obvr::game
