// Checks the tablet as a scroll (vr/TabletScroll.h): both grips closed soon
// after each other with the hands together at the inventory ring - the menus
// opened, the tablet unrolled by how far the hands are pulled apart - let go
// with one hand fully open (held by the other) or short of it (rolled back,
// the menus closed), the free hand rolling it up, a close owed to menus not
// yet open, the tablet opened by the left B, Nadi's five refused tries of
// 2026-11-27, and what keeps the gesture away.

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

// The hands in the inventory ring (its default place: 17 cm ahead of the eyes,
// 33 cm below them), `apart` metres apart - the drawn palms and the
// controllers alike.
TabletScrollFrame Hands(float apart, bool leftGrip, bool rightGrip) {
	TabletScrollFrame f;
	f.allowed = true;
	f.handsEmpty = true;
	f.tracked = true;
	f.head = NiPoint3{0.0f, 0.0f, 0.0f};
	f.headForward = NiPoint3{0.0f, 0.0f, -1.0f};
	f.left = NiPoint3{-0.5f * apart, -0.33f, -0.17f};
	f.right = NiPoint3{0.5f * apart, -0.33f, -0.17f};
	f.leftController = f.left;
	f.rightController = f.right;
	f.grip[0] = leftGrip;
	f.grip[1] = rightGrip;
	f.fullMetres = 0.40f;
	f.dt = 0.05f;
	return f;
}

TabletScrollFrame InMenu(TabletScrollFrame f) {
	f.menuMode = true;
	return f;
}

// Brought together with open grips, both closed in one frame: unrolling.
void Begin(TabletScrollState& s, float apart) {
	StepTabletScroll(s, Hands(apart, false, false));
	StepTabletScroll(s, Hands(apart, true, true));
}

void TestOpen() {
	std::printf("Unrolling it open:\n");
	TabletScrollState s;
	TabletScrollResult r = StepTabletScroll(s, Hands(0.12f, false, false));
	Check(r.phase == TabletScrollPhase::Idle && !r.menuKey, "the hands together, grips open: nothing yet");
	r = StepTabletScroll(s, Hands(0.12f, true, true));
	Check(r.phase == TabletScrollPhase::Unrolling && r.menuKey && r.betweenHands && r.gesture && r.amount == 0.0f,
	      "both grips closed: unrolling from nothing, the menus opened at once");
	r = StepTabletScroll(s, InMenu(Hands(0.30f, true, true)));
	Check(!r.menuKey && Near(r.amount, 0.18f / 0.28f), "pulled apart: unrolled by the pull, to the tablet's width; no second key");
	r = StepTabletScroll(s, InMenu(Hands(0.45f, true, true)));
	Check(r.amount == 1.0f, "past the tablet's width: fully open");
	r = StepTabletScroll(s, InMenu(Hands(0.45f, false, true)));
	Check(r.phase == TabletScrollPhase::Held && r.holderRight && !r.betweenHands && !r.menuKey,
	      "the left let go fully open: held by the right hand, the menus kept");

	// Rolled up by the free hand, at the free end.
	TabletScrollFrame f = InMenu(Hands(0.30f, true, true));
	f.freeEndValid = true;
	f.freeEnd = NiPoint3{f.left.x + 0.30f, f.left.y, f.left.z};  // over the page, not the end
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Held, "the free hand's grip over the page: no rolling up (the item there is the inventory's)");
	StepTabletScroll(s, InMenu(Hands(0.30f, false, true)));
	f = InMenu(Hands(0.30f, true, true));
	f.freeEndValid = true;
	f.freeEnd = NiPoint3{f.left.x + 0.05f, f.left.y, f.left.z};
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::RollingUp && r.betweenHands && Near(r.amount, 0.18f / 0.28f),
	      "the free hand's grip at the free end: rolling up, from where the scroll began");
	r = StepTabletScroll(s, InMenu(Hands(0.30f, false, true)));
	Check(r.phase == TabletScrollPhase::Held && r.amount == 1.0f, "let go: held again, open");
	f = InMenu(Hands(0.30f, true, true));
	f.freeEndValid = true;
	f.freeEnd = f.left;
	StepTabletScroll(s, f);
	r = StepTabletScroll(s, InMenu(Hands(0.16f, true, true)));
	Check(r.phase == TabletScrollPhase::RollingUp && !r.menuKey, "the hands coming together: still rolling up");
	r = StepTabletScroll(s, InMenu(Hands(0.14f, true, true)));
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "back where it began: rolled up, the menus closed");
	r = StepTabletScroll(s, Hands(0.14f, false, false));
	Check(r.phase == TabletScrollPhase::Idle, "both let go, the menus gone: idle");

	TabletScrollState held;
	held.phase = TabletScrollPhase::Held;
	TabletScrollFrame away = InMenu(Hands(1.5f, true, true));
	r = StepTabletScroll(held, away);
	Check(r.phase == TabletScrollPhase::Held, "no free end known: no rolling up");
}

