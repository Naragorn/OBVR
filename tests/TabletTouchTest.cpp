// Checks the finger on the tablet as a touch screen (vr/TabletTouch.h): the
// tap on the release where the finger went down, the scroll and its coast,
// the page turned by a sideways swipe, the hold that drags, the scroll bar,
// and every way a touch ends with nothing clicked; and the tap's tip from
// the drawn index finger (vr/FingerTip.h).

#include <cstdio>

#include "vr/FingerTip.h"
#include "vr/TabletTouch.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 0.001f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

constexpr float kDt = 1.0f / 90.0f;

// The finger at a point on the glass (metres from the centre, y down), its
// pixel 1000 a metre from 500,500.
TabletTouchInput At(float x, float y, bool touching, bool onGlass = true) {
	TabletTouchInput in;
	in.onGlass = onGlass;
	in.touching = touching;
	in.glassX = x;
	in.glassY = y;
	in.pixelX = 500.0f + x * 1000.0f;
	in.pixelY = 500.0f + y * 1000.0f;
	return in;
}

// Touch at x,y and move to tx,ty over `frames` frames, still touching.
int Drag(TabletTouchState& s, float x, float y, float tx, float ty, int frames, int* pages = nullptr,
         bool* clicked = nullptr, TabletTouchEvent* firstEvent = nullptr) {
	int wheel = 0;
	for (int i = 0; i <= frames; ++i) {
		const float share = frames > 0 ? static_cast<float>(i) / static_cast<float>(frames) : 1.0f;
		const TabletTouchVerdict v = StepTabletTouch(s, At(x + (tx - x) * share, y + (ty - y) * share, true), kDt);
		wheel += v.wheel;
		if (pages != nullptr) {
			*pages += v.pageStep;
		}
		if (clicked != nullptr && v.mouseDown) {
			*clicked = true;
		}
		if (firstEvent != nullptr && *firstEvent == TabletTouchEvent::None) {
			*firstEvent = v.event;
		}
	}
	return wheel;
}

void TestTap() {
	std::printf("A tap\n");
	TabletTouchState s;
	TabletTouchVerdict v = StepTabletTouch(s, At(0.0f, 0.0f, false), kDt);
	Check(!v.active && !v.mouseDown && v.event == TabletTouchEvent::None, "hovering over the glass: nothing");
	v = StepTabletTouch(s, At(0.0f, 0.0f, true), kDt);
	Check(!v.mouseDown && v.active && v.pinned && Near(v.pinX, 500.0f) && Near(v.pinY, 500.0f),
	      "the finger on the glass: no click yet, the cursor held where it went down");
	v = StepTabletTouch(s, At(0.005f, 0.004f, true), kDt);
	Check(!v.mouseDown && v.pinned && Near(v.pinX, 500.0f), "a wobble under the slop: still held, still no click");
	v = StepTabletTouch(s, At(0.006f, 0.004f, false), kDt);
	Check(v.mouseDown && v.event == TabletTouchEvent::Tap && v.pinned && Near(v.pinX, 500.0f) && Near(v.pinY, 500.0f),
	      "lifted: the click, where the finger went down");
	v = StepTabletTouch(s, At(0.006f, 0.004f, false), kDt);
	Check(!v.mouseDown && v.active && v.pinned, "the next frame the button comes up, still there");
	v = StepTabletTouch(s, At(0.006f, 0.004f, false), kDt);
	Check(!v.active && !v.mouseDown && s.phase == TabletTouchPhase::Idle, "and the touch is over");
}

void TestScroll() {
	std::printf("A scroll\n");
	TabletTouchState s;
	bool clicked = false;
	TabletTouchEvent first = TabletTouchEvent::None;
	int wheel = Drag(s, 0.0f, 0.0f, 0.0f, 0.05f, 30, nullptr, &clicked, &first);
	Check(s.phase == TabletTouchPhase::Scrolling && first == TabletTouchEvent::ScrollStart,
	      "moved down past the slop: a scroll");
	Check(wheel == 2 && !clicked, "5 cm down: two notches up (the earlier rows), nothing clicked");
	TabletTouchVerdict v = StepTabletTouch(s, At(0.0f, 0.05f, false), kDt);
	Check(!v.mouseDown && v.event == TabletTouchEvent::None && s.phase == TabletTouchPhase::Idle,
	      "let go when still: no click, no coast");
	int coast = v.wheel;
	for (int i = 0; i < 90; ++i) {
		coast += StepTabletTouch(s, At(0.0f, 0.0f, false, false), kDt).wheel;
	}
	Check(coast == 0, "a slow drag does not coast");

	TabletTouchState up;
	wheel = Drag(up, 0.0f, 0.03f, 0.0f, -0.03f, 30);
	Check(wheel == -3, "6 cm up: three notches down");
}

