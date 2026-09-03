// Checks the watchdog's decision: quiet before the first frame, moving while
// frames end, one declaration when they stop for the given ticks, silence
// while they stay stopped, and a fresh declaration only after frames moved.

#include <cstdio>

#include "core/StallDetector.h"

namespace {

using obvr::watchdog::StallDetector;
using obvr::watchdog::StallVerdict;
using obvr::watchdog::Tick;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestStartup() {
	std::printf("Startup\n");

	StallDetector d;
	Check(Tick(d, 0, 3) == StallVerdict::Quiet, "no frame ever: quiet");
	Check(Tick(d, 0, 3) == StallVerdict::Quiet, "still no frame: quiet, however long");
	Check(Tick(d, 0, 3) == StallVerdict::Quiet, "a start that never renders is not a hang");
	Check(Tick(d, 1, 3) == StallVerdict::Moving, "the first frame is movement");
}

void TestStall() {
	std::printf("Stall\n");

	StallDetector d;
	Tick(d, 5, 3);
	Check(Tick(d, 6, 3) == StallVerdict::Moving, "frames ending: moving");
	Check(Tick(d, 6, 3) == StallVerdict::Quiet, "one quiet tick is not a stall");
	Check(Tick(d, 6, 3) == StallVerdict::Quiet, "two are not either");
	Check(Tick(d, 6, 3) == StallVerdict::Stalled, "the third quiet tick declares the stall");
	Check(Tick(d, 6, 3) == StallVerdict::Standing, "and it is declared once");
	Check(Tick(d, 6, 3) == StallVerdict::Standing, "however long it stands");
	Check(Tick(d, 7, 3) == StallVerdict::Moving, "a frame ending clears it");
	Check(Tick(d, 7, 3) == StallVerdict::Quiet, "the count starts over");
	Check(Tick(d, 7, 3) == StallVerdict::Quiet, "two quiet ticks again");
	Check(Tick(d, 7, 3) == StallVerdict::Stalled, "and a second stall is declared afresh");
}

void TestWrap() {
	std::printf("Wrap\n");

	StallDetector d;
	Tick(d, 0xFFFFFFFFu, 2);
	Check(Tick(d, 0, 2) == StallVerdict::Moving, "the counter wrapping to zero is movement");
}

}  // namespace

int main() {
	TestStartup();
	TestStall();
	TestWrap();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
