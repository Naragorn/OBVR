#include <cstdio>

#include "render/SubmitPacing.h"

namespace {
int failures = 0;
void Check(bool condition, const char* text) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", text);
	if (!condition) ++failures;
}
bool Near(double a, double b) { return a - b < 0.001 && b - a < 0.001; }
}  // namespace

int main() {
	using namespace obvr::render;
	// --- the vsync from the compositor's running start
	Check(Near(MsToVsyncFromRunningStart(-3.25f), 3.25), "poses 3.25 ms before the vsync put it 3.25 ms ahead");
	Check(Near(MsToVsyncFromRunningStart(-7.0f), 7.0), "a 7 ms running start is followed");
	Check(MsToVsyncFromRunningStart(0.0f) < 0.0, "poses at the vsync are not a running start");
	Check(MsToVsyncFromRunningStart(0.5f) < 0.0, "poses after the vsync are not a running start");
	Check(MsToVsyncFromRunningStart(-13.0f) < 0.0, "poses a period early are not a running start");
	// Nadi's Windows game, Pimax Dream Air at 90 Hz, 2026-10-25: the compositor
	// handed every frame's poses out 10.97 ms before its vsync - the whole
	// 11.11 ms period. Taken for a running start, that held every frame about
	// 9.3 ms and made it 13 ms long; SteamVR ran the game at half rate for 90 %
	// of the session.
	Check(MsToVsyncFromRunningStart(-10.97f) < 0.0, "poses a 90 Hz period early are not a running start");
	Check(MsToVsyncFromRunningStart(-8.0f) < 0.0, "poses 8 ms early are not a running start either");
	Check(Near(MsToVsyncFromRunningStart(-7.5f), 7.5), "7.5 ms is the longest running start followed");
	// --- the headset's period (Prop_DisplayFrequency_Float)
	Check(Near(PeriodMsFromDisplayHz(90.0f), 11.111), "90 Hz is a period of 11.11 ms");
	Check(Near(PeriodMsFromDisplayHz(144.0f), 6.944), "144 Hz is a period of 6.94 ms");
	Check(PeriodMsFromDisplayHz(0.0f) == 0.0, "no rate: the period unknown");
	Check(PeriodMsFromDisplayHz(-90.0f) == 0.0, "a negative rate: unknown");
	Check(PeriodMsFromDisplayHz(20.0f) == 0.0 && PeriodMsFromDisplayHz(1000.0f) == 0.0,
	      "a rate outside 30..500 Hz: unknown");
	volatile float zero = 0.0f;
	Check(PeriodMsFromDisplayHz(zero / zero) == 0.0, "not a number: unknown");
	// --- the running start's cap, by period
	Check(Near(RunningStartCapMs(0.0), 7.5), "the period unknown: the fixed 7.5 ms");
	Check(Near(RunningStartCapMs(1000.0 / 60.0), 7.5), "60 Hz: still 7.5 ms, the fixed cap is the lower");
	Check(Near(RunningStartCapMs(1000.0 / 75.0), 7.333), "75 Hz: 55 % of 13.33 ms");
	Check(Near(RunningStartCapMs(1000.0 / 90.0), 6.111), "90 Hz: 55 % of 11.11 ms");
	Check(Near(RunningStartCapMs(1000.0 / 144.0), 3.819), "144 Hz: 55 % of 6.94 ms");
	// The Beyond at 75 Hz under Linux: 3.0 to 3.3 ms, 7 under load - all
	// running starts still.
	const double p75 = 1000.0 / 75.0;
	Check(Near(MsToVsyncFromRunningStart(-3.2f, p75), 3.2), "75 Hz: a 3.2 ms running start is followed");
	Check(Near(MsToVsyncFromRunningStart(-7.0f, p75), 7.0), "75 Hz: the 7 ms seen under load is followed");
	// The Pimax at 90 Hz after the fix (2026-10-26): its poses 6.5 to 11 ms
	// before the vsync, 59 to 99 % of the period - never a running start.
	const double p90 = 1000.0 / 90.0;
	Check(Near(MsToVsyncFromRunningStart(-3.2f, p90), 3.2), "90 Hz: a 3.2 ms running start is followed");
	Check(MsToVsyncFromRunningStart(-6.47f, p90) < 0.0, "90 Hz: the Pimax's shortest, 6.47 ms, is not one");
	Check(MsToVsyncFromRunningStart(-10.97f, p90) < 0.0, "90 Hz: a whole period is not one");
	// 144 Hz (a tester on Virtual Desktop, 2026-10-23): a runtime that hands
	// the poses out a whole period early reports about 6.8 ms - under the
	// fixed 7.5 ms cap, so the old pacing held those frames past their vsync.
	const double p144 = 1000.0 / 144.0;
	Check(Near(MsToVsyncFromRunningStart(-3.0f, p144), 3.0), "144 Hz: a 3 ms running start is followed");
	Check(MsToVsyncFromRunningStart(-6.8f, p144) < 0.0, "144 Hz: a whole period, 6.8 ms, is not one");
	Check(MsToVsyncFromRunningStart(-4.5f, p144) < 0.0, "144 Hz: 4.5 ms, 65 % of the period, is not one");
	Check(MsToVsyncFromRunningStart(-6.8f) > 0.0, "the period unknown: 6.8 ms is followed as before");
	Check(MsToVsyncFromRunningStart(0.5f, p144) < 0.0, "144 Hz: poses after the vsync are not one");
	// --- the hold
	const UInt64 freq = 10000000;  // 10 MHz, 10000 ticks a millisecond
	const UInt64 vsync = 1000000000;
	Check(SubmitHoldTicks(vsync - 12000, vsync, true, freq) == 17000,
	      "a frame done 1.2 ms before the vsync is held to the vsync plus the margin");
	Check(SubmitHoldTicks(vsync + 10000, vsync, true, freq) == 0, "a frame done after the vsync goes at once");
	Check(SubmitHoldTicks(vsync + 5000, vsync, true, freq) == 0, "a frame done at the margin goes at once");
	Check(SubmitHoldTicks(vsync - 12000, vsync, false, freq) == 0, "an unknown vsync holds nothing");
	Check(SubmitHoldTicks(vsync - 12000, vsync, true, 0) == 0, "a zero frequency holds nothing");
	Check(SubmitHoldTicks(vsync - 200000, vsync, true, freq) == 0, "a hold longer than the cap is refused");
	// The hold covers the running start window and the margin: a frame that
	// would wait longer is early by more than a running start, nothing flips
	// for it, and holding it only adds latency - or a whole period, when the
	// vsync estimate is wrong (the Pimax case above).
	Check(SubmitHoldTicks(vsync - 35000, vsync, true, freq) == 40000, "a frame done 3.5 ms early is held 4 ms");
	Check(SubmitHoldTicks(vsync - 36000, vsync, true, freq) == 0, "a frame done 3.6 ms early goes at once");
	Check(SubmitHoldTicks(vsync - 90000, vsync, true, freq) == 0, "a frame done 9 ms early goes at once");
	Check(SubmitHoldTicks(vsync - 12000, vsync, true, freq, 1.0) == 22000, "the margin adds to the hold");
	return failures == 0 ? 0 : 1;
}
