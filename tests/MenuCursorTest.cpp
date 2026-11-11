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
	using obvr::game::KnobLineOffset;
	using obvr::game::kKnobLinePoints;
	using obvr::game::kKnobLineSteps;
	std::printf("past the rings: along the bar's line, then its column\n");
	Check(KnobLineOffset(0, 20.0f, dx, dy) && dx == 100.0f && dy == 0.0f, "the line's first: right, just past the rings");
	Check(KnobLineOffset(1, 20.0f, dx, dy) && dx == -100.0f && dy == 0.0f, "then left as far");
	Check(KnobLineOffset(2, 20.0f, dx, dy) && dx == 120.0f && dy == 0.0f, "then one step further right");
	Check(KnobLineOffset(kKnobLineSteps * 2 - 1, 20.0f, dx, dy) && dx == -20.0f * (4 + kKnobLineSteps) && dy == 0.0f,
	      "the line's last: left, 40 steps past the rings");
	Check(KnobLineOffset(kKnobLineSteps * 2, 20.0f, dx, dy) && dx == 0.0f && dy == 100.0f, "the column's first: down");
	Check(KnobLineOffset(kKnobLineSteps * 2 + 1, 20.0f, dx, dy) && dx == 0.0f && dy == -100.0f, "then up");
	Check(KnobLineOffset(kKnobLinePoints - 1, 20.0f, dx, dy) && dx == 0.0f && dy < 0.0f, "the column's last: up");
	Check(!KnobLineOffset(kKnobLinePoints, 20.0f, dx, dy), "past the column: none");
	Check(!KnobLineOffset(0, 0.0f, dx, dy), "no step: none");
	using obvr::game::KnobAxis;
	using obvr::game::KnobAxisOf;
	using obvr::game::KnobSearchOffset;
	using obvr::game::kKnobAxisSteps;
	std::printf("the search's axis from the pressed part\n");
	Check(KnobAxisOf("horizontal_scroll_rightside") == KnobAxis::Row, "a horizontal bar's side: its row");
	Check(KnobAxisOf("horizontal_scroll_left") == KnobAxis::Row, "a horizontal bar's arrow: its row");
	Check(KnobAxisOf("vertical_scroll_bottomside") == KnobAxis::Column, "a vertical bar's part: its column");
	Check(KnobAxisOf("video_gamma_slider_marker") == KnobAxis::Any, "a vanilla slider's track: anywhere");
	Check(KnobAxisOf(nullptr) == KnobAxis::Any, "no name: anywhere");
	Check(KnobSearchOffset(KnobAxis::Row, 0, 20.0f, dx, dy) && dx == 20.0f && dy == 0.0f, "row: one step right first");
	Check(KnobSearchOffset(KnobAxis::Row, 1, 20.0f, dx, dy) && dx == -20.0f && dy == 0.0f, "row: then left");
	Check(KnobSearchOffset(KnobAxis::Row, 5, 20.0f, dx, dy) && dx == -60.0f && dy == 0.0f, "row: never off the row");
	Check(KnobSearchOffset(KnobAxis::Row, kKnobAxisSteps * 2 - 1, 20.0f, dx, dy) && dy == 0.0f, "row: its last");
	Check(!KnobSearchOffset(KnobAxis::Row, kKnobAxisSteps * 2, 20.0f, dx, dy), "row: past its last, none");
	Check(KnobSearchOffset(KnobAxis::Column, 0, 20.0f, dx, dy) && dx == 0.0f && dy == 20.0f, "column: down first");
	Check(KnobSearchOffset(KnobAxis::Column, 3, 20.0f, dx, dy) && dx == 0.0f && dy == -40.0f, "column: never off it");
	Check(KnobSearchOffset(KnobAxis::Any, 0, 20.0f, dx, dy) && dx == 0.0f && dy == -20.0f, "anywhere: the rings first");
	Check(KnobSearchOffset(KnobAxis::Any, obvr::game::kKnobProbePoints, 20.0f, dx, dy) && dx == 100.0f && dy == 0.0f,
	      "anywhere: then the line");
	Check(!KnobSearchOffset(KnobAxis::Any, obvr::game::kKnobProbePoints + kKnobLinePoints, 20.0f, dx, dy),
	      "anywhere: past the column, none");
	Check(!KnobSearchOffset(KnobAxis::Row, 0, 0.0f, dx, dy), "no step: none");
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
