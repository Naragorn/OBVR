// Checks the tablet as a scroll (vr/TabletScroll.h): the pose held with open
// grips, both grips closed - the menus opened, the tablet unrolled by the
// hands' distance - let go with one hand fully open (held by the other) or
// short of it (rolled back, the menus closed), the free hand rolling it up,
// a close owed to menus not yet open, the tablet opened by the left B, and
// what keeps the gesture away.

#include <cstdio>

#include "vr/TabletScroll.h"

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

bool Near(float a, float b, float tolerance = 0.001f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

// The hands half a metre ahead, 0.3 below the eyes, `apart` metres apart.
TabletScrollFrame Hands(float apart, bool leftGrip, bool rightGrip) {
	TabletScrollFrame f;
	f.allowed = true;
	f.handsEmpty = true;
	f.tracked = true;
	f.head = NiPoint3{0.0f, 0.0f, 0.0f};
	f.headForward = NiPoint3{0.0f, 0.0f, -1.0f};
	f.left = NiPoint3{-0.5f * apart, -0.3f, -0.5f};
	f.right = NiPoint3{0.5f * apart, -0.3f, -0.5f};
	f.grip[0] = leftGrip;
	f.grip[1] = rightGrip;
	f.fullMetres = 0.40f;
	f.dt = 0.05f;
	return f;
}

// Held still with open grips until ready.
void MakeReady(TabletScrollState& s) {
	for (int i = 0; i < 8; ++i) {
		StepTabletScroll(s, Hands(0.12f, false, false));
	}
}

void TestOpen() {
	std::printf("Unrolling it open:\n");
	TabletScrollState s;
	TabletScrollResult r = StepTabletScroll(s, Hands(0.12f, false, false));
	Check(r.phase == TabletScrollPhase::Idle, "the pose begun: not yet ready");
	MakeReady(s);
	Check(s.phase == TabletScrollPhase::Ready, "held 0.35 s with open grips: ready");
	r = StepTabletScroll(s, Hands(0.12f, true, true));
	Check(r.phase == TabletScrollPhase::Unrolling && r.menuKey && r.betweenHands && r.gesture,
	      "both grips closed: unrolling, the menus opened at once");
	Check(Near(r.amount, (0.12f - 0.08f) / 0.32f), "unrolled by the hands' distance");
	TabletScrollFrame f = Hands(0.30f, true, true);
	f.menuMode = true;
	r = StepTabletScroll(s, f);
	Check(!r.menuKey && Near(r.amount, 0.22f / 0.32f), "pulled apart: further, no second key");
	f = Hands(0.45f, true, true);
	f.menuMode = true;
	r = StepTabletScroll(s, f);
	Check(r.amount == 1.0f, "past the tablet's width: fully open");
	f = Hands(0.45f, false, true);
	f.menuMode = true;
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Held && r.holderRight && !r.betweenHands && !r.menuKey,
	      "the left let go fully open: held by the right hand, the menus kept");

	// Rolled up by the free hand, at the free end.
	f = Hands(0.30f, true, true);
	f.menuMode = true;
	f.freeEndValid = true;
	f.freeEnd = NiPoint3{f.left.x + 0.30f, f.left.y, f.left.z};  // over the page, not the end
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Held, "the free hand's grip over the page: no rolling up (the item there is the inventory's)");
	f = Hands(0.30f, false, true);
	f.menuMode = true;
	StepTabletScroll(s, f);
	f = Hands(0.30f, true, true);
	f.menuMode = true;
	f.freeEndValid = true;
	f.freeEnd = NiPoint3{f.left.x + 0.05f, f.left.y, f.left.z};
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::RollingUp && r.betweenHands, "the free hand's grip at the free end: rolling up");
	f = Hands(0.30f, false, true);
	f.menuMode = true;
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Held && r.amount == 1.0f, "let go: held again, open");
	f = Hands(0.30f, true, true);
	f.menuMode = true;
	f.freeEndValid = true;
	f.freeEnd = f.left;
	StepTabletScroll(s, f);
	f = Hands(0.07f, true, true);
	f.menuMode = true;
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "the hands together: rolled up, the menus closed");
	r = StepTabletScroll(s, Hands(0.07f, false, false));
	Check(r.phase == TabletScrollPhase::Idle, "both let go, the menus gone: idle");
	TabletScrollFrame far = Hands(0.30f, true, true);
	far.menuMode = true;
	TabletScrollState held;
	held.phase = TabletScrollPhase::Held;
	far.right = NiPoint3{5.0f, 0.0f, 0.0f};
	StepTabletScroll(held, Hands(0.30f, false, false));
	held.phase = TabletScrollPhase::Held;
	held.lastGrip[1] = false;
	TabletScrollFrame away = Hands(1.5f, true, true);
	away.menuMode = true;
	r = StepTabletScroll(held, away);
	Check(r.phase == TabletScrollPhase::Held, "no free end known: no rolling up");
}

