#include "game/PlayerStagger.h"

#include <intrin.h>

#include "core/AddressSpace.h"
#include "core/AroundCall.h"
#include "core/CodeWriter.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/ExhaustionLogic.h"
#include "game/GameAddresses.h"
#include "game/NiMath.h"

#include <cmath>

namespace obvr::game {
namespace {

bool g_enabled = true;
bool g_knockdownEnabled = true;
bool g_staggerReported = false;
bool g_knockbackReported = false;
UInt32 g_knockdownLines = 20;
// The player's hits on others ([Hands] EnemyKnockback; HitKnockbackLogic.h).
HitKnockbackSettings g_enemyKnockback;
bool g_motionHitActive = false;
bool g_motionHitPower = false;
bool g_powerFlagChecked = false;
float g_hitShare = 1.0f;  // what the proxy call decided for the push that follows it
UInt32 g_enemyKnockbackLines = 24;

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

// The player's own knockback skipped, as the setting says.
bool PlayerKnockbackSkipped(void* actor) {
	if (!SkipForPlayer(g_enabled, reinterpret_cast<UInt32>(actor), Player())) {
		return false;
	}
	if (!g_knockbackReported) {
		g_knockbackReported = true;
		OBVR_LOG("Comfort: a hit's knockback of the player was skipped (Look.NoPlayerStagger)");
	}
	return true;
}

// Reached by `call` with the target in ecx and nothing on the stack - the
// fallback when the stub below could not be made: the player's own skip
// alone.
void* __fastcall OnKnockbackProxy(void* actor, void* /*edx*/) {
	g_hitShare = 1.0f;
	if (PlayerKnockbackSkipped(actor)) {
		return nullptr;
	}
	return reinterpret_cast<ActorPointerFn>(kCharacterProxyOf)(actor);
}

// Reached through the stub (InstallPlayerStagger): the target in ecx as the
// engine's call had it, the attacker - the hit handler's this, edi - in edx,
// and the handler's power attack flag pushed from its frame, which this
// callee takes off. Answers the proxy, or none when this hit is to push
// nobody, and leaves the share for OnHitKnockbackPush.
void* __fastcall OnHitKnockbackProxy(void* actor, void* attacker, UInt32 powerFlag) {
	g_hitShare = 1.0f;
	if (PlayerKnockbackSkipped(actor)) {
		return nullptr;
	}
	const bool handlerPower = (powerFlag & 0xFFu) != 0;
	// A strike by motion knows itself whether it is a power attack: taken
	// over the handler's flag, which is checked against it once.
	const bool power = g_motionHitActive ? g_motionHitPower : handlerPower;
	if (g_motionHitActive && !g_powerFlagChecked) {
		g_powerFlagChecked = true;
		OBVR_LOG("Comfort: the hit handler's power flag read %u for a %s strike by motion - %s", powerFlag & 0xFFu,
		         g_motionHitPower ? "power" : "light",
		         handlerPower == g_motionHitPower ? "the stack reading agrees" : "THE STACK READING DISAGREES");
	}
	const UInt32 player = Player();
	const bool byPlayer = player != 0 && reinterpret_cast<UInt32>(attacker) == player;
	const float share =
		HitKnockbackShare(g_enemyKnockback, byPlayer, reinterpret_cast<UInt32>(actor) == player, power);
	if (share <= 0.0f) {
		if (g_enemyKnockbackLines > 0) {
			--g_enemyKnockbackLines;
			OBVR_LOG("Comfort: the knockback of %08X by the player's %s hit is skipped (Hands.EnemyKnockback %.0f %%, "
			         "power only %d)",
			         reinterpret_cast<UInt32>(actor), power ? "power" : "light",
			         static_cast<double>(g_enemyKnockback.percent), g_enemyKnockback.powerOnly ? 1 : 0);
		}
		return nullptr;
	}
	g_hitShare = share;
	if (byPlayer && share >= 1.0f && g_enemyKnockbackLines > 0) {
		--g_enemyKnockbackLines;
		OBVR_LOG("Comfort: the knockback of %08X by the player's %s hit kept whole (Hands.EnemyKnockback %.0f %%)",
		         reinterpret_cast<UInt32>(actor), power ? "power" : "light",
		         static_cast<double>(g_enemyKnockback.percent));
	}
	return reinterpret_cast<ActorPointerFn>(kCharacterProxyOf)(actor);
}

using ProxyPushFn = void(__thiscall*)(void* proxy, const NiPoint3* distance, float seconds);

// Reached by `call` with the proxy in ecx and two stack arguments the callee
// clears (ret 8): a fastcall with them after edx does the same. The push's
// distance scaled by the share the proxy call left.
void __fastcall OnHitKnockbackPush(void* proxy, void* /*edx*/, const NiPoint3* distance, float seconds) {
	const float share = g_hitShare;
	g_hitShare = 1.0f;
	if (share >= 1.0f || distance == nullptr) {
		reinterpret_cast<ProxyPushFn>(kProxyKnockbackPush)(proxy, distance, seconds);
		return;
	}
	const NiPoint3 scaled{distance->x * share, distance->y * share, distance->z * share};
	if (g_enemyKnockbackLines > 0) {
		--g_enemyKnockbackLines;
		OBVR_LOG("Comfort: the knockback of the player's hit scaled to %.2f (Hands.EnemyKnockback %.0f %%): %.1f units "
		         "instead of %.1f over %.2f s",
		         static_cast<double>(share), static_cast<double>(g_enemyKnockback.percent),
		         std::sqrt(static_cast<double>(scaled.LengthSquared())),
		         std::sqrt(static_cast<double>(distance->LengthSquared())), static_cast<double>(seconds));
	}
	reinterpret_cast<ProxyPushFn>(kProxyKnockbackPush)(proxy, &scaled, seconds);
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
	// The proxy call through a stub that hands the attacker and the power
	// flag along (PlayerStagger.h, the push): `mov edx, edi; push dword
	// [esp+1E8h]; call OnHitKnockbackProxy; ret` - the callee takes the
	// pushed flag off, the ret goes back to the hit handler. Without the
	// stub, the player's own skip alone.
	UInt32 knockbackTarget = reinterpret_cast<UInt32>(&OnKnockbackProxy);
	if (UInt8* const stub = static_cast<UInt8*>(mem::AllocExecutable(32))) {
		mem::CodeWriter code(stub, 32, reinterpret_cast<UInt32>(stub));
		static const UInt8 kMoveEdxEdi[] = {0x8B, 0xD7};
		static const UInt8 kPushPowerFlag[] = {0xFF, 0xB4, 0x24, 0xE8, 0x01, 0x00, 0x00};
		code.Bytes(kMoveEdxEdi, sizeof(kMoveEdxEdi));
		code.Bytes(kPushPowerFlag, sizeof(kPushPowerFlag));
		code.CallRelative(reinterpret_cast<UInt32>(&OnHitKnockbackProxy));
		code.Byte(0xC3);
		if (!code.Overflowed()) {
			knockbackTarget = reinterpret_cast<UInt32>(stub);
		} else {
			OBVR_LOG("Comfort: the knockback stub overflowed its buffer - the player's own skip alone");
		}
	} else {
		OBVR_LOG("Comfort: no memory for the knockback stub - the player's own skip alone");
	}
	Reroute(kCallKnockbackProxy, kCharacterProxyOf, knockbackTarget, "a hit's knockback");
	Reroute(kCallKnockbackPush, kProxyKnockbackPush, reinterpret_cast<UInt32>(&OnHitKnockbackPush),
	        "a hit's knockback push");
	RerouteSlot(kKnockSlotHighProcess, "high process");
	RerouteSlot(kKnockSlotMiddleHighProcess, "middle-high process");
	RerouteStepSlot(kKnockStepSlotHighProcess, "high process");
	RerouteStepSlot(kKnockStepSlotMiddleHighProcess, "middle-high process");
}

void SetNoPlayerStagger(bool enabled) { g_enabled = enabled; }

void SetNoPlayerKnockdown(bool enabled) { g_knockdownEnabled = enabled; }

void SetExhaustion(bool enabled) { g_exhaustionEnabled = enabled; }

void SetEnemyKnockback(const HitKnockbackSettings& settings) { g_enemyKnockback = settings; }

void SetMotionHit(bool active, bool powerAttack) {
	g_motionHitActive = active;
	g_motionHitPower = powerAttack;
}

UInt32 TakeCollapseSkips() {
	const UInt32 skips = g_collapseSkips;
	g_collapseSkips = 0;
	return skips;
}

}  // namespace obvr::game
