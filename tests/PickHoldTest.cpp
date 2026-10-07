// Checks the pick's hold from frame to frame and the eased anchor points
// (game/PickHold.h), and the per-hand rank of one item (game/NearbyItems.h).

#include <cstdio>

#include "game/PickHold.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 1.0e-3f) {
	const float d = a - b;
	return d <= tolerance && d >= -tolerance;
}

NearItem Item(UInt32 ref, bool left, UInt8 rankClass, float rankKey, float distance = 10.0f) {
	NearItem i;
	i.valid = true;
	i.left = left;
	i.ref = ref;
	i.centre = NiPoint3{1.0f, 2.0f, 3.0f};
	i.distance = distance;
	i.rankClass = rankClass;
	i.rankKey = rankKey;
	return i;
}

// A frame where item A (ref 0xA) is held by the right hand at `heldClass`/
// `heldKey`, and the frame's best is `best`.
NearItem Frame(PickHoldState& s, const NearItem& best, const NearItem& heldRight, const NearItem& heldLeft,
               float dt) {
	const NearItem byHand[2] = {heldRight, heldLeft};
	return StepPickHold(s, best, byHand, dt);
}

void TestClearlyBetter() {
	std::printf("Clearly better\n");
	Check(RanksClearlyBetter(0, 100.0f, 1, 0.0f), "a better class wins whatever the keys");
	Check(!RanksClearlyBetter(2, 0.0f, 1, 1.0f), "a worse class loses whatever the keys");
	Check(RanksClearlyBetter(2, 0.010f, 2, 0.050f), "the laser's miss: 2.3 degrees less is clear");
	Check(!RanksClearlyBetter(2, 0.030f, 2, 0.050f), "the laser's miss: 1.1 degrees less is not");
	Check(RanksClearlyBetter(1, 10.0f, 1, 12.0f), "by distance: 2 units (3 cm) nearer is clear");
	Check(!RanksClearlyBetter(1, 11.0f, 1, 12.0f), "by distance: 1 unit nearer is not");
	Check(RanksClearlyBetter(0, 1.0f, 0, 3.0f) && !RanksClearlyBetter(4, 30.0f, 4, 31.0f),
	      "touched and palm-found: by distance too");
	Check(!RanksClearlyBetter(3, 0.2f, 3, 0.2f), "the same is never clearer");
}

void TestTakeAndStay() {
	std::printf("Taking and staying\n");
	PickHoldState s;
	const NearItem none;
	Check(!Frame(s, none, none, none, 0.011f).valid, "nothing in reach: nothing held");
	const NearItem a = Item(0xA, false, 1, 10.0f);
	const NearItem got = Frame(s, a, none, none, 0.011f);
	Check(got.valid && got.ref == 0xA && !got.left, "the first item is taken at once");
	// The next frame: still the best, nearer now.
	const NearItem aNearer = Item(0xA, false, 1, 8.0f, 8.0f);
	const NearItem kept = Frame(s, aNearer, aNearer, none, 0.011f);
	Check(kept.valid && kept.ref == 0xA && Near(kept.distance, 8.0f), "held on, with this frame's values");
	// Another item ranks a little better (a centimetre nearer): not
	// clearly, so A stays.
	const NearItem bClose = Item(0xB, false, 1, 7.0f);
	for (int i = 0; i < 30; ++i) {
		Frame(s, bClose, aNearer, none, 0.011f);
	}
	Check(s.held.ref == 0xA && s.challenger == 0, "a challenger within the margin never takes over");
}

