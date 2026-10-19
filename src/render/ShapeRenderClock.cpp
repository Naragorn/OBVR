#include "render/ShapeRenderClock.h"

#include "core/Config.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "perf/Profiler.h"
#include "platform/Win32Min.h"

namespace obvr::render {
namespace {

// The renderer's shape dispatcher at 0x007693E0 (`ret 4`) renders a shape
// through one of two functions (objdump of Oblivion.exe 1.2.0.416,
// 2026-10-19): 0x00767520 (`this` in ecx, five stack arguments) from
// 0x007694BD, and 0x007672F0 (`this` in ecx, six stack arguments) from
// 0x007694D7 - the first sample run put 24 % of a pass's samples under the
// one and 46 % under the other, so both are clocked into shape_render. Their
// first seven bytes hold nothing relative and displace whole.
constexpr UInt32 kShapeRenderA = 0x00767520u;
constexpr UInt8 kShapeRenderAEntry[7] = {0x8B, 0x44, 0x24, 0x0C, 0x83, 0xEC, 0x1C};
constexpr UInt32 kShapeRenderB = 0x007672F0u;
constexpr UInt8 kShapeRenderBEntry[7] = {0x51, 0x53, 0x55, 0x8B, 0x6C, 0x24, 0x10};

using ShapeRenderAFn = UInt32(__fastcall*)(void* self, void* edx, UInt32 a1, UInt32 a2, UInt32 a3,
                                           UInt32 a4, UInt32 a5);
using ShapeRenderBFn = UInt32(__fastcall*)(void* self, void* edx, UInt32 a1, UInt32 a2, UInt32 a3,
                                           UInt32 a4, UInt32 a5, UInt32 a6);
ShapeRenderAFn g_originalA = nullptr;
ShapeRenderBFn g_originalB = nullptr;
UInt64 g_ticks = 0;
UInt32 g_calls = 0;
UInt32 g_probeLeft = 2;

UInt32 __fastcall HookedShapeRenderA(void* self, void* edx, UInt32 a1, UInt32 a2, UInt32 a3, UInt32 a4,
                                     UInt32 a5) {
	if (g_probeLeft > 0 && self != nullptr) {
		--g_probeLeft;
		const UInt32* vtable = *reinterpret_cast<UInt32* const*>(self);
		OBVR_LOG("Shape render A: this=%08X vtable=%08X slots +0x34=%08X +0x38=%08X +0x3C=%08X, "
		         "args %08X %08X %08X %08X %08X",
		         reinterpret_cast<UInt32>(self), reinterpret_cast<UInt32>(vtable), vtable[0x34 / 4],
		         vtable[0x38 / 4], vtable[0x3C / 4], a1, a2, a3, a4, a5);
	}
	const bool timed = perf::Profiler::Instance().IsCapturing() && GetConfig().performance.callClock;
	const UInt64 before = timed ? static_cast<UInt64>(ReadPerformanceCounter()) : 0;
	const UInt32 result = g_originalA(self, edx, a1, a2, a3, a4, a5);
	if (timed) {
		g_ticks += static_cast<UInt64>(ReadPerformanceCounter()) - before;
		++g_calls;
	}
	return result;
}

UInt32 __fastcall HookedShapeRenderB(void* self, void* edx, UInt32 a1, UInt32 a2, UInt32 a3, UInt32 a4,
                                     UInt32 a5, UInt32 a6) {
	const bool timed = perf::Profiler::Instance().IsCapturing() && GetConfig().performance.callClock;
	const UInt64 before = timed ? static_cast<UInt64>(ReadPerformanceCounter()) : 0;
	const UInt32 result = g_originalB(self, edx, a1, a2, a3, a4, a5, a6);
	if (timed) {
		g_ticks += static_cast<UInt64>(ReadPerformanceCounter()) - before;
		++g_calls;
	}
	return result;
}

// An entry detoured: the bytes checked, the trampoline built, the patch
// written; the original is then the trampoline.
bool InstallEntry(const char* what, UInt32 entry, const UInt8* bytes, UInt32 length, UInt32 replacement,
                  void*& original) {
	if (!mem::Verify(entry, bytes, length)) {
		OBVR_LOG("Shape render: the bytes at %08X differ - %s stays unclocked", entry, what);
		mem::ReportForeignCode("Shape render", entry);
		return false;
	}
	constexpr UInt32 kCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Shape render: no executable memory for %s - it stays unclocked", what);
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	if (mem::BuildEntryTrampoline(trampoline, kCapacity, trampolineAddress, entry, bytes, length) == 0) {
		OBVR_LOG("Shape render: the trampoline for %s does not fit - it stays unclocked", what);
		return false;
	}
	UInt8 patch[16];
	if (mem::BuildEntryPatch(patch, sizeof(patch), entry, replacement, length) != length) {
		OBVR_LOG("Shape render: the patch for %s has an unexpected length - it stays unclocked", what);
		return false;
	}
	original = trampoline;
	if (!mem::SafeWrite(entry, patch, length)) {
		original = nullptr;
		OBVR_LOG("Shape render: could not write the entry at %08X for %s - it stays unclocked", entry, what);
		return false;
	}
	OBVR_LOG("Shape render: %s at %08X is clocked (trampoline at %08X) - shape_render in the capture", what,
	         entry, trampolineAddress);
	return true;
}

}  // namespace

bool InstallShapeRenderClock() {
	if (g_originalA != nullptr || g_originalB != nullptr) {
		return true;
	}
	void* a = nullptr;
	void* b = nullptr;
	const bool okA = InstallEntry("the per-shape render A", kShapeRenderA, kShapeRenderAEntry,
	                              sizeof(kShapeRenderAEntry), reinterpret_cast<UInt32>(&HookedShapeRenderA), a);
	const bool okB = InstallEntry("the per-shape render B", kShapeRenderB, kShapeRenderBEntry,
	                              sizeof(kShapeRenderBEntry), reinterpret_cast<UInt32>(&HookedShapeRenderB), b);
	g_originalA = reinterpret_cast<ShapeRenderAFn>(a);
	g_originalB = reinterpret_cast<ShapeRenderBFn>(b);
	return okA && okB;
}

void TakeShapeRenderTicks(UInt64& ticks, UInt32& calls) {
	ticks = g_ticks;
	calls = g_calls;
	g_ticks = 0;
	g_calls = 0;
}

}  // namespace obvr::render
