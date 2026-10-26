#include "game/FatigueCost.h"

#include <intrin.h>

#include "core/BranchDecode.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "vr/OneHandTwoHander.h"

#pragma intrinsic(_ReturnAddress)

namespace obvr::game {
namespace {

struct CostSite {
	UInt32 site;
	const char* what;
};

// The seven calls of 0x005E07D0, in the order of kFatigueCostKinds.
constexpr CostSite kCostSites[] = {
	{0x005E4075, "an attack"},  {0x005F5C5D, "a block"}, {0x005FACBF, "running"},
	{0x005FAD16, "a bow held drawn"}, {0x005FCA6F, "a spell"}, {0x005FD4D1, "a bow's shot"},
	{0x00672B7C, "a jump"},
};
constexpr UInt32 kSiteCount = sizeof(kCostSites) / sizeof(kCostSites[0]);
static_assert(kSiteCount + 1 == kFatigueCostKinds, "the engine's calls and OBVR's own");
constexpr UInt32 kOwnKind = kSiteCount;

// 0x005E07D0's first bytes as read: push ecx; fldz; push esi; fcomp [esp+0Ch].
constexpr UInt8 kChangeBytes[] = {0x51, 0xD9, 0xEE, 0x56, 0xD8, 0x5C, 0x24, 0x0C};

volatile float g_scale = 1.0f;
float g_extra[kFatigueCostKinds] = {};
UInt32 g_linesLeft[kFatigueCostKinds] = {3, 3, 3, 3, 3, 3, 3, 3};
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
			UInt32 kind = kSiteCount;
			for (UInt32 i = 0; i < kSiteCount; ++i) {
				if (kCostSites[i].site == site) {
					kind = i;
					break;
				}
			}
			if (kind < kSiteCount) {
				g_extra[kind] += change - paid;
				if (g_linesLeft[kind] > 0) {
					--g_linesLeft[kind];
					OBVR_LOG("Fatigue: the game's cost of %s %.2f x%.2f = %.2f (a two-hander in one hand)",
					         kCostSites[kind].what, static_cast<double>(-change), static_cast<double>(scale),
					         static_cast<double>(-paid));
				}
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
		OBVR_LOG("Fatigue: the engine's cost function (%08X) is not as read - its costs are paid as vanilla pays "
		         "them, a two-hander in one hand too",
		         addr::kActorChangeFatigue);
		mem::ReportForeignCode("Fatigue", addr::kActorChangeFatigue);
		return;
	}
	const UInt32 target = reinterpret_cast<UInt32>(&ChangeFatigueScaled);
	UInt32 redirected = 0;
	for (UInt32 i = 0; i < kSiteCount; ++i) {
		const UInt32 site = kCostSites[i].site;
		mem::RelativeBranch branch;
		if (!mem::DecodeRelativeBranch(reinterpret_cast<const UInt8*>(site), site, branch) || !branch.isCall ||
		    branch.target != addr::kActorChangeFatigue) {
			OBVR_LOG("Fatigue: the cost of %s (%08X) is not the call as read - paid as vanilla pays it",
			         kCostSites[i].what, site);
			mem::ReportForeignCode("Fatigue", site);
			continue;
		}
		const UInt32 rel = target - (site + 5);
		const UInt8 displacement[4] = {static_cast<UInt8>(rel), static_cast<UInt8>(rel >> 8),
		                               static_cast<UInt8>(rel >> 16), static_cast<UInt8>(rel >> 24)};
		if (!mem::SafeWrite(site + 1, displacement, sizeof(displacement))) {
			OBVR_LOG("Fatigue: could not redirect the cost of %s (%08X) - paid as vanilla pays it",
			         kCostSites[i].what, site);
			continue;
		}
		++redirected;
	}
	g_installed = redirected > 0;
	OBVR_LOG("Fatigue: %u of the engine's %u costs pass through OBVR (an attack, a block, running, a bow, a "
	         "spell, a jump) - a two-hander in one hand makes them dearer by [Hands] OneHandTwoHanderFatigue",
	         redirected, kSiteCount);
}

void SetFatigueCostScale(float scale) { g_scale = scale > 1.0f && scale < 1.0e3f ? scale : 1.0f; }

float FatigueCostScale() { return g_scale; }

float PlayerFatigueCost(float cost) {
	const float scale = g_scale;
	const float paid = -vr::ScaledFatigueChange(-cost, scale);
	if (paid != cost && paid == paid) {
		g_extra[kOwnKind] += paid - cost;
	}
	return paid;
}

const char* FatigueCostKindName(UInt32 kind) {
	if (kind < kSiteCount) {
		return kCostSites[kind].what;
	}
	return kind == kOwnKind ? "OBVR's own (a swing, a parry, a shove, a grab, a teleport)" : "nothing";
}

void TakeFatigueCostExtras(float out[kFatigueCostKinds]) {
	for (UInt32 i = 0; i < kFatigueCostKinds; ++i) {
		out[i] = g_extra[i];
		g_extra[i] = 0.0f;
	}
}

}  // namespace obvr::game
