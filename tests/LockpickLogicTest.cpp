// Checks the lock's minigame under the controllers (vr/LockpickLogic.h): the
// minigame's places in the layer's pixels from its tiles, the tumbler under
// the beam, and the cursor the game is given - the beam choosing a tumbler,
// a stick's flick stepping from it, a stick's push sending the upward step,
// the trigger's click without a beam on the menu, the buttons' row left to
// the beam, and everything left alone without the lock on top.

#include <cstdio>
#include <limits>

#include "vr/LockpickLogic.h"

namespace {

using namespace obvr;
using namespace obvr::vr;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float within = 0.6f) { return a == a && a >= b - within && a <= b + within; }

const float kNan = std::numeric_limits<float>::quiet_NaN();

// The game's own lockpick_menu.xml on the harness's layer (measured
// 2026-11-06: the background at 427.1 69.5, 853 by 821, the layer 2568 wide).
LockTiles VanillaTiles() {
	LockTiles t;
	t.background = true;
	t.backgroundX = 427.1f;
	t.backgroundY = 69.5f;
	t.backgroundWidth = 853.0f;
	t.backgroundHeight = 821.0f;
	const float left[5] = {292.0f, 343.0f, 394.0f, 444.0f, 495.0f};
	for (UInt32 i = 0; i < 5; ++i) {
		t.cover[i] = true;
		t.coverX[i] = left[i];
		t.coverWidth[i] = 51.0f;
	}
	t.buttons = true;
	t.buttonsY = 740.0f;
	return t;
}

LockpickFrame Frame(bool beam = false, float x = 0.0f, float y = 0.0f) {
	LockpickFrame f;
	f.lockOnTop = true;
	f.geometry = LockGeometryFrom(VanillaTiles(), 2568.0f);
	f.beamValid = beam;
	f.beamX = x;
	f.beamY = y;
	f.pickTumbler = 3;
	f.cursorValid = true;
	f.cursorY = 722.0f;  // the engine's cursor at the screen's middle, as a fresh menu has it
	return f;
}

void TestGeometry() {
	std::printf("the minigame's places\n");
	const LockGeometry g = LockGeometryFrom(VanillaTiles(), 2568.0f);
	Check(g.valid, "the background and five tumblers: known");
	Check(Near(g.unit, 1.5042f, 0.001f), "1.504 layer pixels a menu unit (the menu centred on the screen)");
	Check(Near(g.middleX, 1284.0f), "the middle: half the layer");
	Check(Near(g.tumblerLeft[0], 1082.0f, 1.0f) && Near(g.tumblerRight[0], 1158.0f, 1.0f),
	      "the first tumbler drawn from 1082 to 1158");
	Check(Near(g.tumblerLeft[4], 1387.0f, 1.0f) && Near(g.tumblerRight[4], 1464.0f, 1.0f),
	      "the fifth from 1387 to 1464");
	Check(Near(g.buttonsTop, (69.5f + 740.0f - kLockButtonsMarginUnits) * 1.5042f, 1.0f),
	      "the buttons' row: from a margin over the buttons down");
	Check(Near(g.restY, g.buttonsTop - kLockRestMarginUnits * 1.5042f, 1.0f) && g.restY < g.buttonsTop,
	      "the cursor rests a little over the buttons' row");
	LockTiles t = VanillaTiles();
	t.buttons = false;
	Check(Near(LockGeometryFrom(t, 2568.0f).buttonsTop, (69.5f + 0.9f * 821.0f - kLockButtonsMarginUnits) * 1.5042f, 1.0f),
	      "no button found: the row taken at nine tenths of the background");
	t = VanillaTiles();
	t.cover[2] = false;
	Check(!LockGeometryFrom(t, 2568.0f).valid, "a tumbler missing: not known");
	t = VanillaTiles();
	t.background = false;
	Check(!LockGeometryFrom(t, 2568.0f).valid, "no background: not known");
	Check(!LockGeometryFrom(VanillaTiles(), 0.0f).valid, "no layer width: not known");
	t = VanillaTiles();
	t.backgroundWidth = kNan;
	Check(!LockGeometryFrom(t, 2568.0f).valid, "no number for the width: not known");
}

