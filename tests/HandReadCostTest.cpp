#include <cmath>
#include <cstdio>
#include <limits>

#include "game/HandReadCostLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::game::AddHandReadCost;
using obvr::game::HandReadCostMean;
using obvr::game::HandReadCostWindow;
using obvr::game::HandReadProbesMean;
using obvr::game::HandReadShapesMean;

bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }

void TestWindow() {
	std::printf("the hand fit's read, its cost a window\n");
	HandReadCostWindow w;
	Check(!AddHandReadCost(w, 100.0, 4, 30, 3), "the first read: no line yet");
	Check(!AddHandReadCost(w, 900.0, 6, 50, 3), "the second: no line yet");
	Check(AddHandReadCost(w, 200.0, 5, 10, 3), "the third of three: a line due");
	Check(w.reads == 3 && Near(w.sumMicros, 1200.0) && Near(w.maxMicros, 900.0) && w.shapes == 15 && w.probes == 90,
	      "the window holds the reads, the sum, the longest, the shapes and the probes");
	Check(Near(HandReadCostMean(w), 400.0), "its mean time");
	Check(Near(HandReadShapesMean(w), 5.0), "its mean shapes a read");
	Check(Near(HandReadProbesMean(w), 30.0), "its mean probes a read");
}

void TestEmptyAndRefused() {
	std::printf("the window's edges\n");
	HandReadCostWindow w;
	Check(HandReadCostMean(w) == 0.0 && HandReadShapesMean(w) == 0.0 && HandReadProbesMean(w) == 0.0,
	      "an empty window: every mean 0");
	Check(!AddHandReadCost(w, 5.0, 1, 1, 0) && w.reads == 1, "no reads per line: counted, never due");
	w = HandReadCostWindow{};
	const double nan = std::numeric_limits<double>::quiet_NaN();
	Check(!AddHandReadCost(w, nan, 1, 1, 1) && w.reads == 0, "a time that is no number: not counted");
	Check(!AddHandReadCost(w, -1.0, 1, 1, 1) && w.reads == 0, "a negative time: not counted");
	Check(AddHandReadCost(w, 0.0, 0, 0, 1) && w.reads == 1, "a read of nothing in no time: counted, due at one a line");
}

}  // namespace

int main() {
	TestWindow();
	TestEmptyAndRefused();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
