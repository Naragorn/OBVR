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

void TestKnobNames() {
	std::printf("a slider's knob, and its other parts (names read in the gameplay options, 2026-10-10)\n");
	using obvr::game::IsScrollKnobName;
	using obvr::game::IsScrollPartName;
	Check(IsScrollKnobName("horizontal_scroll_marker"), "horizontal_scroll_marker: the knob");
	Check(IsScrollKnobName("vertical_scroll_marker"), "vertical_scroll_marker: a list's knob");
	Check(IsScrollKnobName("Horizontal_Scroll_Marker"), "any case: the knob");
	Check(!IsScrollKnobName("gameplay_difficulty_slider_marker"), "the track (\"slider_marker\"): not the knob");
	Check(!IsScrollKnobName("scroll_marke"), "shorter than the tail: no");
	Check(!IsScrollKnobName(nullptr), "no name: no");
	Check(IsScrollPartName("gameplay_difficulty_slider_marker"), "the track: a slider's part");
	Check(IsScrollPartName("horizontal_scroll_rightside"), "the bar's right side: a part");
	Check(IsScrollPartName("horizontal_scroll_left"), "the left arrow: a part");
	Check(!IsScrollPartName("horizontal_scroll_marker"), "the knob itself: no search");
	Check(!IsScrollPartName("pause_options_button"), "a button: no search");
	Check(!IsScrollPartName(nullptr), "no name: no search");
}

void TestKnobProbes() {
	std::printf("the points round the beam a press tries for the knob\n");
	using obvr::game::KnobProbeOffset;
	using obvr::game::kKnobProbePoints;
	float dx = 0.0f;
	float dy = 0.0f;
	Check(KnobProbeOffset(0, 20.0f, dx, dy) && dx == 0.0f && dy == -20.0f, "first: straight up, one step");
	Check(KnobProbeOffset(1, 20.0f, dx, dy) && dx == 0.0f && dy == 20.0f, "then straight down");
	Check(KnobProbeOffset(2, 20.0f, dx, dy) && dx == -20.0f && dy == 0.0f, "then left");
	Check(KnobProbeOffset(3, 20.0f, dx, dy) && dx == 20.0f && dy == 0.0f, "then right");
	Check(KnobProbeOffset(7, 20.0f, dx, dy) && dx > 14.0f && dx < 14.2f && dy > 14.0f, "the last of the ring: a diagonal");
	Check(KnobProbeOffset(9, 20.0f, dx, dy) && dx == 0.0f && dy == 40.0f, "the second ring: two steps");
	Check(KnobProbeOffset(kKnobProbePoints - 1, 20.0f, dx, dy) && dx > 56.5f && dx < 56.6f, "the last point: four steps out");
	Check(!KnobProbeOffset(kKnobProbePoints, 20.0f, dx, dy), "past the last: none");
	Check(!KnobProbeOffset(0, 0.0f, dx, dy), "no step: none");
}

}  // namespace

int main() {
	TestEnginesOwn();
	TestNotOnAScreen();
	TestKnobNames();
	TestKnobProbes();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
