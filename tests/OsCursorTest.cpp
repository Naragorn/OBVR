#include <cstdio>
#include <limits>

#include "game/OsCursorLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::game::AnswerOsCursor;
using obvr::game::OsCursorAsk;
using obvr::game::OsCursorAnswer;

OsCursorAsk Menu(float x, float y, UInt32 asked, UInt32 believed) {
	OsCursorAsk a;
	a.menuMode = true;
	a.cursorKnown = true;
	a.x = x;
	a.y = y;
	a.askedHeight = asked;
	a.believedHeight = believed;
	return a;
}

void TestPassThrough() {
	std::printf("the real cursor is answered\n");
	OsCursorAsk a = Menu(100.0f, 100.0f, 1440, 1404);
	a.menuMode = false;
	Check(!AnswerOsCursor(a).replace, "in game mode: the real cursor");
	a = Menu(100.0f, 100.0f, 1440, 1404);
	a.cursorKnown = false;
	Check(!AnswerOsCursor(a).replace, "the interface cursor unreadable: the real cursor");
	a = Menu(100.0f, 100.0f, 1440, 0);
	Check(!AnswerOsCursor(a).replace, "no believed size: the real cursor");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(!AnswerOsCursor(Menu(nan, 100.0f, 1440, 1404)).replace, "a NaN x: the real cursor");
	Check(!AnswerOsCursor(Menu(100.0f, nan, 1440, 1404)).replace, "a NaN y: the real cursor");
	Check(!AnswerOsCursor(Menu(20000.0f, 100.0f, 1440, 1404)).replace, "off any screen: the real cursor");
	Check(!AnswerOsCursor(Menu(100.0f, -70.0f, 1440, 1404)).replace, "above any screen: the real cursor");
}

void TestMapped() {
	std::printf("in a menu: the interface cursor, in the asked size's pixels\n");
	// The 2D believing a raised size (2496 x 1404 of a 2496 x 2688 eye frame)
	// while iSize H still reads the game's own 1440: LINK's 960 / iSize H
	// must give the same 960-unit x as the believed size's own mapping.
	OsCursorAnswer v = AnswerOsCursor(Menu(1248.0f, 702.0f, 1440, 1404));
	Check(v.replace, "a menu with the cursor known: replaced");
	Check(v.x == 1280 && v.y == 720, "the believed 1248,702 is the asked 1280,720");
	const float unitsBelieved = 1248.0f * (960.0f / 1404.0f);
	const float unitsLink = static_cast<float>(v.x) * (960.0f / 1440.0f);
	const float diff = unitsBelieved - unitsLink;
	Check(diff < 1.0f && diff > -1.0f, "LINK's 960 / iSize H lands on the same menu unit");
	v = AnswerOsCursor(Menu(300.4f, 200.6f, 0, 1080));
	Check(v.replace && v.x == 300 && v.y == 201, "the asked size unknown: the believed one, rounded");
	v = AnswerOsCursor(Menu(500.0f, 400.0f, 1080, 1080));
	Check(v.x == 500 && v.y == 400, "no raised size: the same pixels");
	v = AnswerOsCursor(Menu(-10.4f, -0.6f, 1080, 1080));
	Check(v.replace && v.x == -10 && v.y == -1, "a little left of the screen: rounded away from zero");
}

}  // namespace

int main() {
	TestPassThrough();
	TestMapped();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
