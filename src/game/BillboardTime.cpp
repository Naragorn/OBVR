#include "game/BillboardTime.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"

namespace obvr::game {
namespace {

// NiBillboardNode::UpdateDownwardPass(float time, bool updateControllers),
// __thiscall with `ret 8`: the same call as __fastcall with a dead edx.
using DownwardFn = void(__fastcall*)(void* self, void* unusedEdx, float time, UInt32 updateControllers);

DownwardFn g_original = nullptr;

void __fastcall HookedDownwardPass(void* self, void* unusedEdx, float time, UInt32 updateControllers) {
	float* const kept = reinterpret_cast<float*>(static_cast<UInt8*>(self) + kBillboardSavedTimeOffset);
	const float before = *kept;
	g_original(self, unusedEdx, time, updateControllers);
	*kept = BillboardKeptTime((updateControllers & 0xFFu) != 0, before, time);
}

}  // namespace

void InstallBillboardTime(bool enabled) {
	if (!enabled) {
		OBVR_LOG("Billboard time: off (Render.KeepBillboardTime=0) - a transforms-only update sets the time a "
		         "billboard hands its controllers, as before");
		return;
	}
	if (g_original != nullptr) {
		return;
	}
	if (!mem::Verify(kBillboardDownwardPass, kBillboardDownwardEntry, sizeof(kBillboardDownwardEntry)) ||
	    !mem::Verify(kBillboardSavedTimeStore, kBillboardSavedTimeStoreBytes, sizeof(kBillboardSavedTimeStoreBytes))) {
		OBVR_LOG("Billboard time: the bytes at %08X are not the billboard's downward pass as read - left alone, "
		         "a held torch's flame may stand still in the dual pass",
		         kBillboardDownwardPass);
		mem::ReportForeignCode("Billboard time", kBillboardDownwardPass);
		return;
	}
	constexpr UInt32 kTrampolineCapacity = 16;
	auto* const trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Billboard time: no executable memory for the trampoline");
		return;
	}
	const UInt32 trampolineSize = mem::BuildEntryTrampoline(
		trampoline, kTrampolineCapacity, reinterpret_cast<UInt32>(trampoline), kBillboardDownwardPass,
		kBillboardDownwardEntry, sizeof(kBillboardDownwardEntry));
	UInt8 patch[sizeof(kBillboardDownwardEntry)];
	const UInt32 patchSize = mem::BuildEntryPatch(patch, sizeof(patch), kBillboardDownwardPass,
	                                              reinterpret_cast<UInt32>(&HookedDownwardPass),
	                                              sizeof(kBillboardDownwardEntry));
	if (trampolineSize == 0 || patchSize != sizeof(patch)) {
		OBVR_LOG("Billboard time: the detour's bytes do not fit");
		return;
	}
	g_original = reinterpret_cast<DownwardFn>(trampoline);
	if (!mem::SafeWrite(kBillboardDownwardPass, patch, patchSize)) {
		g_original = nullptr;
		OBVR_LOG("Billboard time: the write to %08X failed - left alone", kBillboardDownwardPass);
		return;
	}
	OBVR_LOG("Billboard time: NiBillboardNode's downward pass (%08X) keeps its controllers' time through "
	         "transforms-only updates - a held torch's flame and its sparks move in the dual pass",
	         kBillboardDownwardPass);
}

}  // namespace obvr::game
