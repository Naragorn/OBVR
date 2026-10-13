#include "game/PlayerStagger.h"

#include <intrin.h>

#include "core/AddressSpace.h"
#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/ExhaustionLogic.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

bool g_enabled = true;
bool g_knockdownEnabled = true;
bool g_staggerReported = false;
bool g_knockbackReported = false;
UInt32 g_knockdownLines = 20;

UInt32 Player() { return *reinterpret_cast<const UInt32*>(addr::kPlayerPointer); }

using ActorVoidFn = void(__thiscall*)(void* actor);
using ActorPointerFn = void*(__thiscall*)(void* actor);

// Reached by `call` with the actor in ecx and nothing on the stack: a
// fastcall with no stack arguments takes it the same way and returns the
// same way.
void __fastcall OnStagger(void* actor, void* /*edx*/) {
	if (SkipForPlayer(g_enabled, reinterpret_cast<UInt32>(actor), Player())) {
		if (!g_staggerReported) {
			g_staggerReported = true;
			OBVR_LOG("Comfort: a stagger of the player was skipped (Look.NoPlayerStagger)");
		}
		return;
	}
	reinterpret_cast<ActorVoidFn>(kStaggerStart)(actor);
}

void* __fastcall OnKnockbackProxy(void* actor, void* /*edx*/) {
	if (SkipForPlayer(g_enabled, reinterpret_cast<UInt32>(actor), Player())) {
		if (!g_knockbackReported) {
			g_knockbackReported = true;
			OBVR_LOG("Comfort: a hit's knockback of the player was skipped (Look.NoPlayerStagger)");
		}
		return nullptr;
	}
	return reinterpret_cast<ActorPointerFn>(kCharacterProxyOf)(actor);
}

using KnockActorAwayFn = void(__thiscall*)(void* process, void* actor, float x, float y, float z, float force);

// Reached through the process's vtable: the process in ecx, five stack
// arguments the callee clears - a fastcall with them after edx does the same.
void __fastcall OnKnockActorAway(void* process, void* /*edx*/, void* actor, float x, float y, float z,
                                 float force) {
	if (SkipForPlayer(g_knockdownEnabled, reinterpret_cast<UInt32>(actor), Player())) {
		if (g_knockdownLines > 0) {
			--g_knockdownLines;
			const UInt32 from = reinterpret_cast<UInt32>(_ReturnAddress());
			OBVR_LOG("Comfort: a knockdown of the player was skipped (Look.NoPlayerKnockdown) - from %08X (%s), "
			         "force %.1f",
			         from,
			         from == 0x0060040Bu   ? "a hit"
			         : from == 0x00699AABu ? "a magic explosion"
			         : (from > 0x0050EAB0u && from < 0x0050EC00u) ? "the script PushActorAway"
			                                                       : "elsewhere",
			         static_cast<double>(force));
		}
		return;
	}
	reinterpret_cast<KnockActorAwayFn>(kKnockActorAway)(process, actor, x, y, z, force);
}

void RerouteSlot(UInt32 slot, const char* what) {
	static const UInt8 kStart[9] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x48};
	const UInt32 held = *reinterpret_cast<const UInt32*>(slot);
	if (held != kKnockActorAway || !mem::Verify(kKnockActorAway, kStart, sizeof(kStart))) {
		OBVR_LOG("Comfort: the %s's knockdown slot at %08X holds %08X, not %08X - knockdowns are left to the game",
		         what, slot, held, kKnockActorAway);
		mem::ReportForeignCode("Comfort", slot);
		return;
	}
	const UInt32 replacement = reinterpret_cast<UInt32>(&OnKnockActorAway);
	if (!mem::SafeWrite(slot, &replacement, sizeof(replacement))) {
		OBVR_LOG("Comfort: could not reroute the %s's knockdown slot at %08X", what, slot);
		return;
	}
	OBVR_LOG("Comfort: the %s's knockdown rerouted at %08X", what, slot);
}

// The knock state's step (PlayerStagger.h, the exhaustion): the player's
// skipped on the frames it would begin the collapse or the paralysis fall.
bool g_exhaustionEnabled = true;
UInt32 g_collapseSkips = 0;
bool g_collapseSkipping = false;
UInt32 g_collapseLines = 20;
constexpr UInt32 kProcessKnockedState = 0x11C;  // HighProcess, one byte
constexpr UInt32 kGetActorValueSlot = 0x284;     // int, current
constexpr UInt32 kGetActorValueFSlot = 0x288;    // float, current
constexpr UInt32 kAvFatigue = 0x0A;
constexpr UInt32 kAvParalysis = 0x30;

UInt32 ActorSlot(void* actor, UInt32 offset) {
	const UInt32 a = reinterpret_cast<UInt32>(actor);
	const UInt32 vtable = mem::LooksLikeObjectAddress(a) ? *reinterpret_cast<const UInt32*>(a) : 0;
	return mem::LooksLikeObjectAddress(vtable) ? *reinterpret_cast<const UInt32*>(vtable + offset) : 0;
}

float ActorFatigue(void* actor) {
	const UInt32 slot = ActorSlot(actor, kGetActorValueFSlot);
	if (slot == 0) {
		return 0.0f;
	}
	using GetFloatFn = float(__thiscall*)(void* actor, UInt32 av);
	return reinterpret_cast<GetFloatFn>(slot)(actor, kAvFatigue);
}

