#include "game/ActorRoster.h"

#include <cmath>
#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/Shove.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }
float FloatAt(UInt32 a) { return *reinterpret_cast<const float*>(a); }
bool Looks(UInt32 a) { return mem::LooksLikeObjectAddress(a); }

struct ListNode {
	void* data;
	ListNode* next;
};

// Dead as a body: the life state 1 or 2 (ActorRosterLogic.h); an actor
// whose vtable's IsDead is unreadable counts as dead, so a scenario never
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
	// A dead body only (ActorRosterLogic.h LifeStateIsDeadBody): not one
	// knocked out, whom IsDead(0) counts too.
	return LifeStateIsDeadBody(Read(actor + kActorLifeStateOffset));
}

}  // namespace

bool ActorInWorld(UInt32 actor, bool& dead) {
	if (!Looks(actor)) {
		return false;
	}
	const UInt32 vtable = Read(actor);
	if ((vtable != addr::kVtblCharacter && vtable != addr::kVtblCreature) || Read(actor + addr::kRefNiNodeOffset) == 0) {
		return false;
	}
	dead = IsDead(actor);
	return true;
}

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

// What the engine says about each actor near the player, for a scenario's
// marks: the life state (+0xB0), IsDead with its argument 0 and 1, and the
// knocked state (Shove.h) - to see which values a dying, dead, knocked-down
// or knocked-out one carries (the review of 2026-11-11 on LifeStateIsDeadBody).
void LogLifeStates() {
	RosterActor actors[64];
	const UInt32 n = CollectLoadedActors(actors, 64);
	char line[640];
	UInt32 at = 0;
	line[0] = '\0';
	for (UInt32 i = 0; i < n && at + 64 < sizeof(line); ++i) {
		const UInt32 actor = actors[i].ref;
		if (actors[i].distance > 600.0f || !Looks(actor)) {
			continue;
		}
		const UInt32 fn = Read(Read(actor) + addr::kActorVtableIsDeadOffset);
		using DeadFn = UInt8(__fastcall*)(void* self, void* edx, UInt32 arg);
		const bool callable = fn >= addr::kTextStart && fn < addr::kTextEnd;
		const int dead0 = callable ? (reinterpret_cast<DeadFn>(fn)(reinterpret_cast<void*>(actor), nullptr, 0) & 1) : -1;
		const int dead1 = callable ? (reinterpret_cast<DeadFn>(fn)(reinterpret_cast<void*>(actor), nullptr, 1) & 1) : -1;
		const int written = std::snprintf(line + at, sizeof(line) - at, "%s%08X state %u IsDead(0) %d IsDead(1) %d knocked %d",
		                                  at == 0 ? "" : "; ", actors[i].formId, Read(actor + kActorLifeStateOffset),
		                                  dead0, dead1, static_cast<int>(ActorKnockState(reinterpret_cast<void*>(actor))));
		if (written < 0) {
			break;
		}
		at += static_cast<UInt32>(written);
	}
	OBVR_LOG("HandScript: life states - %s", at == 0 ? "nobody within 600 units" : line);
}

}  // namespace obvr::game

