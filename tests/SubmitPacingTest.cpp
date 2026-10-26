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