void TestShort() {
	std::printf("Let go short of open, or brought back:\n");
	TabletScrollState s;
	Begin(s, 0.12f);
	StepTabletScroll(s, InMenu(Hands(0.25f, true, true)));
	TabletScrollResult r = StepTabletScroll(s, InMenu(Hands(0.25f, true, false)));
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "one hand let go half open: rolled back, closed");
	r = StepTabletScroll(s, Hands(0.25f, true, false));
	Check(r.phase == TabletScrollPhase::AwaitRelease, "a grip still closed: waits for the release");

	TabletScrollState both;
	Begin(both, 0.12f);
	StepTabletScroll(both, InMenu(Hands(0.45f, true, true)));
	r = StepTabletScroll(both, InMenu(Hands(0.45f, false, false)));
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "both let go at once, even fully open: closed");

	// The menus not yet up when it rolls back: the close waits for them.
	TabletScrollState early;
	Begin(early, 0.12f);
	r = StepTabletScroll(early, Hands(0.12f, true, false));
	Check(r.phase == TabletScrollPhase::AwaitRelease && !r.menuKey && early.closeOwed >= 0.0f,
	      "let go before the menus came: no key yet, a close owed");
	r = StepTabletScroll(early, InMenu(Hands(0.12f, false, false)));
	Check(r.menuKey && early.closeOwed < 0.0f, "the menus up: the owed close sent");
	TabletScrollState lost;
	Begin(lost, 0.12f);
	StepTabletScroll(lost, Hands(0.12f, false, false));
	for (int i = 0; i < 25; ++i) {
		StepTabletScroll(lost, Hands(0.12f, false, false));
	}
	Check(lost.closeOwed < 0.0f, "menus that never come: the owed close dropped after a second");

	// Pulled, then brought back together while both grips stay closed.
	TabletScrollState back;
	Begin(back, 0.12f);
	StepTabletScroll(back, InMenu(Hands(0.30f, true, true)));
	r = StepTabletScroll(back, InMenu(Hands(0.16f, true, true)));
	Check(r.phase == TabletScrollPhase::Unrolling, "brought part of the way back: still unrolling");
	r = StepTabletScroll(back, InMenu(Hands(0.14f, true, true)));
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "back where it began: rolled back, closed");
	// Not yet pulled: a hand wobbling at the start closes nothing.
	TabletScrollState wobble;
	Begin(wobble, 0.12f);
	StepTabletScroll(wobble, Hands(0.16f, true, true));
	r = StepTabletScroll(wobble, Hands(0.08f, true, true));
	Check(r.phase == TabletScrollPhase::Unrolling && r.amount == 0.0f, "a wobble before the pull: still unrolling, nothing shown");
}

