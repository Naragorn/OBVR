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
	Check(SubmitHoldTicks(vsync - 12000, vsync, true, freq, 1.0) == 22000, "the margin adds to the hold");
	return failures == 0 ? 0 : 1;
}