void TestShort() {
	std::printf("Let go short of open:\n");
	TabletScrollState s;
	MakeReady(s);
	StepTabletScroll(s, Hands(0.12f, true, true));
	TabletScrollFrame f = Hands(0.25f, true, true);
	f.menuMode = true;
	StepTabletScroll(s, f);
	f = Hands(0.25f, true, false);
	f.menuMode = true;
	TabletScrollResult r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "one hand let go half open: rolled back, closed");
	r = StepTabletScroll(s, Hands(0.25f, true, false));
	Check(r.phase == TabletScrollPhase::AwaitRelease, "a grip still closed: waits for the release");

	// Both let go at once.
	TabletScrollState both;
	MakeReady(both);
	StepTabletScroll(both, Hands(0.12f, true, true));
	f = Hands(0.45f, true, true);
	f.menuMode = true;
	StepTabletScroll(both, f);
	f = Hands(0.45f, false, false);
	f.menuMode = true;
	r = StepTabletScroll(both, f);
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "both let go at once, even fully open: closed");

	// The menus not yet up when it rolls back: the close waits for them.
	TabletScrollState early;
	MakeReady(early);
	StepTabletScroll(early, Hands(0.12f, true, true));
	r = StepTabletScroll(early, Hands(0.12f, true, false));
	Check(r.phase == TabletScrollPhase::AwaitRelease && !r.menuKey && early.closeOwed >= 0.0f,
	      "let go before the menus came: no key yet, a close owed");
	f = Hands(0.12f, false, false);
	f.menuMode = true;
	r = StepTabletScroll(early, f);
	Check(r.menuKey && early.closeOwed < 0.0f, "the menus up: the owed close sent");
	TabletScrollState lost;
	MakeReady(lost);
	StepTabletScroll(lost, Hands(0.12f, true, true));
	StepTabletScroll(lost, Hands(0.12f, false, false));
	for (int i = 0; i < 25; ++i) {
		StepTabletScroll(lost, Hands(0.12f, false, false));
	}
	Check(lost.closeOwed < 0.0f, "menus that never come: the owed close dropped after a second");

	// Pressed back together while still unrolling.
	TabletScrollState back;
	MakeReady(back);
	StepTabletScroll(back, Hands(0.12f, true, true));
	r = StepTabletScroll(back, Hands(0.06f, true, true));
	Check(r.phase == TabletScrollPhase::AwaitRelease, "squeezed back together while unrolling: closed");
}