void TestRefused() {
	std::printf("What keeps the gesture away:\n");
	TabletScrollState s;
	TabletScrollFrame f = Hands(0.12f, true, true);
	f.handsEmpty = false;
	TabletScrollResult r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Idle && !r.menuKey && r.refused == TabletScrollRefusal::HandsHold,
	      "a weapon or anything in a hand: refused");
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.allowed = false;
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Idle && r.refused == TabletScrollRefusal::NotAllowed,
	      "not allowed (the tablet off, a menu of OBVR's): refused");
	s = TabletScrollState{};
	r = StepTabletScroll(s, Hands(0.30f, true, true));
	Check(r.phase == TabletScrollPhase::Idle && r.refused == TabletScrollRefusal::Apart && Near(r.refusedBy, 0.30f),
	      "the controllers 30 cm apart: refused, by their distance");
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.leftController.x = -0.10f;  // the palms together, the controllers 21 cm apart
	f.rightController.x = 0.11f;
	r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::Apart && Near(r.refusedBy, 0.21f), "measured on the controllers, not the palms");

	// Where the hands are: the ring and its margin.
	const float heights[3] = {0.25f, -0.80f, -0.33f};
	const float aheads[3] = {-0.17f, -0.17f, -0.70f};
	const char* where[3] = {"above the eyes: out of the ring", "together at the hip: out of the ring",
	                        "together half a metre further out: out of the ring"};
	for (int i = 0; i < 3; ++i) {
		s = TabletScrollState{};
		f = Hands(0.12f, true, true);
		f.left.y = f.right.y = f.leftController.y = f.rightController.y = heights[i];
		f.left.z = f.right.z = f.leftController.z = f.rightController.z = aheads[i];
		r = StepTabletScroll(s, f);
		Check(r.phase == TabletScrollPhase::Idle && !r.menuKey && r.refused == TabletScrollRefusal::OutOfRing, where[i]);
	}
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.left.y = f.right.y = 0.25f;
	r = StepTabletScroll(s, f);
	Check(Near(r.refusedBy, 0.32f), "  by how far the hands' middle is past the reach and its 10 cm");
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.left.y = f.right.y = -0.33f + 0.25f;  // 25 cm over the ring's centre: past 16 cm, within 26
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Unrolling, "the hands' middle just past the ring's reach, within the margin: begins");
	s = TabletScrollState{};
	f = Hands(0.24f, true, true);
	f.left.x = -0.30f;  // the left palm 30 cm aside, out of the ring; the middle still at it
	f.right.x = 0.0f;
	f.leftController.x = -0.08f;
	f.rightController.x = 0.08f;
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Unrolling, "one palm out of the ring, the hands' middle at it: begins");

	// When the grips close.
	TabletScrollState late;
	StepTabletScroll(late, Hands(0.12f, true, false));
	for (int i = 0; i < 20; ++i) {
		StepTabletScroll(late, Hands(0.12f, true, false));
	}
	r = StepTabletScroll(late, Hands(0.12f, true, true));
	Check(r.phase == TabletScrollPhase::Idle && !r.menuKey && r.refused == TabletScrollRefusal::Late && Near(r.refusedBy, 1.05f),
	      "one grip held a second, then the other: refused - the second came 1.05 s late");
	TabletScrollState soon;
	StepTabletScroll(soon, Hands(0.12f, false, true));
	for (int i = 0; i < 7; ++i) {
		StepTabletScroll(soon, Hands(0.12f, false, true));
	}
	r = StepTabletScroll(soon, Hands(0.12f, true, true));
	Check(r.phase == TabletScrollPhase::Unrolling && r.menuKey, "the right first, the left 0.4 s after: begins");
	TabletScrollState again;
	for (int i = 0; i < 20; ++i) {
		StepTabletScroll(again, Hands(0.12f, true, false));
	}
	StepTabletScroll(again, Hands(0.12f, false, false));
	StepTabletScroll(again, Hands(0.12f, true, false));
	r = StepTabletScroll(again, Hands(0.12f, true, true));
	Check(r.phase == TabletScrollPhase::Unrolling, "a grip opened and closed again: its time starts anew");
	TabletScrollState carried;
	StepTabletScroll(carried, Hands(0.60f, true, true));
	StepTabletScroll(carried, Hands(0.30f, true, true));
	r = StepTabletScroll(carried, Hands(0.12f, true, true));
	Check(r.phase == TabletScrollPhase::Idle && !r.menuKey && r.refused == TabletScrollRefusal::None,
	      "grips closed elsewhere, brought to the ring closed: nothing opens, nothing said");

	TabletScrollState one;
	r = StepTabletScroll(one, Hands(0.12f, true, false));
	Check(r.phase == TabletScrollPhase::Idle && !r.menuKey, "one grip only: nothing opens");
	TabletScrollState untracked;
	f = Hands(0.12f, true, true);
	f.tracked = false;
	r = StepTabletScroll(untracked, f);
	Check(r.phase == TabletScrollPhase::Idle && !r.menuKey, "the hands lost: nothing opens");
	TabletScrollState lostMid;
	Begin(lostMid, 0.12f);
	f = InMenu(Hands(0.30f, true, true));
	f.tracked = false;
	r = StepTabletScroll(lostMid, f);
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "the hands lost while unrolling: closed");
	TabletScrollState offMid;
	Begin(offMid, 0.12f);
	f = InMenu(Hands(0.30f, true, true));
	f.allowed = false;
	r = StepTabletScroll(offMid, f);
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "no longer allowed while unrolling: closed");
	TabletScrollState menu;
	r = StepTabletScroll(menu, InMenu(Hands(0.12f, true, true)));
	Check(menu.phase == TabletScrollPhase::Idle && !r.menuKey, "another menu up: nothing opens");
}