void TestChallenge() {
	std::printf("The challenge\n");
	PickHoldState s;
	const NearItem none;
	const NearItem a = Item(0xA, false, 1, 10.0f);
	Frame(s, a, none, none, 0.011f);
	const NearItem bClear = Item(0xB, false, 1, 5.0f);
	NearItem got = Frame(s, bClear, a, none, 0.05f);
	Check(got.ref == 0xA && s.challenger == 0xB, "clearly better: the challenge starts, A still held");
	got = Frame(s, bClear, a, none, 0.05f);
	Check(got.ref == 0xA, "0.10 s: still A");
	got = Frame(s, bClear, a, none, 0.06f);
	Check(got.ref == 0xB && s.held.ref == 0xB && s.challenger == 0, "0.16 s: B taken, the challenge over");

	// The challenger loses its margin half way: the clock restarts.
	PickHoldState t;
	Frame(t, a, none, none, 0.011f);
	Frame(t, bClear, a, none, 0.10f);
	Frame(t, Item(0xB, false, 1, 9.5f), a, none, 0.011f);
	Check(t.held.ref == 0xA && t.challenger == 0 && Near(t.challengeSeconds, 0.0f),
	      "a challenger back within the margin: the clock reset");
	Frame(t, bClear, a, none, 0.10f);
	Check(t.held.ref == 0xA, "and it has to start over");
	Frame(t, bClear, a, none, 0.06f);
	Check(t.held.ref == 0xB, "... until it is over again");

	// Another challenger: its own clock.
	PickHoldState u;
	Frame(u, a, none, none, 0.011f);
	Frame(u, bClear, a, none, 0.10f);
	Frame(u, Item(0xC, false, 1, 4.0f), a, none, 0.10f);
	Check(u.held.ref == 0xA && u.challenger == 0xC && Near(u.challengeSeconds, 0.10f),
	      "a second challenger starts its own clock");

	// A better class without a touch waits too.
	PickHoldState v;
	Frame(v, Item(0xA, false, 3, 0.3f), none, none, 0.011f);
	Frame(v, Item(0xB, false, 1, 0.05f), Item(0xA, false, 3, 0.3f), none, 0.10f);
	Check(v.held.ref == 0xA, "B in the grab's reach against A in the cone: not before the challenge is over");

	// A touch wins at once.
	PickHoldState w;
	Frame(w, Item(0xA, false, 1, 0.05f), none, none, 0.011f);
	const NearItem touched = Frame(w, Item(0xB, false, 0, 1.0f), Item(0xA, false, 1, 0.05f), none, 0.011f);
	Check(touched.ref == 0xB, "a touched item takes over at once");
	PickHoldState x;
	Frame(x, Item(0xA, false, 0, 1.0f), none, none, 0.011f);
	Frame(x, Item(0xB, false, 0, 0.0f), Item(0xA, false, 0, 1.0f), none, 0.011f);
	Check(x.held.ref == 0xA, "but not from another touched item within the margin");
}

void TestGrace() {
	std::printf("The grace\n");
	PickHoldState s;
	const NearItem none;
	const NearItem a = Item(0xA, false, 1, 10.0f);
	Frame(s, a, none, none, 0.011f);
	// No hand reaches for A any more; B is the best.
	const NearItem b = Item(0xB, false, 4, 40.0f);
	NearItem got = Frame(s, b, none, none, 0.05f);
	Check(got.ref == 0xA, "A not reached for: kept through the grace");
	got = Frame(s, b, none, none, 0.05f);
	Check(got.ref == 0xA, "0.10 s: still kept");
	got = Frame(s, b, none, none, 0.06f);
	Check(got.ref == 0xB, "0.16 s: let go, B taken");

	PickHoldState t;
	Frame(t, a, none, none, 0.011f);
	Frame(t, b, none, none, 0.10f);
	Frame(t, a, a, none, 0.011f);
	Check(t.held.ref == 0xA && Near(t.missingSeconds, 0.0f), "reached for again within the grace: as before");
	Frame(t, none, none, none, 0.10f);
	Frame(t, none, none, none, 0.10f);
	Check(!t.held.valid, "gone with nothing else in reach: nothing held");
}

void TestHand() {
	std::printf("The hand\n");
	PickHoldState s;
	const NearItem none;
	const NearItem aRight = Item(0xA, false, 1, 10.0f);
	const NearItem aLeft = Item(0xA, true, 1, 9.5f);
	Frame(s, aRight, none, none, 0.011f);
	// Both hands reach for A, the left a little nearer: the right keeps it.
	for (int i = 0; i < 30; ++i) {
		Frame(s, aLeft, aRight, aLeft, 0.011f);
	}
	Check(s.held.ref == 0xA && !s.held.left, "the left hand a little nearer: the right keeps it");
	// The left clearly nearer: after the challenge.
	const NearItem aLeftNear = Item(0xA, true, 1, 2.0f);
	Frame(s, aLeftNear, aRight, aLeftNear, 0.10f);
	Check(!s.held.left && s.challenger == 0xA && s.challengerLeft, "the left hand clearly nearer: a challenge");
	Frame(s, aLeftNear, aRight, aLeftNear, 0.06f);
	Check(s.held.left, "... and the left hand has it");
	// The left hand leaves it, the right still reaches: the right at once.
	NearItem got = Frame(s, aRight, aRight, none, 0.011f);
	Check(got.left == false && got.ref == 0xA && s.missingSeconds == 0.0f,
	      "the holding hand gone, the other reaching: the other at once, no grace");
}

