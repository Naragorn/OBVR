#include "game/ActorRoster.h"

#include <cmath>

#include "core/AddressSpace.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }
float FloatAt(UInt32 a) { return *reinterpret_cast<const float*>(a); }
bool Looks(UInt32 a) { return mem::LooksLikeObjectAddress(a); }

struct ListNode {
	void* data;
	ListNode* next;
};

// Actor::IsDead through the vtable (kActorVtableIsDeadOffset), as
// MeleeHits.cpp reads it; unreadable counts as dead, so a scenario never
// takes an odd object for a living one.
bool IsDead(UInt32 actor) {
	const UInt32 vtable = Read(actor);
	if (!Looks(vtable)) {
		return true;
	}
	const UInt32 fn = Read(vtable + addr::kActorVtableIsDeadOffset);
	if (fn < addr::kTextStart || fn >= addr::kTextEnd) {
		return true;
	}
	using DeadFn = UInt8(__fastcall*)(void* self, void* edx, UInt32 arg);
	return (reinterpret_cast<DeadFn>(fn)(reinterpret_cast<void*>(actor), nullptr, 0) & 1) != 0;
}

}  // namespace

UInt32 CollectLoadedActors(RosterActor* out, UInt32 most) {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!Looks(player) || most == 0) {
		return 0;
	}
	const float px = FloatAt(player + addr::kRefPositionOffset);
	const float py = FloatAt(player + addr::kRefPositionOffset + 4);
	const float pz = FloatAt(player + addr::kRefPositionOffset + 8);
	auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
	using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
	UInt32 count = 0;
	for (UInt32 level = 0; level < 4 && count < most; ++level) {
		auto* node = static_cast<ListNode*>(reinterpret_cast<ListFn>(addr::kActorListByLevel)(manager, nullptr, level));
		for (UInt32 visited = 0; node != nullptr && visited < 1024 && count < most; ++visited) {
			if (!Looks(reinterpret_cast<UInt32>(node))) {
				break;
			}
			const UInt32 actor = reinterpret_cast<UInt32>(node->data);
			node = node->next;
			if (actor == 0 || actor == player || !Looks(actor)) {
				continue;
			}
			const UInt32 vtable = Read(actor);
			if (vtable != addr::kVtblCharacter && vtable != addr::kVtblCreature) {
				continue;
			}
			// Only those with a model in the world: the lower lists hold
			// every NPC of Tamriel on his schedule (96 in the testing hall,
			// 2026-10-23), and a scenario counts what stands in the loaded
			// cells, a corpse included.
			if (Read(actor + addr::kRefNiNodeOffset) == 0) {
				continue;
			}
			bool seen = false;
			for (UInt32 i = 0; i < count; ++i) {
				if (out[i].formId == Read(actor + addr::kFormIdOffset)) {
					seen = true;
					break;
				}
			}
			if (seen) {
				continue;
			}
			RosterActor& entry = out[count++];
			entry.formId = Read(actor + addr::kFormIdOffset);
			entry.ref = actor;
			const UInt32 base = Read(actor + addr::kRefBaseFormOffset);
			entry.baseFormId = Looks(base) ? Read(base + addr::kFormIdOffset) : 0;
			entry.dead = IsDead(actor);
			const float dx = FloatAt(actor + addr::kRefPositionOffset) - px;
			const float dy = FloatAt(actor + addr::kRefPositionOffset + 4) - py;
			const float dz = FloatAt(actor + addr::kRefPositionOffset + 8) - pz;
			entry.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		}
	}
	return count;
}

}  // namespace obvr::game