void TestCoast() {
	std::printf("A flick coasts\n");
	TabletTouchState s;
	// 6 cm in 6 frames: 0.9 m/s, well over the coast's 0.25 m/s.
	int wheel = Drag(s, 0.0f, 0.0f, 0.0f, 0.06f, 6);
	Check(wheel == 3, "the flick's own notches");
	int coast = StepTabletTouch(s, At(0.0f, 0.06f, false), kDt).wheel;
	for (int i = 0; i < 20; ++i) {
		coast += StepTabletTouch(s, At(0.0f, 0.0f, false, false), kDt).wheel;
	}
	Check(coast > 0, "let go while moving: the list coasts on the same way, the finger gone from the glass");
	int later = 0;
	for (int i = 0; i < 400; ++i) {
		later += StepTabletTouch(s, At(0.0f, 0.0f, false, false), kDt).wheel;
	}
	int after = 0;
	for (int i = 0; i < 90; ++i) {
		after += StepTabletTouch(s, At(0.0f, 0.0f, false, false), kDt).wheel;
	}
	Check(after == 0, "and comes to a stop");

	TabletTouchState t;
	Drag(t, 0.0f, 0.0f, 0.0f, 0.06f, 6);
	StepTabletTouch(t, At(0.0f, 0.06f, false), kDt);
	StepTabletTouch(t, At(0.1f, 0.1f, true), kDt);
	int stopped = 0;
	for (int i = 0; i < 10; ++i) {
		stopped += StepTabletTouch(t, At(0.1f, 0.1f, true), kDt).wheel;
	}
	Check(stopped == 0 && t.phase == TabletTouchPhase::Down, "a new touch stops a coasting list");
	(void)later;
}

void TestPage() {
	std::printf("A sideways swipe turns the page\n");
	TabletTouchState s;
	int pages = 0;
	bool clicked = false;
	TabletTouchEvent first = TabletTouchEvent::None;
	Drag(s, 0.05f, 0.0f, -0.05f, 0.0f, 20, &pages, &clicked, &first);
	Check(pages == 1 && first == TabletTouchEvent::PageTurn, "the finger moved left: the page on the right, once");
	TabletTouchVerdict v = StepTabletTouch(s, At(-0.05f, 0.0f, false), kDt);
	Check(!v.mouseDown && v.pageStep == 0 && !clicked && s.phase == TabletTouchPhase::Idle,
	      "lifted: no click, no second page");

	TabletTouchState r;
	pages = 0;
	Drag(r, -0.05f, 0.0f, 0.05f, 0.01f, 20, &pages);
	Check(pages == -1, "moved right: the page on the left");

	TabletTouchState d;
	pages = 0;
	const int wheel = Drag(d, 0.0f, 0.0f, 0.02f, 0.03f, 10, &pages);
	Check(pages == 0 && wheel != 0 && d.phase == TabletTouchPhase::Scrolling, "more down than across: a scroll");
}

void TestHold() {
	std::printf("A hold drags\n");
	TabletTouchState s;
	StepTabletTouch(s, At(0.0f, 0.0f, true), kDt);
	TabletTouchVerdict v;
	int frames = 0;
	while (frames < 100 && !v.mouseDown) {
		v = StepTabletTouch(s, At(0.002f, 0.0f, true), kDt);
		++frames;
	}
	Check(v.mouseDown && v.event == TabletTouchEvent::HoldStart && frames >= 44 && frames <= 46,
	      "held still half a second: the button goes down");
	v = StepTabletTouch(s, At(0.08f, 0.0f, true), kDt);
	Check(v.mouseDown && v.pageStep == 0 && v.wheel == 0 && !v.pinned,
	      "and stays down while the finger moves on - a slider pulled, no page, no scroll");
	v = StepTabletTouch(s, At(0.08f, 0.0f, false), kDt);
	Check(!v.mouseDown && v.event == TabletTouchEvent::None && s.phase == TabletTouchPhase::Idle,
	      "the finger leaves: the button comes up, no click after it");

	TabletTouchState noTime;
	StepTabletTouch(noTime, At(0.0f, 0.0f, true), 0.0f);
	for (int i = 0; i < 100; ++i) {
		v = StepTabletTouch(noTime, At(0.0f, 0.0f, true), 0.0f);
	}
	Check(!v.mouseDown && noTime.phase == TabletTouchPhase::Down, "no time passing: no hold");

	TabletTouchState bar;
	TabletTouchInput on = At(0.0f, 0.0f, true);
	on.onScrollBar = true;
	v = StepTabletTouch(bar, on, kDt);
	Check(v.mouseDown && v.event == TabletTouchEvent::HoldStart, "put down on a scroll bar's knob: held at once");
	TabletTouchState late;
	StepTabletTouch(late, At(0.0f, 0.0f, true), kDt);
	v = StepTabletTouch(late, on, kDt);
	Check(v.mouseDown && late.phase == TabletTouchPhase::Dragging, "the knob found under the touch after it began");
}