void TestTumblerUnderBeam() {
	std::printf("the tumbler under the beam\n");
	const LockGeometry g = LockGeometryFrom(VanillaTiles(), 2568.0f);
	Check(TumblerUnderBeam(g, 1120.0f) == 1 && TumblerUnderBeam(g, 1197.0f) == 2 && TumblerUnderBeam(g, 1273.0f) == 3 &&
	          TumblerUnderBeam(g, 1349.0f) == 4 && TumblerUnderBeam(g, 1425.0f) == 5,
	      "on each tumbler's middle: that tumbler");
	Check(TumblerUnderBeam(g, 500.0f) == 1, "left of them all: the first");
	Check(TumblerUnderBeam(g, 2400.0f) == 5, "right of them all: the last");
	Check(TumblerUnderBeam(g, 1160.0f) == 2 && TumblerUnderBeam(g, 1156.0f) == 1, "either side of an edge: its side's");
	Check(TumblerUnderBeam(g, kNan) == 0, "no number: none");
	Check(TumblerUnderBeam(LockGeometry{}, 1273.0f) == 0, "the places not known: none");
}

void TestSticks() {
	std::printf("the sticks\n");
	Check(LockStick(0.8f, 0.1f) == 0.8f && LockStick(0.1f, -0.9f) == -0.9f, "the stick further from rest counts");
	Check(LockStick(kNan, 0.5f) == 0.5f && LockStick(0.5f, kNan) == 0.5f && LockStick(kNan, kNan) == 0.0f,
	      "a stick that reads as no number is at rest");
}

void TestTrigger() {
	std::printf("the trigger without a beam on the menu\n");
	Check(LockTriggerClicks(false, 0.9f, 0.0f), "no beam on the menu, the right trigger pulled: the click");
	Check(LockTriggerClicks(false, 0.0f, 0.9f), "the left trigger: the click as well");
	Check(!LockTriggerClicks(false, 0.5f, 0.6f), "neither pulled through: none");
	Check(!LockTriggerClicks(true, 1.0f, 1.0f), "a beam on the menu: the hand mode's own click stands, none from here");
	Check(!LockTriggerClicks(false, kNan, kNan), "triggers that read as no number: none");
}

void TestButtonA() {
	std::printf("A secures as well\n");
	Check(LockAClicks(true, false), "the right A down: the click");
	Check(LockAClicks(false, true), "the left A down: the click as well");
	Check(LockAClicks(true, true), "both down: the click");
	Check(!LockAClicks(false, false), "neither down: none");
}

