#include "game/PlayerStagger.h"

#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

bool g_enabled = true;
bool g_staggerReported = false;
bool g_knockbackReported = false;

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
}

void SetNoPlayerStagger(bool enabled) { g_enabled = enabled; }

}  // namespace obvr::game
