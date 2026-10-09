#include "game/Parry.h"

#include <cstdio>

#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

bool g_enabled = false;
bool g_stopsAll = true;
float g_fatigue = 0.0f;
float g_owed = 0.0f;
ParryLedger g_ledger;
// The blow being resolved: the block forced for a parry, and the attacker
// whose blow the cone took as parried (consumed by the share).
bool g_forced = false;
UInt32 g_parriedBlow = 0;
// The frame's picture for the blow log: the player's blade, the eyes, the
// blades near.
bool g_haveBlade = false;
NiPoint3 g_guard{0.0f, 0.0f, 0.0f};
NiPoint3 g_tip{0.0f, 0.0f, 0.0f};
NiPoint3 g_eye{0.0f, 0.0f, 0.0f};
BladeFoes g_foes;
UInt32 g_blowLines = 24;
UInt32 g_parryLines = 24;
bool g_installed = false;

UInt32 Player() { return *reinterpret_cast<const UInt32*>(addr::kPlayerPointer); }

using BlockingFn = UInt8(__fastcall*)(void* actor, void* edx);
using ShareFn = float(__cdecl*)(UInt32 block, UInt32 luck, float factor, UInt32 flagA, UInt32 flagB);

// Every blow at the player, the first two dozen: what the engine says, who
// is parried, and where each attacker's blade stood at that moment - the
// timing the parry depends on (does a blow land before the blade reaches
// the player, as Skyrim's do?).
void LogBlow(bool engineBlocking, bool forced) {
	if (g_blowLines == 0) {
		return;
	}
	--g_blowLines;
	char foes[256] = {};
	UInt32 at = 0;
	for (UInt32 i = 0; i < g_foes.count && at + 80 < sizeof(foes); ++i) {
		const BladeFoe& f = g_foes.foe[i];
		const float tipToEyes = math::Sqrt((f.b - g_eye).LengthSquared());
		const float toOurs = g_haveBlade ? SegmentSegmentDistance(f.a, f.b, g_guard, g_tip) : -1.0f;
		const int n = std::snprintf(foes + at, sizeof(foes) - at, "%s%08X action %d tip %.0f from the eyes, %.0f from ours",
		                            i > 0 ? "; " : "", f.actor, static_cast<int>(f.action),
		                            static_cast<double>(tipToEyes), static_cast<double>(toOurs));
		if (n > 0) {
			at += static_cast<UInt32>(n);
		}
	}
	OBVR_LOG("Parry: a blow at the player - the engine says blocking %d, a block forced for a parry %d, %u parried; "
	         "blades near: %s",
	         engineBlocking ? 1 : 0, forced ? 1 : 0, g_ledger.count, g_foes.count > 0 ? foes : "none");
}

UInt8 __fastcall OnBlockingCheck(void* target, void* /*edx*/) {
	const UInt8 engine = reinterpret_cast<BlockingFn>(kBlockingCheck)(target, nullptr);
	const bool isPlayer = reinterpret_cast<UInt32>(target) == Player();
	g_parriedBlow = 0;
	g_forced = g_enabled && ForceBlockForParry(isPlayer, engine != 0, g_ledger.count > 0);
	if (isPlayer) {
		LogBlow(engine != 0, g_forced);
	}
	return g_forced ? 1 : engine;
}

float __cdecl OnBlockShare(UInt32 block, UInt32 luck, float factor, UInt32 flagA, UInt32 flagB) {
	const float engine = reinterpret_cast<ShareFn>(kBlockShare)(block, luck, factor, flagA, flagB);
	const bool parried = g_parriedBlow != 0;
	const float share = ParriedShare(parried, g_stopsAll, engine);
	if (parried && g_parryLines > 0) {
		--g_parryLines;
		OBVR_LOG("Parry: %08X's blow parried - blocked %.2f of it (the engine's own share %.2f, Block %u)",
		         g_parriedBlow, static_cast<double>(share), static_cast<double>(engine), block);
	}
	g_parriedBlow = 0;
	g_forced = false;
	return share;
}

bool Reroute(UInt32 site, UInt32 original, UInt32 replacement, const char* what) {
	const UInt32 displacement = mem::CallRelativeDisplacement(site, original);
	const UInt8 expected[5] = {0xE8, static_cast<UInt8>(displacement & 0xFF), static_cast<UInt8>((displacement >> 8) & 0xFF),
	                           static_cast<UInt8>((displacement >> 16) & 0xFF),
	                           static_cast<UInt8>((displacement >> 24) & 0xFF)};
	if (!mem::Verify(site, expected, sizeof(expected))) {
		OBVR_LOG("Parry: the call at %08X is not `call %08X` - %s left alone, no parries", site, original, what);
		mem::ReportForeignCode("Parry", site);
		return false;
	}
	UInt8 patch[5];
	if (mem::BuildCallSitePatch(patch, sizeof(patch), site, replacement) != sizeof(patch) ||
	    !mem::SafeWrite(site, patch, sizeof(patch))) {
		OBVR_LOG("Parry: could not reroute %s at %08X", what, site);
		return false;
	}
	return true;
}

}  // namespace

