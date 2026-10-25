#include "game/ScriptClock.h"

#include <atomic>

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "core/Types.h"
#include "game/GameAddresses.h"
#include "game/ScriptClockLogic.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// ScriptRunner::Run: thiscall(ScriptRunner* this, Script* script,
// TESObjectREFR* thisObj, ScriptEventList* eventList, TESObjectREFR*
// containing, UInt8, UInt8, UInt8, float), ret 20h, the result in al. Its
// first seven bytes in 1.2.0.416 - push -1; push 009B735Bh - are whole
// instructions with nothing relative in them.
constexpr UInt32 kScriptRunnerRun = 0x00517330;
constexpr UInt8 kOriginalBytes[] = {0x6A, 0xFF, 0x68, 0x5B, 0x73, 0x9B, 0x00};
constexpr UInt32 kScriptTypeOffset = 0x28;

// __fastcall with a dummy edx mirrors __thiscall's register use; the eight
// arguments travel the stack in both and both clean it up. The bytes and
// the float are passed on as the 32-bit slots they occupy.
using RunFn = bool(__fastcall*)(void* runner, void* edx, const UInt8* script, const UInt8* thisObj,
                                void* eventList, void* containing, UInt32 a5, UInt32 a6, UInt32 a7, UInt32 a8);
RunFn g_original = nullptr;

bool g_installTried = false;
bool g_enabled = false;
UInt32 g_gameThread = 0;
long long g_frequency = 0;
std::atomic<UInt32> g_untimedRuns{0};
ScriptClock g_clock;

UInt32 ReadFormId(const UInt8* form) {
	return form != nullptr ? *reinterpret_cast<const UInt32*>(form + addr::kFormIdOffset) : 0;
}

bool __fastcall OnScriptRun(void* runner, void* /*edx*/, const UInt8* script, const UInt8* thisObj,
                            void* eventList, void* containing, UInt32 a5, UInt32 a6, UInt32 a7, UInt32 a8) {
	if (!g_enabled) {
		return g_original(runner, nullptr, script, thisObj, eventList, containing, a5, a6, a7, a8);
	}
	if (GetCurrentThreadId() != g_gameThread) {
		g_untimedRuns.fetch_add(1, std::memory_order_relaxed);
		return g_original(runner, nullptr, script, thisObj, eventList, containing, a5, a6, a7, a8);
	}
	const ScriptKind kind =
		script != nullptr ? ScriptKindOf(*reinterpret_cast<const UInt32*>(script + kScriptTypeOffset))
		                  : ScriptKind::Other;
	g_clock.Enter(ReadFormId(script), ReadFormId(thisObj), kind, ReadPerformanceCounter());
	const bool result = g_original(runner, nullptr, script, thisObj, eventList, containing, a5, a6, a7, a8);
	g_clock.Exit(ReadPerformanceCounter());
	return result;
}

bool Install() {
	if (!mem::Verify(kScriptRunnerRun, kOriginalBytes, sizeof(kOriginalBytes))) {
		OBVR_LOG("Script clock: the script run's entry at %08X differs; no clock", kScriptRunnerRun);
		mem::ReportForeignCode("Script clock", kScriptRunnerRun);
		return false;
	}
	g_frequency = ReadPerformanceFrequency();
	if (g_frequency <= 0) {
		OBVR_LOG("Script clock: no performance counter; no clock");
		return false;
	}
	constexpr UInt32 capacity = 24;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(capacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Script clock: no executable memory; no clock");
		return false;
	}
	if (mem::BuildEntryTrampoline(trampoline, capacity, reinterpret_cast<UInt32>(trampoline), kScriptRunnerRun,
	                              kOriginalBytes, sizeof(kOriginalBytes)) == 0) {
		OBVR_LOG("Script clock: trampoline did not fit; no clock");
		return false;
	}
	g_original = reinterpret_cast<RunFn>(trampoline);
	UInt8 patch[sizeof(kOriginalBytes)];
	if (mem::BuildEntryPatch(patch, sizeof(patch), kScriptRunnerRun, reinterpret_cast<UInt32>(&OnScriptRun),
	                         sizeof(kOriginalBytes)) == 0 ||
	    !mem::SafeWrite(kScriptRunnerRun, patch, sizeof(patch))) {
		OBVR_LOG("Script clock: could not patch the script run; no clock");
		g_original = nullptr;
		return false;
	}
	return true;
}

}  // namespace

void StepScriptClock(bool enabled, float thresholdMs) {
	if (enabled && !g_installTried) {
		g_installTried = true;
		// Patched from here, between frames on the game's thread, where no
		// script is running.
		if (Install()) {
			OBVR_LOG("Script clock: the script run at %08X detoured (%lld Hz)", kScriptRunnerRun, g_frequency);
		}
	}
	if (g_original == nullptr) return;
	if (enabled != g_enabled) {
		g_clock.Reset();
		g_untimedRuns.store(0, std::memory_order_relaxed);
		g_enabled = enabled;
		if (enabled) {
			OBVR_LOG("Script clock: on - a line for each frame whose scripts take %.1f ms or more (at most %u "
			         "in ten seconds), a summary every ten seconds",
			         static_cast<double>(thresholdMs), ScriptClock::kFrameLinesPerWindow);
		} else {
			OBVR_LOG("Script clock: off");
		}
	}
	g_gameThread = GetCurrentThreadId();
	if (!g_enabled) return;
	char frameLine[1024];
	char windowLine[1024];
	const ScriptClockLines lines =
		g_clock.FrameEnd(ReadPerformanceCounter(), g_frequency, thresholdMs,
		                 g_untimedRuns.exchange(0, std::memory_order_relaxed), frameLine, sizeof(frameLine),
		                 windowLine, sizeof(windowLine));
	if (lines.frameLine) OBVR_LOG("Script clock: %s", frameLine);
	if (lines.windowLine) OBVR_LOG("Script clock: %s", windowLine);
}

}  // namespace obvr::game