// Both grips closed with the hands together and the scroll does not begin:
// it says why, with the measure (the headset run of 2026-11-20: five tries,
// nothing, and nothing in the log to say what was missing).
void TestSaysWhy() {
	std::printf("Why a try did not begin it:\n");
	TabletScrollState s;
	TabletScrollFrame f = Hands(0.30f, true, true);
	TabletScrollResult r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::Apart, "refused: said");
	r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::None, "said once, on the frame the grips closed");
	TabletScrollFrame turned = Hands(0.12f, false, false);
	turned.headForward = NiPoint3{1.0f, 0.0f, 0.0f};  // facing +x: the ring 17 cm along +x
	turned.left = turned.leftController = NiPoint3{0.17f, -0.33f, -0.06f};
	turned.right = turned.rightController = NiPoint3{0.17f, -0.33f, 0.06f};
	float by = 0.0f;
	Check(ScrollPoseRefusal(turned, TabletScrollLimits{}, by) == TabletScrollRefusal::None,
	      "the head turned: the ring turns with its heading");

	// What is no try at the scroll says nothing.
	s = TabletScrollState{};
	r = StepTabletScroll(s, Hands(0.60f, true, true));
	Check(r.refused == TabletScrollRefusal::None && r.refusedBy == 0.0f,
	      "the controllers 60 cm apart: a two-handed grab, nothing said");
	s = TabletScrollState{};
	f = Hands(0.12f, true, true);
	f.tracked = false;
	r = StepTabletScroll(s, f);
	Check(r.refused == TabletScrollRefusal::None, "a hand lost: nothing said");
	s = TabletScrollState{};
	r = StepTabletScroll(s, InMenu(Hands(0.12f, true, true)));
	Check(r.refused == TabletScrollRefusal::None, "another menu up: nothing said");
	s = TabletScrollState{};
	r = StepTabletScroll(s, Hands(0.12f, true, false));
	Check(r.refused == TabletScrollRefusal::None, "one grip: nothing said");
	by = 1.0f;
	f = Hands(0.12f, false, false);
	f.tracked = false;
	Check(ScrollPoseRefusal(f, TabletScrollLimits{}, by) == TabletScrollRefusal::NotTracked && by == 0.0f,
	      "the pose asked of lost hands: not tracked");
}

void TestByButton() {
	std::printf("Opened by the left B:\n");
	TabletScrollState s;
	TabletScrollFrame f = InMenu(Hands(0.40f, false, false));
	f.handsEmpty = false;
	f.tabletUp = true;
	TabletScrollResult r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Held && !r.holderRight && !r.menuKey, "held by the left hand, no key");
	f = Hands(0.40f, false, false);
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::AwaitRelease, "closed by the B again: the scroll lets it go");
	r = StepTabletScroll(s, f);
	Check(r.phase == TabletScrollPhase::Idle, "and is idle");

	// Rolled up before any scroll has measured the hands together.
	TabletScrollState b;
	f = InMenu(Hands(0.34f, false, false));
	f.tabletUp = true;
	StepTabletScroll(b, f);
	f = InMenu(Hands(0.34f, false, true));
	f.freeEndValid = true;
	f.freeEnd = f.right;
	r = StepTabletScroll(b, f);
	Check(r.phase == TabletScrollPhase::RollingUp && Near(r.amount, 0.10f / 0.16f),
	      "opened by the B, the right takes its free end: rolling up towards the palms 0.24 m apart");
	r = StepTabletScroll(b, InMenu(Hands(0.26f, true, true)));
	Check(r.phase == TabletScrollPhase::RollingUp, "not yet there");
	r = StepTabletScroll(b, InMenu(Hands(0.25f, true, true)));
	Check(r.phase == TabletScrollPhase::AwaitRelease && r.menuKey, "the palms 0.25 m apart: rolled up, closed");

	Check(TabletUnrollShare(0.0f) == 0.0f && TabletUnrollShare(-1.0f) == 0.0f, "not up: rolled");
	Check(Near(TabletUnrollShare(0.5f * kTabletUnrollSeconds), 0.5f), "half the time: half unrolled");
	Check(TabletUnrollShare(1.0f) == 1.0f, "after the time: open");
	TabletScrollLimits l;
	Check(UnrollAmount(0.05f, 0.12f, 0.40f, l) == 0.0f && UnrollAmount(1.0f, 0.12f, 0.40f, l) == 1.0f, "the amount clamped");
	Check(Near(UnrollAmount(0.30f, 0.12f, 0.40f, l), 0.18f / 0.28f), "open when the palms stand the tablet's width apart");
	Check(Near(UnrollAmount(0.35f, 0.29f, 0.34f, l), 0.06f / 0.12f),
	      "begun nearly the tablet's width apart: open after a pull of 12 cm");
}