void TestCancel() {
	std::printf("A touch ends with nothing clicked\n");
	TabletTouchState s;
	StepTabletTouch(s, At(0.0f, 0.0f, true), kDt);
	TabletTouchVerdict v = StepTabletTouch(s, At(0.0f, 0.0f, false, false), kDt);
	Check(!v.mouseDown && v.event == TabletTouchEvent::Cancelled && s.phase == TabletTouchPhase::Idle,
	      "the finger gone from the face (off its edge, through it, curled, lost): no click");
	v = StepTabletTouch(s, At(0.0f, 0.0f, false), kDt);
	Check(!v.mouseDown && !v.active, "and none when it comes back over the glass");

	TabletTouchState drag;
	TabletTouchInput on = At(0.0f, 0.0f, true);
	on.onScrollBar = true;
	StepTabletTouch(drag, on, kDt);
	v = StepTabletTouch(drag, At(0.0f, 0.0f, true, false), kDt);
	Check(!v.mouseDown && v.event == TabletTouchEvent::Cancelled, "a drag cut off: the button comes up");

	TabletTouchState scroll;
	Drag(scroll, 0.0f, 0.0f, 0.0f, 0.06f, 6);
	v = StepTabletTouch(scroll, At(0.0f, 0.06f, true, false), kDt);
	int coast = v.wheel;
	for (int i = 0; i < 30; ++i) {
		coast += StepTabletTouch(scroll, At(0.0f, 0.0f, false, false), kDt).wheel;
	}
	Check(v.event == TabletTouchEvent::Cancelled && coast == 0, "a scroll cut off: it stops, no coast");
}

void TestMovedNoClick() {
	std::printf("A move that is neither a scroll nor a page\n");
	TabletTouchState s;
	// 3 cm sideways: past the slop, short of a page's swipe.
	int pages = 0;
	bool clicked = false;
	Drag(s, 0.0f, 0.0f, 0.03f, 0.0f, 10, &pages, &clicked);
	Check(pages == 0 && !clicked && s.phase == TabletTouchPhase::Down, "3 cm sideways: no page yet, no click");
	TabletTouchVerdict v = StepTabletTouch(s, At(0.03f, 0.0f, false), kDt);
	Check(!v.mouseDown && v.event == TabletTouchEvent::Moved && s.phase == TabletTouchPhase::Idle,
	      "let go there: nothing clicked - the finger's way into the glass is no tap elsewhere");
	Check(Near(v.travel, 0.03f), "and the report says how far it moved");

	// Out past the slop and back to the start, then at rest: no hold either.
	TabletTouchState back;
	Drag(back, 0.0f, 0.0f, 0.025f, 0.0f, 5);
	Drag(back, 0.025f, 0.0f, 0.0f, 0.0f, 5);
	for (int i = 0; i < 60; ++i) {
		v = StepTabletTouch(back, At(0.0f, 0.0f, true), kDt);
	}
	Check(!v.mouseDown && back.phase == TabletTouchPhase::Down, "moved away and back, then still: no hold");
	v = StepTabletTouch(back, At(0.0f, 0.0f, false), kDt);
	Check(!v.mouseDown && v.event == TabletTouchEvent::Moved, "and no tap");

	// Under the slop the whole time: the tap, with its measures.
	TabletTouchState tap;
	StepTabletTouch(tap, At(0.0f, 0.0f, true), kDt);
	for (int i = 0; i < 9; ++i) {
		StepTabletTouch(tap, At(0.015f, 0.01f, true), kDt);
	}
	v = StepTabletTouch(tap, At(0.015f, 0.01f, false), kDt);
	Check(v.event == TabletTouchEvent::Tap && Near(v.travel, 0.015f) && Near(v.seconds, 0.1f, 0.012f),
	      "1.5 cm of wobble in a tenth of a second: a tap, and the report has both");
}