void TestRefused() {
	std::printf("What keeps the gesture away:\n");
	TabletScrollState s;
	TabletScrollFrame f = Hands(0.12f, false, false);
	f.handsEmpty = false;
	for (int i = 0; i < 10; ++i) {
		StepTabletScroll(s, f);
	}
	Check(s.phase == TabletScrollPhase::Idle, "a weapon or anything in a hand: never ready");
	f = Hands(0.12f, false, false);
	f.allowed = false;
	for (int i = 0; i < 10; ++i) {
		StepTabletScroll(s, f);
	}
	Check(s.phase == TabletScrollPhase::Idle, "not allowed (the tablet off, a menu of OBVR's): never ready");
	TabletScrollState wide;
	for (int i = 0; i < 10; ++i) {
		StepTabletScroll(wide, Hands(0.40f, false, false));
	}
	Check(wide.phase == TabletScrollPhase::Idle, "the hands too far apart: no pose");
	TabletScrollState high;
	f = Hands(0.12f, false, false);
	f.left.y = 0.3f;
	f.right.y = 0.3f;
	for (int i = 0; i < 10; ++i) {
		StepTabletScroll(high, f);
	}
	Check(high.phase == TabletScrollPhase::Idle, "the hands above the eyes: no pose");
	TabletScrollState gripped;
	for (int i = 0; i < 10; ++i) {
		StepTabletScroll(gripped, Hands(0.12f, true, true));
	}
	Check(gripped.phase == TabletScrollPhase::Idle, "the grips closed all along: never ready, never opens");
	TabletScrollState one;
	MakeReady(one);
	TabletScrollResult r = StepTabletScroll(one, Hands(0.12f, true, false));
	Check(r.phase != TabletScrollPhase::Unrolling && !r.menuKey, "one grip only: nothing opens");
	TabletScrollState untracked;
	MakeReady(untracked);
	f = Hands(0.12f, true, true);
	f.tracked = false;
	r = StepTabletScroll(untracked, f);
	Check(r.phase == TabletScrollPhase::Idle && !r.menuKey, "the hands lost: nothing opens");
	TabletScrollState lostMid;
	MakeReady(lostMid);
	StepTabletScroll(lostMid, Hands(0.12f, true, true));
	f = Hands(0.30f, true, true);
	f.tracked = false;
	f.menuMode = true;
	r = StepTabletScroll(lostMid, f);
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "the hands lost while unrolling: closed");
	TabletScrollState menu;
	f = Hands(0.12f, false, false);
	f.menuMode = true;
	for (int i = 0; i < 10; ++i) {
		StepTabletScroll(menu, f);
	}
	Check(menu.phase == TabletScrollPhase::Idle, "another menu up: no pose");
}

