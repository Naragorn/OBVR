#include "game/FrameClock.h"

#include "core/Config.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "core/Types.h"
#include "game/FrameClockLogic.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// The time info's update: thiscall(TimeInfo* this, UInt32 tick), ret 4,
// called once a frame by the main loop at 0x0040D89F with GetTickCount's
// value. Its first nine bytes in 1.2.0.416 - mov edx,[esp+4]; fldz;
// sub edx,[ecx+14h] - are whole instructions with nothing relative in them.
constexpr UInt32 kTimeInfoUpdate = 0x0047D170;
constexpr UInt8 kOriginalBytes[] = {0x8B, 0x54, 0x24, 0x04, 0xD9, 0xEE, 0x2B, 0x51, 0x14};

// The global every frame's seconds are multiplied by at the update's end
// (fmul [0x00B06704] at 0x0047D247) - the time multiplier.
constexpr UInt32 kTimeMultiplier = 0x00B06704;

// __fastcall with a dummy edx mirrors __thiscall's register use; the tick
// travels the stack in both and both clean it up.
using UpdateFn = void(__fastcall*)(EngineTimeInfo* self, void* edx, UInt32 tick);
UpdateFn g_original = nullptr;

long long g_frequency = 0;
long long g_previous = 0;
bool g_havePrevious = false;

FrameClockTrace g_trace;
long long g_traceStarted = 0;

void __fastcall FrameClockUpdate(EngineTimeInfo* self, void* /*edx*/, UInt32 tick) {
	g_original(self, nullptr, tick);
	const float engineSeconds = self->frameSeconds;
	float given = engineSeconds;
	const long long now = ReadPerformanceCounter();
	if (PreciseClockApplies(self->frozen != 0, self->clampMs, g_havePrevious)) {
		given = PreciseFrameSeconds(now - g_previous, g_frequency,
		                            *reinterpret_cast<const float*>(kTimeMultiplier));
		self->frameSeconds = given;
	}
	// The previous reading moves only when a delta was taken from it, so a
	// frozen or clamped stretch is read afterwards the way the engine reads
	// it: as one long frame, held to the ceiling.
	if (!g_havePrevious || self->frameSeconds == given) {
		g_previous = now;
		g_havePrevious = true;
	}
	if (GetConfig().frameClockTrace) {
		g_trace.Add(engineSeconds, given);
		if (g_traceStarted == 0) g_traceStarted = now;
		if (g_frequency > 0 && now - g_traceStarted >= 10 * g_frequency) {
			char line[512];
			if (g_trace.Summary(line, sizeof(line))) {
				OBVR_LOG("Frame clock: %s", line);
			}
			g_trace.Reset();
			g_traceStarted = now;
		}
	}
}

}  // namespace

void InstallFrameClock() {
	static bool s_done = false;
	if (s_done) return;
	s_done = true;
	if (!GetConfig().tracker.preciseFrameClock) {
		OBVR_LOG("Frame clock: left to the engine ([Render] PreciseFrameClock=0)");
		return;
	}
	if (!mem::Verify(kTimeInfoUpdate, kOriginalBytes, sizeof(kOriginalBytes))) {
		OBVR_LOG("Frame clock: the time update's entry at %08X differs; left to the engine",
		         kTimeInfoUpdate);
		mem::ReportForeignCode("Frame clock", kTimeInfoUpdate);
		return;
	}
	g_frequency = ReadPerformanceFrequency();
	if (g_frequency <= 0) {
		OBVR_LOG("Frame clock: no performance counter; left to the engine");
		return;
	}
	constexpr UInt32 capacity = 24;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(capacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Frame clock: no executable memory; left to the engine");
		return;
	}
	if (mem::BuildEntryTrampoline(trampoline, capacity, reinterpret_cast<UInt32>(trampoline),
	                              kTimeInfoUpdate, kOriginalBytes, sizeof(kOriginalBytes)) == 0) {
		OBVR_LOG("Frame clock: trampoline did not fit; left to the engine");
		return;
	}
	g_original = reinterpret_cast<UpdateFn>(trampoline);
	UInt8 patch[sizeof(kOriginalBytes)];
	if (mem::BuildEntryPatch(patch, sizeof(patch), kTimeInfoUpdate,
	                         reinterpret_cast<UInt32>(&FrameClockUpdate), sizeof(kOriginalBytes)) == 0 ||
	    !mem::SafeWrite(kTimeInfoUpdate, patch, sizeof(patch))) {
		OBVR_LOG("Frame clock: could not patch the time update; left to the engine");
		return;
	}
	OBVR_LOG("Frame clock: precise - the frame's seconds come from the performance counter "
	         "(%lld Hz) instead of GetTickCount's whole milliseconds; trace %s",
	         g_frequency, GetConfig().frameClockTrace ? "on, a line every ten seconds" : "off");
}

}  // namespace obvr::game
