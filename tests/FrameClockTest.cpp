// Checks the frame clock's decisions (game/FrameClockLogic.h): when the
// precise length applies, how the counter delta becomes the frame's seconds
// within the engine's own range, and what the trace reports.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "game/FrameClockLogic.h"

using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

void TestLayout() {
	std::printf("The engine's TimeInfo\n");
	EngineTimeInfo t{};
	Check(reinterpret_cast<char*>(&t.clampMs) - reinterpret_cast<char*>(&t) == 0x04, "clamp at +4");
	Check(reinterpret_cast<char*>(&t.clampCarryMs) - reinterpret_cast<char*>(&t) == 0x08, "carry at +8");
	Check(reinterpret_cast<char*>(&t.frameSeconds) - reinterpret_cast<char*>(&t) == 0x0C,
	      "the frame's seconds at +0xC (0x00B33E9C)");
	Check(reinterpret_cast<char*>(&t.lastMs) - reinterpret_cast<char*>(&t) == 0x10, "last ms at +0x10");
	Check(reinterpret_cast<char*>(&t.startTick) - reinterpret_cast<char*>(&t) == 0x14,
	      "start tick at +0x14 (0x00B33EA4)");
}

void TestApplies() {
	std::printf("When the precise length applies\n");
	Check(PreciseClockApplies(false, 0.0f, true), "running, no clamp, a previous reading: applies");
	Check(!PreciseClockApplies(true, 0.0f, true), "frozen: the engine's zero stands");
	Check(!PreciseClockApplies(false, 16.6667f, true), "iFPSClamp set: the fixed step stands");
	Check(!PreciseClockApplies(false, 0.0f, false), "first frame: nothing to take a delta from");
}

void TestSeconds() {
	std::printf("The seconds from the counter\n");
	const long long hz = 10000000;   // a 10 MHz counter
	Check(Near(PreciseFrameSeconds(133333, hz, 1.0f), 0.0133333, 1e-6),
	      "13.3333 ms at 75 Hz reads 13.3333 ms, not 13 or 14");
	Check(Near(PreciseFrameSeconds(111111, hz, 1.0f), 0.0111111, 1e-6), "11.1111 ms at 90 Hz");
	Check(Near(PreciseFrameSeconds(111111, hz, 0.5f), 0.00555555, 1e-6),
	      "the engine's multiplier scales the result");
	Check(Near(PreciseFrameSeconds(2000, hz, 1.0f), 0.001, 1e-7),
	      "0.2 ms reads as the engine's floor of one millisecond");
	Check(Near(PreciseFrameSeconds(0, hz, 1.0f), 0.001, 1e-7), "a zero delta reads as one millisecond");
	Check(Near(PreciseFrameSeconds(-5000, hz, 1.0f), 0.001, 1e-7),
	      "a counter that went backwards reads as one millisecond");
	Check(Near(PreciseFrameSeconds(3000000, hz, 1.0f), 0.166, 1e-6),
	      "300 ms reads as the engine's ceiling of 166 ms");
	Check(Near(PreciseFrameSeconds(1666000, hz, 1.0f), 0.1666, 1e-6),
	      "166.6 ms is under the engine's ceiling of 166.67 and passes as it is");
	Check(Near(PreciseFrameSeconds(1667000, hz, 1.0f), 0.166, 1e-6),
	      "166.7 ms is over it and reads as the engine's 166");
	Check(Near(PreciseFrameSeconds(133333, 0, 1.0f), 0.001, 1e-7), "no frequency: the floor");
}

void TestTrace() {
	std::printf("The trace\n");
	FrameClockTrace trace;
	char line[512];
	Check(!trace.Summary(line, sizeof(line)) && line[0] == 0, "no frames: no summary");

	// An integer-millisecond engine at 75 Hz: 13 and 14 alternate, the
	// precise clock gives 13.333 every time.
	for (int i = 0; i < 30; ++i) {
		trace.Add(i % 3 == 0 ? 0.014f : 0.013f, 0.0133333f);
	}
	Check(trace.frames == 30 && trace.distinct == 2, "two distinct engine values over 30 frames");
	Check(trace.Summary(line, sizeof(line)), "a summary fits 512 bytes");
	Check(std::strstr(line, "30 frames, engine ms 13.0:20 14.0:10") == line,
	      "the engine's values, most frequent first, with their counts");
	Check(std::strstr(line, "given ms mean 13.333 min 13.333 max 13.333") != nullptr,
	      "the given lengths' mean, minimum and maximum");

	// Values a tenth of a millisecond apart are distinct, closer ones merge.
	trace.Reset();
	trace.Add(0.0133f, 0.0133f);
	trace.Add(0.01334f, 0.01334f);
	trace.Add(0.0134f, 0.0134f);
	Check(trace.distinct == 2 && trace.engineCount[0] == 2, "13.30 and 13.34 merge, 13.40 is its own");

	// More distinct values than slots are counted as other.
	trace.Reset();
	for (int i = 0; i < 20; ++i) {
		trace.Add(0.010f + 0.001f * static_cast<float>(i), 0.010f);
	}
	Check(trace.distinct == FrameClockTrace::kDistinct && trace.engineOverflow == 8,
	      "twenty distinct values: twelve kept, eight counted as other");
	Check(trace.Summary(line, sizeof(line)) && std::strstr(line, " other:8") != nullptr,
	      "the summary names the other count");

	// A buffer too small for the summary says so.
	Check(!trace.Summary(line, 20), "a short buffer: false");

	trace.Reset();
	Check(trace.frames == 0 && trace.distinct == 0 && trace.engineOverflow == 0, "reset empties it");
}

}  // namespace

int main() {
	TestLayout();
	TestApplies();
	TestSeconds();
	TestTrace();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
