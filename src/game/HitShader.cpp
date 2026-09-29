#include "game/HitShader.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"

namespace obvr::game {
namespace {

// The entries' first whole instructions, read from this machine's
// Oblivion.exe and verified before patching. Nothing relative in them.
// 0x007EB010: sub esp,8 / cmp byte ptr [00B2D91C],0
constexpr UInt8 kEntryBlocked[] = {0x83, 0xEC, 0x08, 0x80, 0x3D, 0x1C, 0xD9, 0xB2, 0x00, 0x00};
// 0x007EB080: push ecx / cmp byte ptr [00B2D91C],0
constexpr UInt8 kEntryAmount[] = {0x51, 0x80, 0x3D, 0x1C, 0xD9, 0xB2, 0x00, 0x00};

using StartBlockedFn = void(__cdecl*)(UInt32 blocked);
using StartAmountFn = void(__cdecl*)(float amount);
StartBlockedFn g_originalBlocked = nullptr;
StartAmountFn g_originalAmount = nullptr;

bool g_noHitBlur = false;
bool g_skipReported = false;

void ReportSkip() {
	if (!g_skipReported) {
		g_skipReported = true;
		OBVR_LOG("Comfort: the hit blur was skipped (Look.NoHitBlur)");
	}
}

void __cdecl OnStartBlocked(UInt32 blocked) {
	if (!HitShaderRuns(g_noHitBlur)) {
		ReportSkip();
		return;
	}
	g_originalBlocked(blocked);
}

void __cdecl OnStartAmount(float amount) {
	if (!HitShaderRuns(g_noHitBlur)) {
		ReportSkip();
		return;
	}
	g_originalAmount(amount);
}

// Detours one entry; returns the trampoline that runs the original, or 0.
UInt32 Detour(UInt32 entry, const UInt8* bytes, UInt32 length, UInt32 replacement, const char* what) {
	if (!mem::Verify(entry, bytes, length)) {
		OBVR_LOG("Comfort: the bytes at %08X differ - %s is left to the game", entry, what);
		mem::ReportForeignCode("Comfort", entry);
		return 0;
	}
	constexpr UInt32 kTrampolineCapacity = 24;
	auto* const trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Comfort: no executable memory for %s", what);
		return 0;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	if (mem::BuildEntryTrampoline(trampoline, kTrampolineCapacity, trampolineAddress, entry, bytes,
	                              length) == 0) {
		OBVR_LOG("Comfort: the trampoline for %s does not fit", what);
		return 0;
	}
	UInt8 patch[16];
	if (mem::BuildEntryPatch(patch, sizeof(patch), entry, replacement, length) != length ||
	    !mem::SafeWrite(entry, patch, length)) {
		OBVR_LOG("Comfort: could not detour %s at %08X", what, entry);
		return 0;
	}
	OBVR_LOG("Comfort: %s detoured at %08X", what, entry);
	return trampolineAddress;
}

}  // namespace

void InstallHitShader() {
	if (g_originalBlocked == nullptr) {
		g_originalBlocked = reinterpret_cast<StartBlockedFn>(
			Detour(kHitShaderStartBlocked, kEntryBlocked, sizeof(kEntryBlocked),
			       reinterpret_cast<UInt32>(&OnStartBlocked), "the hit blur's start (blocked)"));
	}
	if (g_originalAmount == nullptr) {
		g_originalAmount = reinterpret_cast<StartAmountFn>(
			Detour(kHitShaderStartAmount, kEntryAmount, sizeof(kEntryAmount),
			       reinterpret_cast<UInt32>(&OnStartAmount), "the hit blur's start (amount)"));
	}
}

void SetNoHitBlur(bool enabled) { g_noHitBlur = enabled; }

}  // namespace obvr::game