// Both grips closed with the hands together and the scroll does not begin:
// it says why, with the measure (the headset run of 2026-11-20: five tries,
// nothing, and nothing in the log to say what was missing).
void TestSaysWhy() {
	std::printf("Why a try did not begin it:\n");
	// At the chest: the hands 2 cm ahead of the eyes, 25 cm below them - where
	// three of the run's five tries were made. The pose holds there now.
	TabletScrollState chest;
	TabletScrollFrame f = Hands(0.12f, false, false);
	f.left.z = f.right.z = -0.02f;
	f.left.y = f.right.y = -0.25f;
	for (int i = 0; i < 8; ++i) {
		StepTabletScroll(chest, f);
	}
	Check(chest.phase == TabletScrollPhase::Ready, "the hands at the chest, 2 cm ahead of the eyes: the pose holds");
	f.grip[0] = f.grip[1] = true;
	TabletScrollResult r = StepTabletScroll(chest, f);
	Check(r.phase == TabletScrollPhase::Unrolling && r.menuKey && r.refused == TabletScrollRefusal::None,
	      "both grips closed there: it unrolls, nothing refused");
	TabletScrollState bowed;
	f = Hands(0.12f, false, false);
	f.left.z = f.right.z = 0.08f;  // behind the eyes' plane, the head bowed over them
	for (int i = 0; i < 8; ++i) {
		StepTabletScroll(bowed, f);
	}
	Check(bowed.phase == TabletScrollPhase::Ready, "8 cm behind the eyes' plane under a bowed head: still the pose");

	// Closed at once, without the open hands held together first.
	TabletScrollState quick;
	StepTabletScroll(quick, Hands(0.12f, false, false));
	StepTabletScroll(quick, Hands(0.12f, false, false));
	r = StepTabletScroll(quick, Hands(0.12f, true, true));
	Check(r.phase == TabletScrollPhase::Idle && !r.menuKey && r.refused == TabletScrollRefusal::NoDwell &&
	          Near(r.refusedBy, 0.10f),
	      "closed after 0.1 s: not begun - the dwell, and how long it had");
	r = StepTabletScroll(quick, Hands(0.12f, true, true));
	Check(r.refused == TabletScrollRefusal::None, "said once, on the frame the grips closed");

	// One after the other counts as a try on the second.
	TabletScrollState second;
	StepTabletScroll(second, Hands(0.12f, true, false));
	r = StepTabletScroll(second, Hands(0.12f, true, true));
	Check(r.refused == TabletScrollRefusal::NoDwell, "the second grip closing: the try");

	// Each thing that keeps it away, named.
	TabletScrollState s;
	f = Hands(0.30f, true, true);
	r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::Apart && Near(r.refusedBy, 0.30f), "30 cm apart: the hands' distance");
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.left.z = f.right.z = 0.20f;
	r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::Ahead && Near(r.refusedBy, -0.20f), "20 cm behind the eyes: not ahead");
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.left.y = f.right.y = 0.25f;
	r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::Height && Near(r.refusedBy, 0.25f), "above the eyes: the height");
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.handsEmpty = false;
	r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::HandsHold, "a weapon out: the hands are not empty");
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.allowed = false;
	r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::NotAllowed, "the tablet off: not allowed");

	// What is no try at the scroll says nothing.
	s = TabletScrollState{};
	r = StepTabletScroll(s, Hands(0.60f, true, true));
	Check(r.refused == TabletScrollRefusal::None, "the hands 60 cm apart: a two-handed grab, nothing said");
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.tracked = false;
	r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::None, "a hand lost: nothing said");
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.menuMode = true;
	r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::None, "another menu up: nothing said");
	s = TabletScrollState{};
	r = StepTabletScroll(s, Hands(0.12f, true, false));
	Check(r.refused == TabletScrollRefusal::None, "one grip: nothing said");
	float by = 1.0f;
	f = Hands(0.12f, false, false);
	f.tracked = false;
	Check(ScrollPoseRefusal(f, TabletScrollLimits{}, by) == TabletScrollRefusal::NotTracked && by == 0.0f,
	      "the pose asked of lost hands: not tracked");
}

void TestByButton() {
	std::printf("Opened by the left B:\n");
	TabletScrollState s;
	TabletScrollFrame f = Hands(0.40f, false, false);
	f.handsEmpty = false;
	f.menuMode = true;
	f.tabletUp = true;
	TabletScrollResult r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Held && !r.holderRight && !r.menuKey, "held by the left hand, no key");
	f = Hands(0.40f, false, false);
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::AwaitRelease, "closed by the B again: the scroll lets it go");
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Idle, "and is idle");

	Check(TabletUnrollShare(0.0f) == 0.0f && TabletUnrollShare(-1.0f) == 0.0f, "not up: rolled");
	Check(Near(TabletUnrollShare(0.5f * kTabletUnrollSeconds), 0.5f), "half the time: half unrolled");
	Check(TabletUnrollShare(1.0f) == 1.0f, "after the time: open");
	TabletScrollLimits l;
	Check(UnrollAmount(0.05f, l, 0.40f) == 0.0f && UnrollAmount(1.0f, l, 0.40f) == 1.0f, "the amount clamped");
	Check(Near(UnrollAmount(0.25f, l, 0.10f), (0.25f - 0.08f) / 0.17f), "a tablet narrower than the pose: full a little past it");
}

}  // namespace

int main() {
	TestOpen();
	TestShort();
	TestRefused();
	TestSaysWhy();
	TestByButton();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
