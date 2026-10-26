#include "render/ShadowShareHook.h"

#include "core/Config.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "render/SceneRenderHook.h"
#include "render/ShadowShare.h"

namespace obvr::render {
namespace {

const UInt8 kEntry[addr::kRenderActorShadowsEntryLength] = {
	0x83, 0xEC, 0x4C,                          // sub esp,0x4C
	0x83, 0x3D, 0x48, 0x2F, 0xB4, 0x00, 0x03,  // cmp dword ptr [0x00B42F48],3
};
using RenderFn = void(__cdecl*)();
RenderFn g_original = nullptr;
ShadowShareState g_state;
bool g_firstReuseLogged = false;
// Every so many world renders, how the maps went: drawn and reused since the
// last line, so a run's log says whether the second eye kept the maps.
constexpr UInt32 kSummaryEveryCalls = 600;
UInt32 g_summaryCall = 0;
UInt32 g_summaryDrawn = 0;
UInt32 g_summaryReused = 0;

void __cdecl Hooked() {
	const UInt32 call = CurrentSceneCall();
	const ActorShadowPlan plan =
		PlanActorShadows(g_state, GetConfig().shareActorShadows, CurrentScenePassIndex(), call);
	if (plan == ActorShadowPlan::Draw) {
		g_original();
	} else if (!g_firstReuseLogged) {
		g_firstReuseLogged = true;
		OBVR_LOG("Shadows: the second eye keeps the first eye's actor shadow maps (ShareActorShadows) - first at "
		         "scene call %u",
		         call);
	}
	if (call >= g_summaryCall + kSummaryEveryCalls) {
		OBVR_LOG("Shadows: actor shadow maps drawn %u times, kept for the second eye %u times in the last %u world "
		         "renders (ShareActorShadows %u)",
		         g_state.drawn - g_summaryDrawn, g_state.reused - g_summaryReused, call - g_summaryCall,
		         GetConfig().shareActorShadows ? 1u : 0u);
		g_summaryCall = call;
		g_summaryDrawn = g_state.drawn;
		g_summaryReused = g_state.reused;
	}
}

}  // namespace

bool InstallShadowShareHook() {
	if (g_original != nullptr) return true;
	if (!mem::Verify(addr::kRenderActorShadows, kEntry, sizeof(kEntry))) {
		OBVR_LOG("Shadows: the actor shadow render's entry at %08X differs - each eye draws its own shadow maps",
		         addr::kRenderActorShadows);
		return false;
	}
	UInt8* trampoline = static_cast<UInt8*>(mem::AllocExecutable(24));
	if (trampoline == nullptr) return false;
	const UInt32 trampolineSize = mem::BuildEntryTrampoline(
		trampoline, 24, reinterpret_cast<UInt32>(trampoline), addr::kRenderActorShadows, kEntry, sizeof(kEntry));
	UInt8 patch[sizeof(kEntry)]{};
	const UInt32 patchSize = mem::BuildEntryPatch(patch, sizeof(patch), addr::kRenderActorShadows,
	                                              reinterpret_cast<UInt32>(&Hooked), sizeof(kEntry));
	if (trampolineSize == 0 || patchSize != sizeof(patch)) return false;
	g_original = reinterpret_cast<RenderFn>(trampoline);
	if (!mem::SafeWrite(addr::kRenderActorShadows, patch, sizeof(patch))) {
		g_original = nullptr;
		OBVR_LOG("Shadows: the actor shadow render at %08X could not be patched - each eye draws its own shadow "
		         "maps",
		         addr::kRenderActorShadows);
		return false;
	}
	OBVR_LOG("Shadows: the actor shadow render at %08X hooked; ShareActorShadows %u", addr::kRenderActorShadows,
	         GetConfig().shareActorShadows ? 1u : 0u);
	return true;
}

}  // namespace obvr::render