void InstallParry() {
	const UInt32 check = mem::CallRelativeDisplacement(kCallBlockingCheck, kBlockingCheck);
	const UInt32 share = mem::CallRelativeDisplacement(kCallBlockShare, kBlockShare);
	const UInt8 checkBytes[5] = {0xE8, static_cast<UInt8>(check & 0xFF), static_cast<UInt8>((check >> 8) & 0xFF),
	                             static_cast<UInt8>((check >> 16) & 0xFF), static_cast<UInt8>((check >> 24) & 0xFF)};
	const UInt8 shareBytes[5] = {0xE8, static_cast<UInt8>(share & 0xFF), static_cast<UInt8>((share >> 8) & 0xFF),
	                             static_cast<UInt8>((share >> 16) & 0xFF), static_cast<UInt8>((share >> 24) & 0xFF)};
	// Both or neither: a forced block without its share would let a parried
	// blow through at the engine's share; a share without the forced block
	// would never be asked.
	if (!mem::Verify(kCallBlockingCheck, checkBytes, sizeof(checkBytes)) ||
	    !mem::Verify(kCallBlockShare, shareBytes, sizeof(shareBytes))) {
		OBVR_LOG("Parry: the hit handler's block calls are not as read (%08X, %08X) - no parries", kCallBlockingCheck,
		         kCallBlockShare);
		return;
	}
	g_installed = Reroute(kCallBlockingCheck, kBlockingCheck, reinterpret_cast<UInt32>(&OnBlockingCheck),
	                      "the block check") &&
	              Reroute(kCallBlockShare, kBlockShare, reinterpret_cast<UInt32>(&OnBlockShare), "the blocked share");
	OBVR_LOG("Parry: %s", g_installed ? "the hit handler's block check (005FF7DF) and blocked share (005FF8C7) rerouted"
	                                  : "rerouting failed - no parries");
}

ParryEvent StepParry(const ParryFrame& f) {
	ParryEvent e;
	g_enabled = g_installed && f.enabled;
	g_stopsAll = f.stopsAll;
	g_fatigue = ParryFatigueCost(f.fatigue);
	g_ledger.Step(f.dtSeconds);
	const bool lastValid = g_haveBlade;
	const NiPoint3 lastGuard = g_guard;
	const NiPoint3 lastTip = g_tip;
	g_haveBlade = f.enabled && f.bladeValid;
	g_guard = f.guard;
	g_tip = f.tip;
	g_eye = f.eye;
	g_foes = f.foes != nullptr ? *f.foes : BladeFoes{};
	if (!g_enabled || !(g_haveBlade || (f.enabled && f.shieldValid))) {
		if (!f.enabled) {
			g_ledger = ParryLedger{};
		}
		return e;
	}
	for (UInt32 i = 0; i < g_foes.count; ++i) {
		const BladeFoe& foe = g_foes.foe[i];
		if (!IsAttackAction(foe.action)) {
			continue;
		}
		float share = 0.0f;
		NiPoint3 point{0.0f, 0.0f, 0.0f};
		bool byShield = false;
		bool met = g_haveBlade && BladesMeet(lastValid ? lastGuard : f.guard, lastValid ? lastTip : f.tip, f.guard,
		                                     f.tip, foe.lastA, foe.lastB, foe.a, foe.b, kParryReachUnits, share, point);
		if (!met && f.shieldValid &&
		    BladeMeetsBall(foe.lastA, foe.lastB, foe.a, foe.b, f.shieldCentre, f.shieldRadius, share)) {
			met = true;
			byShield = true;
			point = f.shieldCentre;
		}
		if (!met) {
			continue;
		}
		if (g_ledger.Add(foe.actor, kParryWindowSeconds) && !e.parried) {
			e.parried = true;
			e.byShield = byShield;
			e.actor = foe.actor;
			e.action = foe.action;
			e.point = point;
		}
	}
	return e;
}

ParryCone ParryConeFor(UInt32 target, UInt32 attacker) {
	if (!g_enabled) {
		return ParryCone::Original;
	}
	const bool isPlayer = target == Player();
	const ParryCone verdict = ConeForParry(isPlayer, g_ledger.Has(attacker), g_forced);
	if (verdict == ParryCone::Blocked) {
		g_parriedBlow = attacker;
		g_ledger.Take(attacker);
		g_owed += g_fatigue;
	} else if (verdict == ParryCone::NotBlocked && g_parryLines > 0) {
		--g_parryLines;
		OBVR_LOG("Parry: a blow from %08X under a block forced for another's parry - it lands", attacker);
	}
	return verdict;
}

float TakeParryFatigue() {
	const float owed = g_owed;
	g_owed = 0.0f;
	return owed;
}

void ForgetParries() {
	g_ledger = ParryLedger{};
	g_forced = false;
	g_parriedBlow = 0;
	g_haveBlade = false;
}

}  // namespace obvr::game
