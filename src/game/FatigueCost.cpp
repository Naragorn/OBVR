#include "game/FatigueCost.h"

#include <intrin.h>

#include "core/BranchDecode.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

#pragma intrinsic(_ReturnAddress)

namespace obvr::game {
namespace {

struct CostSite {
	UInt32 site;
	vr::FatigueCostKind kind;
};

// The engine's calls of 0x005E07D0 that a two-hander in one hand makes
// dearer: the attack's and the block's.
constexpr CostSite kCostSites[] = {
	{0x005E4075, vr::FatigueCostKind::Attack},
	{0x005F5C5D, vr::FatigueCostKind::Block},
};
constexpr UInt32 kSiteCount = sizeof(kCostSites) / sizeof(kCostSites[0]);

// 0x005E07D0's first bytes as read: push ecx; fldz; push esi; fcomp [esp+0Ch].
constexpr UInt8 kChangeBytes[] = {0x51, 0xD9, 0xEE, 0x56, 0xD8, 0x5C, 0x24, 0x0C};

volatile float g_scale = 1.0f;
float g_extra[kFatigueCostKinds] = {};
UInt32 g_linesLeft[kSiteCount] = {4, 4};
bool g_installed = false;

UInt32 Player() { return *reinterpret_cast<const UInt32*>(addr::kPlayerPointer); }

using ChangeFn = void(__fastcall*)(void* actor, void* edx, float change);

// Reached by the redirected `call` with the actor in ecx and the change on
// the stack: a fastcall with one stack argument takes it the same way and
// returns the same way (ret 4).
void __fastcall ChangeFatigueScaled(void* actor, void* edx, float change) {
	float paid = change;
	const float scale = g_scale;
	if (scale > 1.0f && reinterpret_cast<UInt32>(actor) == Player()) {
		paid = vr::ScaledFatigueChange(change, scale);
		if (paid != change) {
			const UInt32 site = reinterpret_cast<UInt32>(_ReturnAddress()) - 5;
			for (UInt32 i = 0; i < kSiteCount; ++i) {
				if (kCostSites[i].site != site) {
					continue;
				}
				g_extra[static_cast<UInt32>(kCostSites[i].kind)] += change - paid;
				if (g_linesLeft[i] > 0) {
					--g_linesLeft[i];
					OBVR_LOG("Fatigue: %s cost %.2f x%.2f = %.2f (a two-hander in one hand)",
					         vr::FatigueCostKindWords(kCostSites[i].kind), static_cast<double>(-change),
					         static_cast<double>(scale), static_cast<double>(-paid));
				}
				break;
			}
		}
	}
	reinterpret_cast<ChangeFn>(addr::kActorChangeFatigue)(actor, edx, paid);
}

}  // namespace

void InstallFatigueCost() {
	if (g_installed) {
		return;
	}
	if (!mem::Verify(addr::kActorChangeFatigue, kChangeBytes, sizeof(kChangeBytes))) {
		OBVR_LOG("Fatigue: the engine's cost function (%08X) is not as read - the game's attack and block cost "
		         "as vanilla's, a two-hander in one hand too",
		         addr::kActorChangeFatigue);
		mem::ReportForeignCode("Fatigue", addr::kActorChangeFatigue);
		return;
	}
	const UInt32 target = reinterpret_cast<UInt32>(&ChangeFatigueScaled);
	UInt32 redirected = 0;
	for (UInt32 i = 0; i < kSiteCount; ++i) {
		const UInt32 site = kCostSites[i].site;
		const char* const what = vr::FatigueCostKindWords(kCostSites[i].kind);
		mem::RelativeBranch branch;
		if (!mem::DecodeRelativeBranch(reinterpret_cast<const UInt8*>(site), site, branch) || !branch.isCall ||
		    branch.target != addr::kActorChangeFatigue) {
			OBVR_LOG("Fatigue: the cost of %s (%08X) is not the call as read - paid as vanilla pays it", what, site);
			mem::ReportForeignCode("Fatigue", site);
			continue;
		}
		const UInt32 rel = target - (site + 5);
		const UInt8 displacement[4] = {static_cast<UInt8>(rel), static_cast<UInt8>(rel >> 8),
		                               static_cast<UInt8>(rel >> 16), static_cast<UInt8>(rel >> 24)};
		if (!mem::SafeWrite(site + 1, displacement, sizeof(displacement))) {
			OBVR_LOG("Fatigue: could not redirect the cost of %s (%08X) - paid as vanilla pays it", what, site);
			continue;
		}
		++redirected;
	}
	g_installed = redirected > 0;
	OBVR_LOG("Fatigue: %u of %u of the game's fighting costs pass through OBVR (its attack, its block) - a "
	         "two-hander in one hand makes them dearer by [Hands] OneHandTwoHanderFatigue; running, a jump, a "
	         "spell and a bow cost as the game has them",
	         redirected, kSiteCount);
}

void SetFatigueCostScale(float scale) { g_scale = scale > 1.0f && scale < 1.0e3f ? scale : 1.0f; }

float FatigueCostScale() { return g_scale; }

float PlayerFatigueCost(float cost, vr::FatigueCostKind kind) {
	if (!vr::OneHandScales(kind)) {
		return cost;
	}
	const float paid = -vr::ScaledFatigueChange(-cost, g_scale);
	if (paid != cost && paid == paid) {
		g_extra[static_cast<UInt32>(kind)] += paid - cost;
	}
	return paid;
}

void TakeFatigueCostExtras(float out[kFatigueCostKinds]) {
	for (UInt32 i = 0; i < kFatigueCostKinds; ++i) {
		out[i] = g_extra[i];
		g_extra[i] = 0.0f;
	}
}

}  // namespace obvr::game