void TestAnchor() {
	std::printf("The anchor\n");
	AnchorState a;
	NiPoint3 p = StepAnchor(a, 0xA, NiPoint3{10.0f, 0.0f, 0.0f}, 0.011f);
	Check(a.valid && Near(p.x, 10.0f), "the first point is taken as it is");
	p = StepAnchor(a, 0xA, NiPoint3{20.0f, 0.0f, 0.0f}, 0.011f);
	Check(p.x > 10.5f && p.x < 12.5f, "the same thing: a step of the way (about an eighth at 90 Hz)");
	float x = p.x;
	for (int i = 0; i < 20; ++i) {
		x = StepAnchor(a, 0xA, NiPoint3{20.0f, 0.0f, 0.0f}, 0.011f).x;
	}
	Check(x > 19.0f, "after 0.23 s most of the way there");
	p = StepAnchor(a, 0xB, NiPoint3{50.0f, 0.0f, 0.0f}, 0.011f);
	Check(Near(p.x, 50.0f) && a.ref == 0xB, "another thing: snapped to it");
	p = StepAnchor(a, 0xB, NiPoint3{60.0f, 0.0f, 0.0f}, 0.0f);
	Check(Near(p.x, 60.0f), "no frame time: the target as it is");
	p = StepAnchor(a, 0, NiPoint3{1.0f, 0.0f, 0.0f}, 0.011f);
	Check(!a.valid && Near(p.x, 1.0f), "nothing: not held, the point passed through");
}

void TestShownSettle() {
	std::printf("What is shown settles\n");
	RefSettleState s;
	Check(StepRefSettle(s, 0xA, 0.011f) == 0xA, "nothing shown: the first thing at once");
	Check(StepRefSettle(s, 0xA, 0.011f) == 0xA, "the same thing: shown on");
	Check(StepRefSettle(s, 0xB, 0.03f) == 0xA, "another thing wanted: A still shown");
	Check(StepRefSettle(s, 0xB, 0.03f) == 0xA, "0.06 s: still A");
	Check(StepRefSettle(s, 0xA, 0.011f) == 0xA && s.wanted == 0xA, "back to A before the settle: nothing changed");
	Check(StepRefSettle(s, 0xB, 0.03f) == 0xA, "B again: its clock starts over");
	Check(StepRefSettle(s, 0xB, 0.06f) == 0xB, "0.09 s of B: shown");
	Check(StepRefSettle(s, 0, 0.03f) == 0xB, "nothing wanted: B shown through the settle");
	Check(StepRefSettle(s, 0, 0.06f) == 0, "... then nothing");
	Check(StepRefSettle(s, 0xC, 0.011f) == 0xC, "and the next thing at once");
	Check(StepRefSettle(s, 0xD, 0.011f, 0xD) == 0xD, "the pick's own item: at once, no settle");
	Check(StepRefSettle(s, 0xE, 0.011f, 0xD) == 0xD, "another than the pick's: the settle as before");
}

void TestRankForHand() {
	std::printf("One hand's rank of one item\n");
	SearchHand hand;
	hand.valid = true;
	hand.position = NiPoint3{0.0f, 0.0f, 0.0f};
	hand.direction = NiPoint3{0.0f, 1.0f, 0.0f};
	const NiPoint3 centre{0.0f, 50.0f, 0.0f};
	NearItem r = RankItemForHand(hand, true, 0xA, centre, 5.0f, 100.0f, 20.0f);
	Check(r.valid && r.left && r.ref == 0xA && r.rankClass == 2 && Near(r.distance, 45.0f),
	      "the laser on it from half a metre: class 2, the left hand's");
	r = RankItemForHand(hand, false, 0xA, centre, 5.0f, 30.0f, 20.0f);
	Check(!r.valid, "out of the hand's reach: invalid");
	r = RankItemForHand(hand, false, 0xA, centre, 5.0f, 100.0f, 20.0f, 2.0f);
	Check(r.valid && r.rankClass == 0 && Near(r.rankKey, 2.0f) && Near(r.distance, 2.0f),
	      "the mesh known 2 units away: touched, by that distance");
	r = RankItemForHand(hand, false, 0xA, centre, 60.0f, 100.0f, 20.0f, 30.0f);
	Check(r.valid && r.rankClass == 4 && Near(r.distance, 30.0f),
	      "inside the sphere but the mesh 30 units off: not touched, nor within the grab's reach");
	SearchHand off;
	off.valid = false;
	r = RankItemForHand(off, false, 0xA, centre, 5.0f, 100.0f, 20.0f);
	Check(!r.valid, "no hand: invalid");
	SearchHand away = hand;
	away.direction = NiPoint3{1.0f, 0.0f, 0.0f};
	r = RankItemForHand(away, false, 0xA, centre, 5.0f, 100.0f, 20.0f);
	Check(!r.valid, "pointing elsewhere, beyond the grab's reach: not reached for");
}

}  // namespace

int main() {
	TestClearlyBetter();
	TestTakeAndStay();
	TestChallenge();
	TestGrace();
	TestHand();
	TestAnchor();
	TestShownSettle();
	TestRankForHand();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
