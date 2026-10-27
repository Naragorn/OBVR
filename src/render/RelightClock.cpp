#include "render/RelightClock.h"

#include "core/Config.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "perf/ProfileLogic.h"
#include "perf/Profiler.h"
#include "platform/Win32Min.h"
#include "render/SceneRenderHook.h"

namespace obvr::render {
namespace {

// ShadowSceneNode::RelightGeometry at 0x007C6100 (Oblivion.exe 1.2.0.416): a
// `push -1; push <handler>` exception prologue, seven bytes with nothing
// relative in them, so they displace whole into the trampoline.
constexpr UInt32 kRelight = 0x007C6100u;
constexpr UInt8 kRelightEntry[7] = {0x6A, 0xFF, 0x68, 0x78, 0xE6, 0x9C, 0x00};

// __cdecl, as the function's plain `ret` says: a callee-cleaning hook here
// popped eight bytes the caller pops again and killed the first load
// (2026-10-27 10:32).
using RelightFn = UInt32(__cdecl*)(UInt32 geometry, UInt32 nodeOverride);
RelightFn g_original = nullptr;
UInt64 g_ticks = 0;
UInt32 g_timedCalls = 0;
UInt32 g_firstPass = 0;
UInt32 g_secondPass = 0;
UInt32 g_outside = 0;

UInt32 __cdecl HookedRelight(UInt32 geometry, UInt32 nodeOverride) {
	const int pass = CurrentScenePassIndex();
	if (pass == 0) {
		++g_firstPass;
	} else if (pass == 1) {
		++g_secondPass;
	} else {
		++g_outside;
	}
	const bool timed =
		perf::RelightTimed(perf::Profiler::Instance().IsCapturing(), GetConfig().performance.callClock);
	const UInt64 before = timed ? static_cast<UInt64>(ReadPerformanceCounter()) : 0;
	const UInt32 result = g_original(geometry, nodeOverride);
	if (timed) {
		g_ticks += static_cast<UInt64>(ReadPerformanceCounter()) - before;
		++g_timedCalls;
	}
	return result;
}

}  // namespace

bool InstallRelightClock() {
	if (g_original != nullptr) {
		return true;
	}
	if (!mem::Verify(kRelight, kRelightEntry, sizeof(kRelightEntry))) {
		OBVR_LOG("Relight clock: the bytes at %08X differ - the relight stays unclocked", kRelight);
		mem::ReportForeignCode("Relight clock", kRelight);
		return false;
	}
	constexpr UInt32 kCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Relight clock: no executable memory - the relight stays unclocked");
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	if (mem::BuildEntryTrampoline(trampoline, kCapacity, trampolineAddress, kRelight, kRelightEntry,
	                              sizeof(kRelightEntry)) == 0) {
		OBVR_LOG("Relight clock: the trampoline does not fit - the relight stays unclocked");
		return false;
	}
	UInt8 patch[16];
	if (mem::BuildEntryPatch(patch, sizeof(patch), kRelight, reinterpret_cast<UInt32>(&HookedRelight),
	                         sizeof(kRelightEntry)) != sizeof(kRelightEntry)) {
		OBVR_LOG("Relight clock: the patch has an unexpected length - the relight stays unclocked");
		return false;
	}
	g_original = reinterpret_cast<RelightFn>(trampoline);
	if (!mem::SafeWrite(kRelight, patch, sizeof(kRelightEntry))) {
		g_original = nullptr;
		OBVR_LOG("Relight clock: could not write the entry at %08X - the relight stays unclocked", kRelight);
		return false;
	}
	OBVR_LOG("Relight clock: the engine's per-geometry relight at %08X is clocked (trampoline at %08X) - "
	         "\"Relight at scene call\" lines on the heartbeat, `relight` in a capture with the call clock",
	         kRelight, trampolineAddress);
	return true;
}

void TakeRelightTicks(UInt64& ticks, UInt32& timedCalls) {
	ticks = g_ticks;
	timedCalls = g_timedCalls;
	g_ticks = 0;
	g_timedCalls = 0;
}

void TakeRelightCounts(UInt32& firstPass, UInt32& secondPass, UInt32& outside) {
	firstPass = g_firstPass;
	secondPass = g_secondPass;
	outside = g_outside;
	g_firstPass = 0;
	g_secondPass = 0;
	g_outside = 0;
}

}  // namespace obvr::render
