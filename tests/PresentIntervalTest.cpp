// Checks the present interval the game's device is made with
// (render/PresentInterval.h): the monitor window presents without vsync while
// OBVR drives the headset, and keeps what the game asked for otherwise - with
// no headset, or with MonitorVsync=1.

#include <cstdio>

#include "render/PresentInterval.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s: %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::render::kPresentIntervalDefault;
using obvr::render::kPresentIntervalImmediate;
using obvr::render::kPresentIntervalOne;
using obvr::render::MonitorPresentInterval;
using obvr::render::MonitorPresentsAtOnce;

void TestDrivingTheHeadset() {
	std::printf("While OBVR drives the headset\n");
	Check(MonitorPresentInterval(kPresentIntervalOne, true, false) == kPresentIntervalImmediate,
	      "the game's vsync (iPresentInterval=1): presented at once");
	Check(MonitorPresentInterval(kPresentIntervalDefault, true, false) == kPresentIntervalImmediate,
	      "the default interval, which waits for the monitor as well: presented at once");
	Check(MonitorPresentInterval(2u, true, false) == kPresentIntervalImmediate,
	      "every second refresh: presented at once");
	Check(MonitorPresentInterval(kPresentIntervalImmediate, true, false) == kPresentIntervalImmediate,
	      "already at once: stays");
	Check(MonitorPresentInterval(kPresentIntervalOne, true, true) == kPresentIntervalOne,
	      "MonitorVsync=1: the game's own interval");
}

void TestNoHeadset() {
	std::printf("Without a headset to drive\n");
	Check(MonitorPresentInterval(kPresentIntervalOne, false, false) == kPresentIntervalOne,
	      "the game's vsync stays");
	Check(MonitorPresentInterval(kPresentIntervalDefault, false, false) == kPresentIntervalDefault,
	      "the default interval stays");
	Check(MonitorPresentInterval(kPresentIntervalImmediate, false, true) == kPresentIntervalImmediate,
	      "and with MonitorVsync=1 too: whatever the game asked for");
}

void TestAtOnce() {
	std::printf("Whether the window presents at once\n");
	Check(MonitorPresentsAtOnce(true, false), "driving the headset: at once");
	Check(!MonitorPresentsAtOnce(true, true), "driving it with MonitorVsync=1: not");
	Check(!MonitorPresentsAtOnce(false, false), "no headset: not");
	Check(!MonitorPresentsAtOnce(false, true), "no headset, MonitorVsync=1: not");
}

void TestTheConstants() {
	std::printf("The values, as d3d9caps.h defines them\n");
	Check(kPresentIntervalDefault == 0u && kPresentIntervalOne == 1u && kPresentIntervalImmediate == 0x80000000u,
	      "DEFAULT 0, ONE 1, IMMEDIATE 0x80000000");
}

}  // namespace

int main() {
	TestDrivingTheHeadset();
	TestNoHeadset();
	TestAtOnce();
	TestTheConstants();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