// Nadi's five tries of the Windows run of 2026-11-27 (OBVR.log 5815-5868):
// each the controllers where the Holster line at the same grips put them
// (right, forward, up from the eyes, metres), the drawn palms as far apart as
// the refusal said. His ring: 2 cm right, 2 cm behind the eyes' plane, 23 cm
// down, 20 cm across (his live INI). The palms' middle 10 cm back towards him
// and 8 cm under the controllers' [G - the log has the palms' distance, not
// their place]; the second grip 0.1 s after the first [G - not logged]. The
// rules before this change refused these five frames as the run did: 0.24,
// 0.06 out of the ring, 0.24, 0.29, 0.00 s.
struct LoggedTry {
	const char* what;
	NiPoint3 leftController;  // right, forward, up from the eyes
	NiPoint3 rightController;
	float palmLeftRight;      // the palms' right (body frame), left hand
	float palmRightRight;     // and right hand
	float palmsApart;         // the drawn palms' distance as logged
};

NiPoint3 FromEyes(const NiPoint3& body) { return NiPoint3{body.x, body.z, -body.y}; }

TabletScrollFrame TryFrame(const LoggedTry& t, bool leftGrip, bool rightGrip, float pull = 0.0f) {
	TabletScrollFrame f = Hands(0.0f, leftGrip, rightGrip);
	f.ring = NiPoint3{0.02f, -0.02f, -0.23f};
	f.ringRadius = 0.20f;
	f.fullMetres = 0.34f;
	f.dt = 0.1f;
	f.leftController = FromEyes(t.leftController + NiPoint3{-0.5f * pull, 0.0f, 0.0f});
	f.rightController = FromEyes(t.rightController + NiPoint3{0.5f * pull, 0.0f, 0.0f});
	const NiPoint3 mid = (t.leftController + t.rightController) * 0.5f;
	f.left = FromEyes(NiPoint3{t.palmLeftRight - 0.5f * pull, mid.y - 0.10f, mid.z - 0.08f});
	f.right = FromEyes(NiPoint3{t.palmRightRight + 0.5f * pull, mid.y - 0.10f, mid.z - 0.08f});
	return f;
}

const LoggedTry kHisTries[5] = {
	{"try 1 (refused: the palms 0.24 m apart)", {-0.04f, -0.01f, -0.23f}, {0.05f, -0.03f, -0.24f}, -0.115f, 0.125f, 0.24f},
	{"try 2 (refused: a palm 0.06 m outside the ring)", {0.04f, 0.31f, -0.24f}, {0.10f, 0.29f, -0.23f}, -0.05f, 0.13f, 0.18f},
	{"try 3 (refused: the palms 0.24 m apart)", {-0.04f, -0.07f, -0.22f}, {0.03f, -0.04f, -0.22f}, -0.125f, 0.115f, 0.24f},
	{"try 4 (refused: the palms 0.29 m apart)", {-0.03f, 0.07f, -0.37f}, {-0.02f, 0.08f, -0.38f}, -0.17f, 0.12f, 0.29f},
	{"try 5 (refused: 0.00 s with open grips)", {-0.03f, 0.01f, -0.17f}, {0.01f, 0.03f, -0.18f}, -0.09f, 0.07f, 0.16f},
};

void TestHisTries() {
	std::printf("Nadi's five tries of 2026-11-27 begin it:\n");
	for (const LoggedTry& t : kHisTries) {
		const TabletScrollFrame open = TryFrame(t, false, false);
		Check(Near(Distance(open.left, open.right), t.palmsApart, 0.005f), "  the palms as far apart as logged");
		TabletScrollState s;
		StepTabletScroll(s, open);
		StepTabletScroll(s, TryFrame(t, true, false));
		TabletScrollResult r = StepTabletScroll(s, TryFrame(t, true, true));
		Check(r.phase == TabletScrollPhase::Unrolling && r.menuKey && r.refused == TabletScrollRefusal::None, t.what);
		r = StepTabletScroll(s, InMenu(TryFrame(t, true, true, 0.20f)));
		Check(r.phase == TabletScrollPhase::Unrolling && r.amount == 1.0f, "  pulled 20 cm further apart: fully open");
	}
	// Try 2's palms as the run refused them: the right one 6 cm outside the ring.
	const TabletScrollFrame two = TryFrame(kHisTries[1], true, true);
	const float l = OutsideRing(two, two.left);
	const float r = OutsideRing(two, two.right);
	Check(Near(l > r ? l : r, 0.06f, 0.005f), "try 2's further palm 0.06 m outside the ring, as logged");
}

}  // namespace

int main() {
	TestOpen();
	TestShort();
	TestRefused();
	TestSaysWhy();
	TestByButton();
	TestHisTries();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
