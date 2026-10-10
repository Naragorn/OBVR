#include "game/CombatRing.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/NiMath.h"
#include "game/ParryLogic.h"
#include "game/SlowApproach.h"

namespace obvr::game {
namespace {

constexpr UInt32 kProcessLevelSlot = 0x08;
constexpr UInt32 kIsInCombatSlot = 0x334;      // Actor, as the slow approach calls it
constexpr UInt32 kCombatTargetSlot = 0x338;    // Actor::GetCombatTarget, 0x005E0AF0

RingState g_ring;
UInt32 g_lines = 40;
UInt32 g_wasTurns = 0;
UInt32 g_wasWaiting = 0;

UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }
bool LooksLikeObject(UInt32 a) { return mem::LooksLikeObjectAddress(a); }

UInt32 Slot(UInt32 object, UInt32 offset) {
	const UInt32 vtable = LooksLikeObject(object) ? Read(object) : 0;
	if (!LooksLikeObject(vtable)) {
		return 0;
	}
	const UInt32 fn = Read(vtable + offset);
	return fn >= addr::kTextStart && fn < addr::kTextEnd ? fn : 0;
}

UInt32 HighProcessOf(UInt32 actor) {
	const UInt32 process = LooksLikeObject(actor) ? Read(actor + addr::kMobileProcessOffset) : 0;
	const UInt32 levelFn = Slot(process, kProcessLevelSlot);
	using LevelFn = UInt32(__fastcall*)(void* process, void* edx);
	if (levelFn == 0 || reinterpret_cast<LevelFn>(levelFn)(reinterpret_cast<void*>(process), nullptr) != 0) {
		return 0;
	}
	return process;
}

struct ListNode {
	void* data;
	ListNode* next;
};

struct Seen {
	UInt32 actor = 0;
	UInt32 process = 0;
};

}  // namespace

void StepCombatRing(const CombatRingSettings& settings, float dtSeconds, bool logState) {
	if (!settings.enabled) {
		if (g_ring.count != 0) {
			g_ring = RingState{};
			SetRingLegs(nullptr, 0);
		}
		return;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		SetRingLegs(nullptr, 0);
		return;
	}
	const auto* const pp = reinterpret_cast<const float*>(player + addr::kRefPositionOffset);
	const NiPoint3 playerAt{pp[0], pp[1], pp[2]};

	RingFighter fighters[kRingMembersMax];
	Seen seen[kRingMembersMax];
	UInt32 count = 0;
	auto* const manager = reinterpret_cast<void*>(addr::kActorProcessManager);
	using ListFn = void*(__fastcall*)(void* self, void* edx, UInt32 level);
	auto* node = static_cast<ListNode*>(reinterpret_cast<ListFn>(addr::kActorListByLevel)(manager, nullptr, 0));
	for (UInt32 visited = 0; node != nullptr && visited < 512 && count < kRingMembersMax; ++visited) {
		if (!LooksLikeObject(reinterpret_cast<UInt32>(node))) {
			break;
		}
		const UInt32 actor = reinterpret_cast<UInt32>(node->data);
		node = node->next;
		if (actor == 0 || actor == player || !LooksLikeObject(actor)) {
			continue;
		}
		const UInt32 vtable = Read(actor);
		if (vtable != addr::kVtblCharacter && vtable != addr::kVtblCreature) {
			continue;
		}
		const UInt32 process = HighProcessOf(actor);
		const UInt32 combatFn = Slot(actor, kIsInCombatSlot);
		const UInt32 targetFn = Slot(actor, kCombatTargetSlot);
		if (process == 0 || combatFn == 0 || targetFn == 0) {
			continue;
		}
		using CombatFn = UInt8(__fastcall*)(void* actor, void* edx, UInt32 unk);
		using TargetFn = UInt32(__fastcall*)(void* actor, void* edx);
		if ((reinterpret_cast<CombatFn>(combatFn)(reinterpret_cast<void*>(actor), nullptr, 1) & 1) == 0 ||
		    reinterpret_cast<TargetFn>(targetFn)(reinterpret_cast<void*>(actor), nullptr) != player) {
			continue;
		}
		const auto* const ap = reinterpret_cast<const float*>(actor + addr::kRefPositionOffset);
		const float dx = ap[0] - playerAt.x;
		const float dy = ap[1] - playerAt.y;
		const SInt32 action = *reinterpret_cast<const SInt16*>(process + addr::kProcessCurrentActionOffset);
		fighters[count].actor = actor;
		fighters[count].distanceUnits = math::Sqrt(dx * dx + dy * dy);
		fighters[count].attacking = IsAttackAction(action);
		seen[count].actor = actor;
		seen[count].process = process;
		++count;
	}

	const RingVerdict v = StepCombatRing(g_ring, settings, fighters, count, dtSeconds);

	RingLegs legs[kRingMembersMax];
	UInt32 legCount = 0;
	for (UInt32 i = 0; i < count; ++i) {
		const RingOrder order = RingOrderOf(g_ring, seen[i].actor);
		if (order == RingOrder::Free) {
			continue;
		}
		legs[legCount].process = seen[i].process;
		legs[legCount].order = static_cast<UInt8>(order);
		// Half go round one way, half the other: the circle spreads.
		legs[legCount].left = ((seen[i].actor >> 4) & 1) != 0;
		++legCount;
	}
	SetRingLegs(legs, legCount);

	if ((v.turnsEnded != 0 || v.turnsGiven != 0 || v.turns != g_wasTurns || v.waiting != g_wasWaiting) &&
	    g_lines > 0) {
		--g_lines;
		OBVR_LOG("Combat ring: %u fighting the player - %u with a turn, %u waiting at %.1f m (%u turn(s) ended, %u "
		         "given this frame; at once %u)",
		         count, v.turns, v.waiting, static_cast<double>(settings.ringUnits / 70.0f), v.turnsEnded,
		         v.turnsGiven, settings.attackersAtOnce);
	}
	g_wasTurns = v.turns;
	g_wasWaiting = v.waiting;
	if (logState) {
		for (UInt32 i = 0; i < g_ring.count; ++i) {
			const RingMember& m = g_ring.member[i];
			OBVR_LOG("Combat ring: state - %08X %.1f m off, %s, order %u, attacking %d, %.1f s in turn, %.1f s waiting",
			         m.actor, static_cast<double>(m.distanceUnits / 70.0f), m.turn ? "its turn" : "waits",
			         static_cast<unsigned>(m.order), m.wasAttacking ? 1 : 0, static_cast<double>(m.turnSeconds),
			         static_cast<double>(m.waitingSeconds));
		}
	}
}

}  // namespace obvr::game
