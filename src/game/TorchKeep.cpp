#include "game/TorchKeep.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "game/TorchKeepLogic.h"

namespace obvr::game {
namespace {

// The byte the stubs compare: 1 keeps the torch.
volatile UInt8 g_keepTorch = 0;
bool g_installed = false;

}  // namespace

void InstallTorchKeep() {
	if (g_installed) {
		return;
	}
	if (!mem::Verify(kTorchEquipSite, kTorchEquipSiteBytes, sizeof(kTorchEquipSiteBytes)) ||
	    !mem::Verify(kTorchRefreshSite, kTorchRefreshSiteBytes, sizeof(kTorchRefreshSiteBytes))) {
		OBVR_LOG("Torch: the left hand's decision (005E7230) is not as read at %08X / %08X - left alone; a "
		         "two-hander drawn puts the torch away as vanilla does",
		         kTorchEquipSite, kTorchRefreshSite);
		mem::ReportForeignCode("Torch", kTorchEquipSite);
		mem::ReportForeignCode("Torch", kTorchRefreshSite);
		return;
	}
	auto* const stubs = static_cast<UInt8*>(mem::AllocExecutable(2 * kTorchKeepStubMax));
	if (stubs == nullptr) {
		OBVR_LOG("Torch: no memory for the left hand's stubs - a two-hander drawn puts the torch away");
		return;
	}
	UInt8* const equipStub = stubs;
	UInt8* const refreshStub = stubs + kTorchKeepStubMax;
	const UInt32 flag = reinterpret_cast<UInt32>(&g_keepTorch);
	UInt8 equipPatch[8];
	UInt8 refreshPatch[8];
	const bool built =
		BuildTorchKeepStub(equipStub, kTorchKeepStubMax, reinterpret_cast<UInt32>(equipStub), TorchKeepSite::Equip,
		                   flag) != 0 &&
		BuildTorchKeepStub(refreshStub, kTorchKeepStubMax, reinterpret_cast<UInt32>(refreshStub),
		                   TorchKeepSite::Refresh, flag) != 0 &&
		BuildTorchKeepSitePatch(equipPatch, sizeof(equipPatch), TorchKeepSite::Equip,
		                        reinterpret_cast<UInt32>(equipStub)) == sizeof(kTorchEquipSiteBytes) &&
		BuildTorchKeepSitePatch(refreshPatch, sizeof(refreshPatch), TorchKeepSite::Refresh,
		                        reinterpret_cast<UInt32>(refreshStub)) == sizeof(kTorchRefreshSiteBytes);
	if (!built) {
		OBVR_LOG("Torch: the left hand's stubs could not be built - a two-hander drawn puts the torch away");
		return;
	}
	// Both or neither: the refresh alone would bring the torch back on the
	// draw and the next equip would take it away again.
	if (!mem::SafeWrite(kTorchEquipSite, equipPatch, sizeof(kTorchEquipSiteBytes))) {
		OBVR_LOG("Torch: could not write the jump at %08X - a two-hander drawn puts the torch away", kTorchEquipSite);
		return;
	}
	if (!mem::SafeWrite(kTorchRefreshSite, refreshPatch, sizeof(kTorchRefreshSiteBytes))) {
		mem::SafeWrite(kTorchEquipSite, kTorchEquipSiteBytes, sizeof(kTorchEquipSiteBytes));
		OBVR_LOG("Torch: could not write the jump at %08X - both put back, a two-hander drawn puts the torch away",
		         kTorchRefreshSite);
		return;
	}
	g_installed = true;
	OBVR_LOG("Torch: the left hand's decision (005E7230) asks OBVR at %08X and %08X - with [Hands] "
	         "TorchWithTwoHander the player's torch stays in the hand with a two-handed blade or blunt drawn",
	         kTorchEquipSite, kTorchRefreshSite);
}

void SetTorchKeep(bool keep) { g_keepTorch = keep ? 1 : 0; }

bool TorchKeepInstalled() { return g_installed; }

}  // namespace obvr::game
