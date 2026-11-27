// Checks the tablet lying on the hand that carries it (vr/HandReach.h and
// PlaceTablet's reach in vr/TabletLogic.h; docs/inventory-tablet-spec.md
// section 21): how far a drawn hand reaches out of the page's flat, the
// reading kept and eased, when the hand's model is read again, and the page
// standing that far before the palm - the hand behind it, in either hand and
// every hold.

#include <cmath>
#include <cstdio>

#include "vr/HandReach.h"

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

bool Near(float a, float b, float tolerance = 0.0005f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

bool NearPoint(const NiPoint3& a, const NiPoint3& b, float tolerance = 0.001f) {
	return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
}

NiPoint3 CrossOf(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// Across the controller's own y-z plane: the two hands are each other's mirror image there.
NiPoint3 MirrorX(const NiPoint3& v) { return NiPoint3{-v.x, v.y, v.z}; }

NiPoint3 TurnAboutY(const NiPoint3& v, float degrees) {
	const float r = degrees * 3.14159265f / 180.0f;
	const float c = std::cos(r);
	const float s = std::sin(r);
	return NiPoint3{v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

NiPoint3 Column(const TabletPlace& p, int c) {
	return NiPoint3{p.transform.m[0][c], p.transform.m[1][c], p.transform.m[2][c]};
}

bool SamePlace(const TabletPlace& a, const TabletPlace& b) {
	for (int c = 0; c < 4; ++c) {
		if (!NearPoint(Column(a, c), Column(b, c))) {
			return false;
		}
	}
	return a.valid == b.valid && Near(a.width, b.width) && Near(a.height, b.height);
}

// The game's units a metre, as [Tracker] UnitsPerMetre has them.
constexpr float kUnits = 70.0f;

// How far the drawn hand reaches out of the page's flat: its vertices
// against its palm's middle, along the face the page shows in that hand.
void TestReach() {
	std::printf("How far the hand reaches out of the page's flat:\n");
	HeldAxes held;
	held.valid = true;
	held.blade = NiPoint3{0.0f, 1.0f, 0.0f};
	held.edge = NiPoint3{1.0f, 0.0f, 0.0f};
	const NiPoint3 face = PageLikeTheSword(held, true).front;  // knuckles by blade: +z here
	Check(NearPoint(face, NiPoint3{0.0f, 0.0f, 1.0f}) &&
	          NearPoint(PageLikeTheSword(held, false).front, face * -1.0f),
	      "(a right hand's page faces the other way than a left's on the same axes: the hand's two sides)");
	const NiPoint3 palm{10.0f, 20.0f, 30.0f};
	HeldPlace unturned;  // the sword's own flat (section 21); turned: below
	unturned.turnDegrees = 0.0f;
	unturned.leanDegrees = unturned.rollDegrees = unturned.acrossCm = unturned.outCm = 0.0f;
	// A thumb's ball 4 cm before the flat, a knuckle 2 cm behind it, a finger between.
	const NiPoint3 hand[] = {palm + NiPoint3{2.0f, 1.0f, 2.8f}, palm + NiPoint3{-3.0f, 0.0f, -1.4f},
	                         palm + NiPoint3{5.0f, -2.0f, 0.7f}};
	HandReach r = ReachOfHand(hand, 3, palm, held, unturned, kUnits);
	Check(r.valid && Near(r.beforeLeft, 0.04f), "before the face a left hand shows: its foremost vertex, in metres");
	Check(Near(r.beforeRight, 0.02f), "before the face a right hand shows: its foremost the other way");

	// The axes as the bones give them: not unit, not square.
	HeldAxes rough = held;
	rough.blade = held.blade * 0.9f;
	rough.edge = held.edge * 1.2f + held.blade * 0.4f;
	r = ReachOfHand(hand, 3, palm, rough, unturned, kUnits);
	Check(r.valid && Near(r.beforeLeft, 0.04f) && Near(r.beforeRight, 0.02f), "axes read long or skew: squared as the page's are");

	// Turned in the world with the hand: the same reach.
	HeldAxes turned;
	turned.valid = true;
	turned.blade = TurnAboutY(held.blade, 40.0f);
	turned.edge = TurnAboutY(held.edge, 40.0f);
	NiPoint3 turnedHand[3];
	for (int i = 0; i < 3; ++i) {
		turnedHand[i] = palm + TurnAboutY(hand[i] - palm, 40.0f);
	}
	r = ReachOfHand(turnedHand, 3, palm, turned, unturned, kUnits);
	Check(Near(r.beforeLeft, 0.04f) && Near(r.beforeRight, 0.02f), "the hand turned in the world: the same, it is the hand's own");

	const NiPoint3 before[] = {palm + NiPoint3{1.0f, 0.0f, 1.4f}, palm + NiPoint3{0.0f, 2.0f, 0.7f}};
	r = ReachOfHand(before, 2, palm, held, unturned, kUnits);
	Check(r.valid && Near(r.beforeLeft, 0.02f) && Near(r.beforeRight, 0.0f),
	      "nothing behind the flat: none that way - the palm's middle is in it");
	const NiPoint3 behind[] = {palm + NiPoint3{1.0f, 0.0f, -2.1f}};
	r = ReachOfHand(behind, 1, palm, held, unturned, kUnits);
	Check(r.valid && Near(r.beforeLeft, 0.0f) && Near(r.beforeRight, 0.03f), "nothing before it: none this way");

	Check(!ReachOfHand(nullptr, 3, palm, held, unturned, kUnits).valid && !ReachOfHand(hand, 0, palm, held, unturned, kUnits).valid,
	      "no vertices: not read");
	Check(!ReachOfHand(hand, 3, palm, held, unturned, 0.0f).valid, "no scale: not read");
	HeldAxes none = held;
	none.valid = false;
	Check(!ReachOfHand(hand, 3, palm, none, unturned, kUnits).valid, "the hand's node not read: not read");
	HeldAxes flat = held;
	flat.edge = held.blade;
	Check(!ReachOfHand(hand, 3, palm, flat, unturned, kUnits).valid, "no flat between the axes: not read");

	// A vertex of some other part, or skinned by a bone that went: no hand is that thick.
	const float beyond = (kHandReachMostMetres + 0.01f) * kUnits;
	const NiPoint3 misread[] = {hand[0], palm + NiPoint3{0.0f, 0.0f, beyond}};
	Check(!ReachOfHand(misread, 2, palm, held, unturned, kUnits).valid, "a vertex farther out than any hand is thick: a misread, not read");
	const NiPoint3 misreadBehind[] = {hand[0], palm + NiPoint3{0.0f, 0.0f, -beyond}};
	Check(!ReachOfHand(misreadBehind, 2, palm, held, unturned, kUnits).valid, "the same behind it");
	const float thick = (kHandReachMostMetres - 0.01f) * kUnits;
	const NiPoint3 gauntlet[] = {palm + NiPoint3{0.0f, 0.0f, thick}};
	Check(ReachOfHand(gauntlet, 1, palm, held, unturned, kUnits).valid, "a thick gauntlet under that: read");

	// Turned a quarter towards the head (section 24.4, the default): the two
	// hands' faces both look away from their knuckles - here -x - and each
	// hand's reach is taken that way: the knuckle 3 units out of the palm's middle.
	HeldPlace quarter;  // the turn alone, without the defaults of 2026-11-27
	quarter.leanDegrees = quarter.rollDegrees = quarter.acrossCm = quarter.outCm = 0.0f;
	Check(NearPoint(TurnHeldPage(PageLikeTheSword(held, true), quarter, true).front, NiPoint3{-1.0f, 0.0f, 0.0f}) &&
	          NearPoint(TurnHeldPage(PageLikeTheSword(held, false), quarter, false).front, NiPoint3{-1.0f, 0.0f, 0.0f}),
	      "(turned a quarter, a left and a right hand's page face the same way: away from the knuckles)");
	r = ReachOfHand(hand, 3, palm, held, quarter, kUnits);
	Check(r.valid && Near(r.beforeLeft, 3.0f / kUnits) && Near(r.beforeRight, 3.0f / kUnits),
	      "turned: each hand's reach along the face it now shows - the knuckle's, for either");
	HandForemost q = ForemostOfHand(hand, 3, palm, held, quarter, true, kUnits);
	Check(q.valid && Near(q.out, 3.0f / kUnits), "and the foremost point that way");

	// What the page rests on, for the log: the foremost point's place on the page.
	HandForemost f = ForemostOfHand(hand, 3, palm, held, unturned, true, kUnits);
	Check(f.valid && Near(f.out, 0.04f) && Near(f.across, 2.0f / kUnits) && Near(f.up, 1.0f / kUnits),
	      "the foremost point before a left hand's page: how far out, and where across the page and up it");
	f = ForemostOfHand(hand, 3, palm, held, unturned, false, kUnits);
	Check(f.valid && Near(f.out, 0.02f) && Near(f.across, 3.0f / kUnits) && Near(f.up, 0.0f),
	      "before a right hand's page: the other side's, its place counted the right hand's way across");
	Check(!ForemostOfHand(nullptr, 3, palm, held, unturned, true, kUnits).valid && !ForemostOfHand(hand, 0, palm, held, unturned, true, kUnits).valid &&
	          !ForemostOfHand(hand, 3, palm, none, unturned, true, kUnits).valid && !ForemostOfHand(hand, 3, palm, held, unturned, true, 0.0f).valid,
	      "no vertices, no node or no scale: none");
}

void TestKept() {
	std::printf("The reading kept from frame to frame:\n");
	HandReach shown;
	StepHandReach(shown, HandReach{});
	Check(!shown.valid, "not read: none");
	HandReach open;
	open.valid = true;
	open.beforeLeft = 0.04f;
	open.beforeRight = 0.02f;
	StepHandReach(shown, open);
	Check(shown.valid && Near(shown.beforeLeft, 0.04f) && Near(shown.beforeRight, 0.02f), "the first reading taken whole");
	HandReach fist;
	fist.valid = true;
	fist.beforeLeft = 0.06f;
	fist.beforeRight = 0.01f;
	StepHandReach(shown, fist);
	Check(Near(shown.beforeLeft, 0.04f + 0.02f * kHandReachEase) && Near(shown.beforeRight, 0.02f - 0.01f * kHandReachEase),
	      "the fingers closed: eased towards the new one, the page does not jump");
	for (int i = 0; i < 40; ++i) {
		StepHandReach(shown, fist);
	}
	Check(Near(shown.beforeLeft, 0.06f) && Near(shown.beforeRight, 0.01f), "and there after a few frames");
	StepHandReach(shown, HandReach{});
	Check(!shown.valid, "no longer read: none");
}

// When the hand's model is read again: it costs a tenth of a millisecond, so
// not every frame.
void TestClock() {
	std::printf("When the hand's model is read:\n");
	HandReachClock clock;
	float curls[kHandReachCurls] = {};
	bool read = false;
	for (int i = 0; i < 40; ++i) {
		read = read || StepHandReachClock(clock, false, curls);
	}
	Check(!read, "the tablet not up: never");
	Check(!StepHandReachClock(clock, true, curls),
	      "the frame the tablet comes up: not yet - the menu's opening frame is the heavy one");
	Check(StepHandReachClock(clock, true, curls), "the frame after: read");
	int quiet = 0;
	while (quiet < 100 && !StepHandReachClock(clock, true, curls)) {
		++quiet;
	}
	Check(quiet == static_cast<int>(kHandReachEveryFramesTold) - 1 && kHandReachEveryFramesTold > kHandReachEveryFrames,
	      "then seldom while the told fingers are still - only for what the hand wears changing");

	// Still frames since the last read: a finger's move is read at the
	// earliest kHandReachLeastFrames after it.
	const auto rest = [&]() {
		bool any = false;
		for (UInt32 i = 0; i + 1 < kHandReachLeastFrames; ++i) {
			any = StepHandReachClock(clock, true, curls) || any;
		}
		return any;
	};
	Check(!rest(), "(frames with nothing moved)");
	curls[2] += kHandReachCurlStep + 0.01f;
	Check(StepHandReachClock(clock, true, curls), "a finger of the holding hand moved: read at once, not at the next turn");
	Check(!StepHandReachClock(clock, true, curls), "and not again while it rests there");
	// Fingers moving fast: a step every frame.
	int reads = 0;
	const int frames = 4 * static_cast<int>(kHandReachLeastFrames);
	for (int frame = 0; frame < frames; ++frame) {
		curls[1] += kHandReachCurlStep + 0.01f;
		reads += StepHandReachClock(clock, true, curls) ? 1 : 0;
	}
	Check(reads == 4 && kHandReachLeastFrames >= 2,
	      "a finger moving a step every frame: a read every so many frames, never two frames running");
	for (UInt32 i = 0; i < 2 * kHandReachLeastFrames; ++i) {
		StepHandReachClock(clock, true, curls);
	}
	bool early = false;
	curls[4] += 0.4f * kHandReachCurlStep;
	early = early || StepHandReachClock(clock, true, curls);
	curls[4] += 0.4f * kHandReachCurlStep;
	early = early || StepHandReachClock(clock, true, curls);
	Check(!early, "a finger moving slowly: not for each small step");
	curls[4] += 0.4f * kHandReachCurlStep;
	Check(StepHandReachClock(clock, true, curls), "but once it has come a step from the last read");
	rest();
	curls[6] -= kHandReachCurlStep + 0.01f;
	Check(StepHandReachClock(clock, true, curls), "a joint of the thumb opened: read");

	Check(!StepHandReachClock(clock, false, curls), "put down: none");
	Check(!StepHandReachClock(clock, true, curls) && StepHandReachClock(clock, true, curls),
	      "up again: read anew on its second frame, whatever the fingers did");
	HandReachClock blind;
	StepHandReachClock(blind, true, nullptr);
	Check(StepHandReachClock(blind, true, nullptr), "no curls told (the animation's fingers): read by the frames alone");
	quiet = 0;
	while (quiet < 100 && !StepHandReachClock(blind, true, nullptr)) {
		++quiet;
	}
	Check(quiet == static_cast<int>(kHandReachEveryFrames) - 1, "every so many frames, as the hands' contact fit");
}

UiRect Rect(float left, float top, float right, float bottom) {
	UiRect r;
	r.left = left;
	r.top = top;
	r.right = right;
	r.bottom = bottom;
	r.valid = true;
	return r;
}

// Nadi, 2026-11-25: "das tablet auf die tragende hand schieben sodass die
// hand nicht durchklippt, die hand soll also hinter dem tablet sein und nicht
// andersherum wie es jetzt ist". The page lay in the flat through the palm's
// middle, the fingers before it.
void TestOnTheHand() {
	std::printf("The page on the hand that carries it:\n");
	TabletSettings s;
	s.enabled = true;
	s.widthMetres = 0.40f;
	s.gripShare = 0.25f;
	s.held.turnDegrees = 0.0f;  // the sword's own flat; turned: tablet_test's TestHeldTurnedAndCentred
	s.held.leanDegrees = s.held.rollDegrees = s.held.acrossCm = s.held.outCm = 0.0f;
	TabletFrame f;
	f.up = true;
	f.paper = Rect(100.0f, 200.0f, 900.0f, 1000.0f);  // 800 x 800 pixels: 0.0005 m a pixel
	f.crop = f.paper;
	HeldAxes left;
	left.valid = true;
	left.blade = TurnAboutY(NiPoint3{0.0f, 0.6f, -0.8f}, 15.0f);
	left.edge = TurnAboutY(NiPoint3{0.0f, -0.8f, -0.6f}, 15.0f);
	const NiPoint3 palm{-0.04f, 0.01f, 0.13f};
	const NiPoint3 face = CrossOf(left.edge, left.blade);  // out of the left palm
	HandReach reach;
	reach.valid = true;
	reach.beforeLeft = 0.031f;   // the fingers and the thumb's ball before the palm's middle
	reach.beforeRight = 0.012f;  // the back of the hand behind it

	const float lift = 0.031f + kPageClearMetres;
	Check(Near(PageLiftMetres(reach, true), lift), "the page a hair before the hand's foremost part");
	TabletPlace p = PlaceTablet(f, s, true, palm, TabletHold::Side, left, reach);
	Check(p.valid && NearPoint(Column(p, 3), palm + face * lift),
	      "its middle on the palm's (section 24.4), moved out of the palm along its face - the hand behind it");
	Check(NearPoint(Column(p, 0), left.edge) && NearPoint(Column(p, 1), left.blade) && NearPoint(Column(p, 2), face),
	      "unturned: like the sword");
	Check(Near(p.width, 0.40f) && Near(p.height, 0.40f), "its size as before");
	p = PlaceTablet(f, s, true, palm, TabletHold::Bottom, left, reach);
	Check(NearPoint(Column(p, 3), palm + face * lift), "the scroll's bottom hold: the same, out of the palm");
	p = PlaceTablet(f, s, true, palm, TabletHold::Top, left, reach);
	Check(NearPoint(Column(p, 3), palm + face * lift), "and its top hold");

	// The right hand: the left's mirror image, its own side read.
	HeldAxes right;
	right.valid = true;
	right.blade = MirrorX(left.blade);
	right.edge = MirrorX(left.edge);
	HandReach mirrored;
	mirrored.valid = true;
	mirrored.beforeRight = reach.beforeLeft;
	mirrored.beforeLeft = reach.beforeRight;
	const TabletHold holds[3] = {TabletHold::Side, TabletHold::Bottom, TabletHold::Top};
	bool same = true;
	for (const TabletHold hold : holds) {
		const TabletPlace l = PlaceTablet(f, s, true, palm, hold, left, reach);
		const TabletPlace r = PlaceTablet(f, s, false, MirrorX(palm), hold, right, mirrored);
		same = same && r.valid && NearPoint(Column(r, 3), MirrorX(Column(l, 3))) &&
		       NearPoint(Column(r, 2), MirrorX(Column(l, 2)));
	}
	Check(same, "in the right hand, every hold: the left's mirror image, before its own palm");
	Check(Near(PageLiftMetres(mirrored, false), lift) && Near(PageLiftMetres(mirrored, true), 0.012f + kPageClearMetres),
	      "each physical hand its own side of the flat");

	HandReach board;
	board.valid = true;
	Check(Near(PageLiftMetres(board, true), kPageClearMetres) && Near(PageLiftMetres(board, false), kPageClearMetres),
	      "a hand reaching nowhere out of the flat: the hair alone");
	// The hand's model not read: where an open hand's fingers end, not in the palm.
	const float unread = kHandReachUnreadMetres + kPageClearMetres;
	Check(Near(PageLiftMetres(HandReach{}, true), unread) && Near(PageLiftMetres(HandReach{}, false), unread),
	      "the hand's model not read: an open hand's reach stands in");
	p = PlaceTablet(f, s, true, palm, TabletHold::Side, left);
	Check(NearPoint(Column(p, 3), palm + face * unread), "and the page stands that far before the palm");

	// The node not read: the clipboard on the controller, as it was.
	HeldAxes none = left;
	none.valid = false;
	Check(SamePlace(PlaceTablet(f, s, true, palm, TabletHold::Side, none, reach),
	                PlaceTablet(f, s, true, palm, TabletHold::Side, none)),
	      "the hand's node not read: on the controller as before, whatever the reach");

	TabletFrame both = f;
	both.reach[0] = reach;
	both.reach[1] = mirrored;
	Check(Near(TabletHandReach(both, false).beforeLeft, 0.031f) && Near(TabletHandReach(both, true).beforeRight, 0.031f),
	      "each role's own hand");
}

}  // namespace

int main() {
	TestReach();
	TestKept();
	TestClock();
	TestOnTheHand();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
