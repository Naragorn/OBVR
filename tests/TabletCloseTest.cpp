// Checks the frames the tablet's menus close on (vr/TabletClose.h): only the
// HUD's menus left in the stack, and the closing frames that follow a frame
// with the tablet in the hand - kept apart from another menu taking the
// pages' place, a menu closing without the tablet, and a page read for a
// frame without its entry.

#include <cstdio>

#include "vr/TabletClose.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestOnlyHudMenus() {
	std::printf("Only the HUD's menus in the stack\n");
	Check(OnlyHudMenus(nullptr, 0), "an empty stack: nothing but the HUD's");
	const UInt32 hud[] = {game::kMenuIdHudMain, game::kMenuIdHudInfo, game::kMenuIdHudReticle, game::kMenuIdHudSubtitle};
	Check(OnlyHudMenus(hud, 4), "the four HUD menus: nothing but the HUD's");
	const UInt32 pages[] = {game::kMenuIdHudMain, game::kMenuIdBigFour};
	Check(!OnlyHudMenus(pages, 2), "the pages' entry: a menu");
	const UInt32 esc[] = {game::kMenuIdBigFour, game::kMenuIdPause};
	Check(!OnlyHudMenus(esc, 2), "the Esc menu over the pages: a menu");
	const UInt32 box[] = {game::kMenuIdMessage};
	Check(!OnlyHudMenus(box, 1), "a message box alone: a menu");
}

void TestClosing() {
	std::printf("The closing frames\n");
	TabletCloseState s;
	Check(!StepTabletClose(s, true, false), "the tablet in the hand: not closing");
	Check(StepTabletClose(s, false, true), "put down, menu mode on, only the HUD's left: closing");
	Check(s.frames == 1, "one closing frame counted");
	Check(StepTabletClose(s, false, true), "the fade's next frame: closing");
	Check(StepTabletClose(s, false, true), "and the next");
	Check(s.frames == 3, "three closing frames counted");
	Check(!StepTabletClose(s, false, false), "menu mode over: no longer closing");
	Check(s.frames == 0, "the count starts over");
	Check(!StepTabletClose(s, false, true), "a menu closing later without the tablet: not the tablet's");
}

void TestNotClosing() {
	std::printf("Put down, but not closing\n");
	TabletCloseState s;
	StepTabletClose(s, true, false);
	Check(!StepTabletClose(s, false, false),
	      "another menu takes the pages' place (the Esc menu): not closing - its own panel shows");
	Check(!StepTabletClose(s, false, true), "its stack empties later: the tablet was not up the frame before");
	TabletCloseState never;
	Check(!StepTabletClose(never, false, true), "a menu closing with no tablet ever up: not closing");
	Check(never.frames == 0, "nothing counted");
}

void TestPageBack() {
	std::printf("The pages' entry gone for a frame\n");
	TabletCloseState s;
	StepTabletClose(s, true, false);
	Check(StepTabletClose(s, false, true), "the stack reads empty for a frame: held as closing - nothing in the room");
	Check(!StepTabletClose(s, true, false), "the pages back: the tablet in the hand again, not closing");
	Check(s.frames == 0, "the count starts over");
}

void TestWorldRunsThroughFade() {
	std::printf("The world runs on through the close's fade\n");
	bool ran = false;
	Check(!StepTabletRunsWorld(ran, false, false, true), "no menu mode: the world is not the tablet's to run");
	Check(StepTabletRunsWorld(ran, true, true, false), "the tablet's menus up, the world running behind them: runs");
	Check(StepTabletRunsWorld(ran, true, true, false), "asked again the same frame: the same answer");
	Check(StepTabletRunsWorld(ran, true, false, true),
	      "closed - menu mode on, only the HUD's menus left as they fade: runs on (paused 22 frames before)");
	Check(StepTabletRunsWorld(ran, true, false, true), "the fade's next frame, and the next site asking: runs on");
	Check(!StepTabletRunsWorld(ran, false, false, true), "the fade over, menu mode off: the game's own answer");
	Check(!StepTabletRunsWorld(ran, true, false, true), "a later menu's fade with no tablet before it: not the tablet's");

	bool box = false;
	StepTabletRunsWorld(box, true, true, false);
	Check(!StepTabletRunsWorld(box, true, false, false),
	      "a message box over the pages (the world pauses behind it): not the tablet's to run");
	Check(!StepTabletRunsWorld(box, true, false, true), "the box and the pages closing from there: paused as before");

	bool esc = false;
	StepTabletRunsWorld(esc, true, true, false);
	Check(!StepTabletRunsWorld(esc, true, false, false), "the Esc menu over the pages: its own answer");
	Check(!StepTabletRunsWorld(esc, true, false, true), "and its fade: paused as vanilla");

	bool back = false;
	StepTabletRunsWorld(back, true, true, false);
	StepTabletRunsWorld(back, true, false, true);
	Check(StepTabletRunsWorld(back, true, true, false), "the pages opened again in the fade: up, running");
}

void TestWalkThroughFade() {
	std::printf("The left stick walks through the close's fade\n");
	Check(TabletWalkFrame(true, false, false), "the tablet up: walks");
	Check(TabletWalkFrame(false, true, false), "its menus fading out after the close: walks on");
	Check(!TabletWalkFrame(false, false, false), "neither: no walk under a menu");
	Check(!TabletWalkFrame(true, false, true), "a site the walk needs refused: no walk, up");
	Check(!TabletWalkFrame(false, true, true), "nor in the fade");
	TabletCloseState s;
	StepTabletClose(s, true, false);
	const bool closing = StepTabletClose(s, false, true);
	Check(TabletWalkFrame(false, closing, false), "as wired: the first closing frame walks");
}

}  // namespace

int main() {
	TestOnlyHudMenus();
	TestClosing();
	TestNotClosing();
	TestPageBack();
	TestWorldRunsThroughFade();
	TestWalkThroughFade();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