bool ActorParalyzed(void* actor) {
	const UInt32 slot = ActorSlot(actor, kGetActorValueSlot);
	if (slot == 0) {
		return false;
	}
	using GetIntFn = SInt32(__thiscall*)(void* actor, UInt32 av);
	return reinterpret_cast<GetIntFn>(slot)(actor, kAvParalysis) != 0;
}

using KnockStateStepFn = void(__thiscall*)(void* process, void* actor);

// Reached through the process's vtable: the process in ecx, one stack
// argument the callee clears - a fastcall with it after edx does the same.
void __fastcall OnKnockStateStep(void* process, void* /*edx*/, void* actor) {
	const UInt32 player = Player();
	if (player != 0 && reinterpret_cast<UInt32>(actor) == player && mem::LooksLikeObjectAddress(player)) {
		const SInt32 knock = *reinterpret_cast<const UInt8*>(reinterpret_cast<UInt32>(process) + kProcessKnockedState);
		const float fatigue = ActorFatigue(actor);
		const bool paralyzed = ActorParalyzed(actor);
		if (CollapseSkipped(g_exhaustionEnabled, true, knock, fatigue, paralyzed)) {
			++g_collapseSkips;
			if (!g_collapseSkipping && g_collapseLines > 0) {
				--g_collapseLines;
				OBVR_LOG("Comfort: the player's %s was skipped (Look.Exhaustion) - fatigue %.1f, paralysed %d, knock "
				         "state %d; they stay on their feet, exhausted",
				         fatigue < 0.0f ? "fatigue collapse" : "paralysis fall", static_cast<double>(fatigue),
				         paralyzed ? 1 : 0, knock);
			}
			g_collapseSkipping = true;
			return;
		}
		g_collapseSkipping = false;
	}
	reinterpret_cast<KnockStateStepFn>(kKnockStateStep)(process, actor);
}

void RerouteStepSlot(UInt32 slot, const char* what) {
	static const UInt8 kStart[9] = {0x83, 0xEC, 0x2C, 0x55, 0x56, 0x8B, 0x74, 0x24, 0x38};
	const UInt32 held = *reinterpret_cast<const UInt32*>(slot);
	if (held != kKnockStateStep || !mem::Verify(kKnockStateStep, kStart, sizeof(kStart))) {
		OBVR_LOG("Comfort: the %s's knock state step slot at %08X holds %08X, not %08X - the collapse and the "
		         "paralysis fall are left to the game",
		         what, slot, held, kKnockStateStep);
		mem::ReportForeignCode("Comfort", slot);
		return;
	}
	const UInt32 replacement = reinterpret_cast<UInt32>(&OnKnockStateStep);
	if (!mem::SafeWrite(slot, &replacement, sizeof(replacement))) {
		OBVR_LOG("Comfort: could not reroute the %s's knock state step slot at %08X", what, slot);
		return;
	}
	OBVR_LOG("Comfort: the %s's knock state step rerouted at %08X (the exhaustion instead of the collapse)", what,
	         slot);
}

void Reroute(UInt32 callSite, UInt32 original, UInt32 replacement, const char* what) {
	const UInt32 displacement = mem::CallRelativeDisplacement(callSite, original);
	const UInt8 expected[5] = {0xE8, static_cast<UInt8>(displacement & 0xFF),
	                           static_cast<UInt8>((displacement >> 8) & 0xFF),
	                           static_cast<UInt8>((displacement >> 16) & 0xFF),
	                           static_cast<UInt8>((displacement >> 24) & 0xFF)};
	if (!mem::Verify(callSite, expected, sizeof(expected))) {
		OBVR_LOG("Comfort: the call at %08X is not `call %08X` - %s is left to the game",
		         callSite, original, what);
		mem::ReportForeignCode("Comfort", callSite);
		return;
	}
	UInt8 patch[5];
	if (mem::BuildCallSitePatch(patch, sizeof(patch), callSite, replacement) != sizeof(patch) ||
	    !mem::SafeWrite(callSite, patch, sizeof(patch))) {
		OBVR_LOG("Comfort: could not reroute the call at %08X for %s", callSite, what);
		return;
	}
	OBVR_LOG("Comfort: %s rerouted at %08X", what, callSite);
}

}  // namespace

void InstallPlayerStagger() {
	const UInt32 onStagger = reinterpret_cast<UInt32>(&OnStagger);
	Reroute(kCallStaggerFromHit, kStaggerStart, onStagger, "the stagger from a hit");
	Reroute(kCallStaggerFromReach, kStaggerStart, onStagger, "the stagger from a reach search");
	Reroute(kCallKnockbackProxy, kCharacterProxyOf, reinterpret_cast<UInt32>(&OnKnockbackProxy),
	        "a hit's knockback");
	RerouteSlot(kKnockSlotHighProcess, "high process");
	RerouteSlot(kKnockSlotMiddleHighProcess, "middle-high process");
	RerouteStepSlot(kKnockStepSlotHighProcess, "high process");
	RerouteStepSlot(kKnockStepSlotMiddleHighProcess, "middle-high process");
}

void SetNoPlayerStagger(bool enabled) { g_enabled = enabled; }

void SetNoPlayerKnockdown(bool enabled) { g_knockdownEnabled = enabled; }

void SetExhaustion(bool enabled) { g_exhaustionEnabled = enabled; }

UInt32 TakeCollapseSkips() {
	const UInt32 skips = g_collapseSkips;
	g_collapseSkips = 0;
	return skips;
}

}  // namespace obvr::game