// The contact: where a touch begins and ends on a glass that gives no
// resistance (the headset run of 2026-11-20: nine touches, five cut off, no
// tap - the old rule ended a touch only 4 cm in front of the glass and cut it
// off 6 cm through it).
void TestContact() {
	std::printf("The contact with the glass\n");
	const TabletContactLimits l;
	TabletContactState s;
	TabletContact c = StepTabletContact(s, true, true, 0.08f, false, l);
	Check(!c.touching && c.onGlass && c.end == TabletContactEnd::None, "the tip 8 cm in front: over the glass, no touch");
	c = StepTabletContact(s, true, true, 0.01f, false, l);
	Check(c.touching && c.onGlass, "a centimetre in front: the touch begins");
	c = StepTabletContact(s, true, true, 0.035f, false, l);
	Check(!c.touching && c.onGlass && c.end == TabletContactEnd::PulledBack,
	      "2.5 cm back from there: let go - the finger over the glass still, so a tap");

	// The ordinary way out in front.
	TabletContactState front;
	StepTabletContact(front, true, true, 0.0f, false, l);
	c = StepTabletContact(front, true, true, 0.01f, false, l);
	Check(c.touching, "a centimetre back: still touching");
	c = StepTabletContact(front, true, true, 0.05f, false, l);
	Check(!c.touching && c.onGlass && c.end == TabletContactEnd::Lifted, "5 cm in front of the glass: lifted");

	// Through the glass, as a finger goes without knowing.
	TabletContactState deep;
	StepTabletContact(deep, true, true, 0.01f, false, l);
	c = StepTabletContact(deep, true, true, -0.10f, false, l);
	Check(c.touching && c.onGlass && Near(c.deepest, -0.10f), "10 cm through the glass: still the touch (6 cm cut it off)");
	c = StepTabletContact(deep, true, true, -0.09f, false, l);
	Check(c.touching, "a centimetre back: still touching");
	c = StepTabletContact(deep, true, true, -0.075f, false, l);
	Check(!c.touching && c.onGlass && c.end == TabletContactEnd::PulledBack && Near(c.deepest, -0.10f),
	      "2.5 cm back from the deepest point, still behind the glass: let go - the tap");
	c = StepTabletContact(deep, true, true, -0.07f, false, l);
	Check(!c.touching && c.end == TabletContactEnd::None, "on the way out: no new touch");
	c = StepTabletContact(deep, true, true, -0.06f, false, l);
	c = StepTabletContact(deep, true, true, -0.085f, false, l);
	Check(c.touching && c.onGlass, "in again by 2 cm without coming out in front: the next touch");
	c = StepTabletContact(deep, true, true, -0.06f, false, l);
	Check(!c.touching && c.end == TabletContactEnd::PulledBack, "and its way out");
	c = StepTabletContact(deep, true, true, 0.05f, false, l);
	c = StepTabletContact(deep, true, true, 0.0f, false, l);
	Check(c.touching, "out in front and down again: an ordinary touch");

	// While the finger scrolls or drags its depth wanders: more room.
	TabletContactState move;
	StepTabletContact(move, true, true, -0.05f, false, l);
	c = StepTabletContact(move, true, true, -0.02f, true, l);
	Check(c.touching, "scrolling, 3 cm back from the deepest point: still on the glass");
	c = StepTabletContact(move, true, true, 0.005f, true, l);
	Check(!c.touching && c.end == TabletContactEnd::PulledBack, "5 cm back: let go");

	// What cuts a touch off.
	TabletContactState behindPanel;
	StepTabletContact(behindPanel, true, true, 0.0f, false, l);
	c = StepTabletContact(behindPanel, true, true, -0.30f, false, l);
	Check(!c.touching && !c.onGlass && c.end == TabletContactEnd::TooDeep, "30 cm through: a hand behind the panel, cut off");
	c = StepTabletContact(behindPanel, true, true, -0.20f, false, l);
	c = StepTabletContact(behindPanel, true, true, -0.23f, false, l);
	Check(!c.touching, "moving about behind the panel: no touch");
	c = StepTabletContact(behindPanel, true, true, -0.03f, false, l);
	Check(!c.touching, "coming forward through the glass from behind: no touch");
	c = StepTabletContact(behindPanel, true, true, 0.03f, false, l);
	c = StepTabletContact(behindPanel, true, true, 0.01f, false, l);
	Check(c.touching, "only from the front again");

	TabletContactState edge;
	StepTabletContact(edge, true, true, 0.0f, false, l);
	c = StepTabletContact(edge, true, false, 0.0f, false, l);
	Check(!c.touching && !c.onGlass && c.end == TabletContactEnd::OffEdge, "off the face's edge: cut off");
	TabletContactState curl;
	StepTabletContact(curl, true, true, 0.0f, false, l);
	c = StepTabletContact(curl, false, true, 0.0f, false, l);
	Check(!c.touching && !c.onGlass && c.end == TabletContactEnd::FingerIn, "the index curled or the trigger pulled: cut off");
	c = StepTabletContact(curl, false, true, 0.0f, false, l);
	Check(c.end == TabletContactEnd::None, "said once");

	// Where a touch may begin.
	TabletContactState behind;
	c = StepTabletContact(behind, true, true, -0.05f, false, l);
	Check(c.touching, "a tip first seen 5 cm through the glass: a touch (the hand came in from the side)");
	TabletContactState tooFar;
	c = StepTabletContact(tooFar, true, true, -0.08f, false, l);
	Check(!c.touching && !c.onGlass, "first seen 8 cm through: none");

	// The contact feeding the touch: deep in and a little back is one tap.
	TabletTouchState touch;
	const float depths[] = {0.05f, 0.01f, -0.04f, -0.09f, -0.10f, -0.07f, -0.02f, 0.06f};
	int taps = 0;
	int cancels = 0;
	bool down = false;
	for (float depth : depths) {
		const TabletContact k = StepTabletContact(touch.contact, true, true, depth, false, l);
		const TabletTouchVerdict v = StepTabletTouch(touch, At(0.0f, 0.0f, k.touching, k.onGlass), kDt);
		taps += v.event == TabletTouchEvent::Tap ? 1 : 0;
		cancels += v.event == TabletTouchEvent::Cancelled ? 1 : 0;
		down = down || v.mouseDown;
	}
	Check(taps == 1 && cancels == 0 && down, "a poke 10 cm through the glass and out again: one tap, one click");
}

