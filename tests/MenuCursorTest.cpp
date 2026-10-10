#include <cstdio>
#include <limits>

#include "game/MenuCursor.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::game::CursorReadIsEngines;

void TestEnginesOwn() {
	std::printf("the floats the tile search reads: the engine's own, or the pixel written last\n");
	// Before OBVR has written anything, whatever is there is the engine's -
	// 0,0 at the start (the tester's log, 2026-10-10: "the engine's own stood
	// at 0,0").
	Check(CursorReadIsEngines(false, 0.0f, 0.0f, 0.0f, 0.0f), "nothing written yet: the engine's own, even at 0,0");
	Check(CursorReadIsEngines(false, 0.0f, 0.0f, 1280.0f, 720.0f), "nothing written yet: the engine's own");
	// The engine writes the floats only when its own cursor moves; a frame
	// without a move leaves the pixel written last there.
	Check(!CursorReadIsEngines(true, 2180.0f, 212.0f, 2180.0f, 212.0f),
	      "exactly the pixel written last: still OBVR's, not the engine's");
	Check(CursorReadIsEngines(true, 2180.0f, 212.0f, 2181.0f, 212.0f), "x moved: the engine put it there");
	Check(CursorReadIsEngines(true, 2180.0f, 212.0f, 2180.0f, 213.5f), "y moved: the engine put it there");
	Check(CursorReadIsEngines(true, 2180.0f, 212.0f, 400.0f, 300.0f), "both moved: the engine's own");
}

void TestNotOnAScreen() {
	std::printf("a read that is no place on a screen is nobody's\n");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(!CursorReadIsEngines(false, 0.0f, 0.0f, nan, 300.0f), "a NaN x: refused");
	Check(!CursorReadIsEngines(true, 500.0f, 400.0f, 400.0f, nan), "a NaN y: refused");
	Check(!CursorReadIsEngines(false, 0.0f, 0.0f, -65.0f, 300.0f), "left of any screen: refused");
	Check(!CursorReadIsEngines(false, 0.0f, 0.0f, 400.0f, 16385.0f), "below any screen: refused");
	Check(CursorReadIsEngines(false, 0.0f, 0.0f, -64.0f, 16384.0f), "the bounds themselves: taken");
}

}  // namespace

int main() {
	TestEnginesOwn();
	TestNotOnAScreen();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