void TestStep() {
	std::printf("the cursor the game is given\n");
	const LockGeometry g = LockGeometryFrom(VanillaTiles(), 2568.0f);
	const float band = kPickBandUnits * g.unit;
	{
		LockpickState s;
		LockpickFrame f = Frame();
		f.lockOnTop = false;
		s.selected = 4;
		const LockpickCursor c = StepLockpick(s, f);
		Check(!c.active && s.selected == 0, "the lock's menu not on top: nothing given, nothing remembered");
	}
	{
		LockpickState s;
		LockpickFrame f = Frame();
		f.geometry = LockGeometry{};
		Check(!StepLockpick(s, f).active, "the minigame's places not known: nothing given");
	}
	{
		LockpickState s;
		LockpickFrame f = Frame();
		f.pickTumbler = 2;
		LockpickCursor c = StepLockpick(s, f);
		Check(c.active && s.selected == 2 && Near(c.x, g.middleX - band) && Near(c.y, g.restY),
		      "no beam on it: the pick's own tumbler kept, the cursor at its band's middle and the rest height");
		f.pickTumbler = 0;
		LockpickState fresh;
		c = StepLockpick(fresh, f);
		Check(fresh.selected == 3 && Near(c.x, g.middleX), "the pick unread: the middle tumbler");
	}
	{
		LockpickState s;
		LockpickCursor c = StepLockpick(s, Frame(true, 1425.0f, 500.0f));
		Check(c.active && c.beamChose && s.selected == 5 && Near(c.x, g.middleX + 2.0f * band) && Near(c.y, g.restY),
		      "the beam on the fifth tumbler: the cursor at the fifth's band, 511 pixels right of the middle");
		c = StepLockpick(s, Frame(true, 1430.0f, 620.0f));
		Check(c.active && !c.beamChose && s.selected == 5 && Near(c.y, g.restY),
		      "the beam drifting on it, up or down: the same tumbler, the cursor's height held");
		// A flick of a stick to the left steps from it, and the beam staying
		// on the fifth does not take it back.
		LockpickFrame f = Frame(true, 1430.0f, 620.0f);
		f.stickX = -0.9f;
		c = StepLockpick(s, f);
		Check(c.stepped && s.selected == 4 && Near(c.x, g.middleX + band), "a stick flicked left: one tumbler left");
		c = StepLockpick(s, f);
		Check(!c.stepped && s.selected == 4, "the stick held: no second step");
		f.stickX = -0.45f;
		StepLockpick(s, f);
		f.stickX = -0.9f;
		Check(!StepLockpick(s, f).stepped, "eased but not back at rest: still no step");
		f.stickX = 0.1f;
		StepLockpick(s, f);
		f.stickX = -0.9f;
		c = StepLockpick(s, f);
		Check(c.stepped && s.selected == 3, "back at rest and flicked again: the next step");
		f.stickX = 0.0f;
		StepLockpick(s, f);
		c = StepLockpick(s, Frame(true, 1197.0f, 500.0f));
		Check(c.beamChose && s.selected == 2, "the beam taken to another tumbler: that one");
	}
	{
		// The ends hold.
		LockpickState s;
		LockpickFrame f = Frame();
		f.pickTumbler = 1;
		StepLockpick(s, f);
		f.stickX = -1.0f;
		Check(!StepLockpick(s, f).stepped && s.selected == 1, "a flick left at the first tumbler: stays");
		f.stickX = 0.0f;
		StepLockpick(s, f);
		s.selected = 5;
		f.stickX = 1.0f;
		Check(!StepLockpick(s, f).stepped && s.selected == 5, "a flick right at the fifth: stays");
	}
	{
		// The push: a stick up sends the cursor's upward step for a few frames.
		LockpickState s;
		LockpickFrame f = Frame();
		StepLockpick(s, f);
		f.stickY = 0.9f;
		LockpickCursor c = StepLockpick(s, f);
		Check(c.pushed && Near(c.y, g.restY - kPickPushUnits * g.unit) && Near(c.x, g.middleX),
		      "a stick pushed up: the cursor a push's step higher, the tumbler kept");
		UInt32 raised = 1;
		for (UInt32 i = 0; i < 20; ++i) {
			c = StepLockpick(s, f);
			raised += c.y < g.restY - 1.0f ? 1u : 0u;
		}
		Check(raised == kPickPushFrames && Near(c.y, g.restY) && !c.pushed,
		      "held up for the push's frames, then back at rest - the stick still up pushes no second time");
		f.stickY = 0.0f;
		StepLockpick(s, f);
		f.stickY = 0.9f;
		Check(StepLockpick(s, f).pushed, "let go and pushed again: the next push");
		// The larger of the two wins: a flick mostly sideways is a step.
		LockpickState d;
		LockpickFrame both = Frame();
		StepLockpick(d, both);
		both.stickX = 0.9f;
		both.stickY = 0.7f;
		c = StepLockpick(d, both);
		Check(c.stepped && !c.pushed && d.selected == 4, "a stick up and to the side, more to the side: a step, no push");
		LockpickState u;
		both = Frame();
		StepLockpick(u, both);
		both.stickX = 0.65f;
		both.stickY = 0.95f;
		c = StepLockpick(u, both);
		Check(c.pushed && !c.stepped && u.selected == 3, "more upward: a push, no step");
		LockpickState down;
		LockpickFrame low = Frame();
		StepLockpick(down, low);
		low.stickY = -0.9f;
		c = StepLockpick(down, low);
		Check(!c.pushed && Near(c.y, g.restY), "a stick pulled down: nothing");
		low.stickY = kNan;
		low.stickX = kNan;
		c = StepLockpick(down, low);
		Check(c.active && !c.pushed && !c.stepped, "sticks that read as no number: at rest");
	}
	{
		// The cursor's height: down to its rest at once, up to it a pixel a
		// frame - an upward move is the game's push.
		LockpickState s;
		LockpickFrame f = Frame();
		LockpickCursor c = StepLockpick(s, f);
		Check(Near(c.y, g.restY) && !c.settling, "the engine's cursor above the rest height (a fresh menu): down to it at once");
		LockpickState low;
		f = Frame();
		f.cursorY = g.buttonsTop + 57.0f;  // where a click on Close left it
		c = StepLockpick(low, f);
		Check(Near(c.y, g.buttonsTop + 56.0f) && c.settling,
		      "the engine's cursor on the buttons' row: the cursor comes up a pixel a frame, settling");
		c = StepLockpick(low, f);
		Check(Near(c.y, g.buttonsTop + 55.0f) && c.settling, "the next frame: a pixel more");
		UInt32 frames = 2;
		while (frames < 400 && c.y > g.restY + 0.5f) {
			const float before = c.y;
			c = StepLockpick(low, f);
			frames += 1;
			if (before - c.y > 1.01f) {
				frames = 1000;
			}
		}
		Check(frames < 400 && Near(c.y, g.restY) && !c.settling, "never more than a pixel up a frame, and at rest in the end");
		LockpickFrame none = Frame();
		none.cursorValid = false;
		LockpickState fresh;
		Check(Near(StepLockpick(fresh, none).y, g.restY), "the engine's cursor unread: the rest height");
		// From the buttons' row back onto a tumbler: from the beam's height.
		LockpickState back;
		StepLockpick(back, Frame());
		StepLockpick(back, Frame(true, 1500.0f, g.buttonsTop + 40.0f));
		c = StepLockpick(back, Frame(true, 1425.0f, 500.0f));
		Check(c.active && Near(c.y, g.buttonsTop + 39.0f) && c.settling,
		      "the beam back from the buttons onto a tumbler: the cursor comes up from the beam's height, a pixel a frame");
		LockpickFrame push = Frame(true, 1425.0f, 500.0f);
		push.stickY = 1.0f;
		c = StepLockpick(back, push);
		Check(c.pushed && Near(c.y, g.buttonsTop + 38.0f - kPickPushUnits * g.unit),
		      "a push meanwhile: its step above where the cursor is");
	}
	{
		// The buttons' row is the beam's own.
		LockpickState s;
		StepLockpick(s, Frame(true, 1425.0f, 500.0f));
		LockpickCursor c = StepLockpick(s, Frame(true, 1500.0f, g.buttonsTop + 5.0f));
		Check(!c.active && s.selected == 5, "the beam on the buttons' row: its own pixel goes to the game, the tumbler kept");
		c = StepLockpick(s, Frame(true, 1425.0f, 500.0f));
		Check(c.active && s.selected == 5, "back on the same tumbler: chosen as before");
		LockpickFrame f = Frame(true, 1500.0f, g.buttonsTop + 5.0f);
		f.stickX = -0.9f;
		c = StepLockpick(s, f);
		Check(!c.active && s.selected == 4, "a stick's flick while the beam is on the buttons: stepped all the same");
		c = StepLockpick(s, Frame());
		Check(c.active && Near(c.x, g.middleX + band), "the beam off the menu: the cursor at the tumbler chosen");
	}
}

}  // namespace

int main() {
	TestGeometry();
	TestTumblerUnderBeam();
	TestSticks();
	TestTrigger();
	TestButtonA();
	TestStep();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