void TestIndexTip() {
	std::printf("The tap's tip from the drawn index finger\n");
	const NiPoint3 tip = IndexTipBeyond(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 2.0f, 0.0f});
	Check(Near(tip.x, 0.0f) && Near(tip.y, 3.6f) && Near(tip.z, 0.0f), "the tip past the last joint, along the finger");
	const NiPoint3 palm = PalmBetween(NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{1.0f, 0.0f, -0.08f});
	Check(Near(palm.x, 1.0f) && Near(palm.z, -0.04f), "the palm's middle: halfway from the wrist to the middle finger's base");

	IndexTip t;
	StepIndexTip(t, true, NiPoint3{0.0f, -0.02f, -0.10f});
	Check(t.valid && Near(t.local.z, -0.10f), "the first reading taken as it is");
	StepIndexTip(t, true, NiPoint3{0.0f, -0.02f, -0.20f});
	Check(t.valid && Near(t.local.z, -0.13f), "the next eased into it");
	StepIndexTip(t, false, NiPoint3{0.0f, -0.02f, -0.10f});
	Check(!t.valid, "the drawn fingers not following the controller's: no finger tip");
	StepIndexTip(t, true, NiPoint3{0.0f, 0.0f, -0.40f});
	Check(!t.valid, "40 cm from the controller: a misread, no finger tip");

	HandPose hand;
	hand.valid = true;
	hand.position = NiPoint3{1.0f, 1.0f, 1.0f};
	const NiPoint3 pointing{0.0f, 0.0f, -1.0f};
	IndexTip none;
	NiPoint3 at = TapTip(hand, pointing, 0.08f, none);
	Check(Near(at.x, 1.0f) && Near(at.y, 1.0f) && Near(at.z, 0.92f), "no finger read: the controller's tip, 8 cm on");
	IndexTip finger;
	finger.valid = true;
	finger.local = NiPoint3{0.01f, -0.02f, -0.05f};
	at = TapTip(hand, pointing, 0.08f, finger);
	Check(Near(at.x, 1.01f) && Near(at.y, 0.98f) && Near(at.z, 0.95f), "the finger read: its tip on the controller");
}

}  // namespace

int main() {
	TestTap();
	TestScroll();
	TestCoast();
	TestPage();
	TestHold();
	TestCancel();
	TestMovedNoClick();
	TestContact();
	TestIndexTip();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
