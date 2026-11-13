// Checks the A and B held together (vr/ChordHold.h): which presses make a
// chord in either order, the ring and the reach, the call-off by either
// button, nothing again until both are let go, and the presses that stay
// single buttons.

#include <cstdio>

#include "vr/ChordHold.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr float kHold = 1.0f;
constexpr float kRing = 0.5f;
constexpr float kWindow = 0.15f;
constexpr float kDt = 0.01f;

ChordHoldVerdict Step(ChordHoldState& s, bool a, bool b, float dt = kDt) {
	return StepChordHold(s, a, b, kHold, kRing, kWindow, dt);
}

// Holds both for `seconds`; true if the chord was reached on the way, and
// how many frames reached it.
int HoldBoth(ChordHoldState& s, float seconds, float* lastFill = nullptr, bool* ringSeen = nullptr) {
	int reached = 0;
	for (float t = 0.0f; t < seconds; t += kDt) {
		const ChordHoldVerdict v = Step(s, true, true);
		reached += v.reached ? 1 : 0;
		if (v.ringShown && lastFill != nullptr) {
			*lastFill = v.fill;
		}
		if (v.ringShown && ringSeen != nullptr) {
			*ringSeen = true;
		}
	}
	return reached;
}

void TestBOntoA() {
	std::printf("B onto a held A\n");
	ChordHoldState s;
	bool aloneActive = false;
	for (int i = 0; i < 50; ++i) {
		aloneActive = aloneActive || Step(s, true, false).active;
	}
	Check(!aloneActive, "A alone for 0.5 s is no chord");
	const ChordHoldVerdict v = Step(s, true, true);
	Check(v.active && v.began, "B pressed while A has been held 0.5 s: the chord begins");
	Check(!v.ringShown, "no ring at the start");
	float fill = 0.0f;
	bool ring = false;
	const int reached = HoldBoth(s, 1.2f, &fill, &ring);
	Check(ring, "the ring is up from 0.5 s");
	Check(fill > 0.9f, "it fills toward full");
	Check(reached == 1, "reached exactly once");
	Check(Step(s, true, true).active, "still the chord's after the reach");
	Check(!Step(s, true, true).reached, "held on: not again");
	Check(Step(s, false, true).active, "A let go, B still held: still the chord's");
	const ChordHoldVerdict up = Step(s, false, false);
	Check(up.active && !up.cancelled, "both let go: the release frame is the chord's, nothing called off");
	Check(!Step(s, false, false).active, "then free");
}

void TestSameFrame() {
	std::printf("Both in one frame\n");
	ChordHoldState s;
	const ChordHoldVerdict v = Step(s, true, true);
	Check(v.active && v.began, "A and B in the same frame: the chord");
	Check(HoldBoth(s, 1.1f) == 1, "reached once");
}

void TestAOntoFreshB() {
	std::printf("A onto a fresh B\n");
	ChordHoldState s;
	Step(s, false, true);
	for (int i = 0; i < 9; ++i) {
		Step(s, false, true);
	}
	const ChordHoldVerdict v = Step(s, true, true);
	Check(v.active && v.began, "A 0.10 s after B: the chord");
	Check(HoldBoth(s, 1.1f) == 1, "reached once");
}

void TestAOntoOldB() {
	std::printf("A onto a B held too long\n");
	ChordHoldState s;
	Step(s, false, true);
	for (int i = 0; i < 30; ++i) {
		Step(s, false, true);
	}
	const ChordHoldVerdict v = Step(s, true, true);
	Check(!v.active, "A 0.30 s after B: no chord, B is on its way to its own hold");
	Check(HoldBoth(s, 1.5f) == 0, "never reached while both stay down");
}

void TestCalledOff() {
	std::printf("Called off\n");
	ChordHoldState s;
	Step(s, true, true);
	bool ring = false;
	HoldBoth(s, 0.7f, nullptr, &ring);
	Check(ring, "the ring is up at 0.7 s");
	const ChordHoldVerdict v = Step(s, true, false);
	Check(v.active && v.cancelled && !v.reached, "B let go at 0.7 s: called off");
	Check(Step(s, true, true).active, "B again while A is still held: still the old chord's");
	Check(HoldBoth(s, 1.5f) == 0, "no reach until both were let go");
	Step(s, false, false);
	Step(s, false, false);
	Check(Step(s, true, true).began, "a fresh press of both: a new chord");
	Check(HoldBoth(s, 1.1f) == 1, "that one reaches");

	ChordHoldState early;
	Step(early, true, true);
	HoldBoth(early, 0.2f);
	const ChordHoldVerdict e = Step(early, false, true);
	Check(e.active && !e.cancelled, "A let go at 0.2 s, before the ring: nothing said called off");
	Check(Step(early, false, true).active, "B still held stays the chord's (no Tab, no pause ring)");

	ChordHoldState both;
	Step(both, true, true);
	HoldBoth(both, 0.7f);
	const ChordHoldVerdict b = Step(both, false, false);
	Check(b.active && b.cancelled && !b.reached, "both let go in one frame at 0.7 s: called off as well");
	Check(!Step(both, false, false).active, "and free after it");
}

void TestOnlyOne() {
	std::printf("One button alone\n");
	ChordHoldState s;
	bool any = false;
	for (int i = 0; i < 300; ++i) {
		any = any || Step(s, false, true).active;
	}
	Step(s, false, false);
	for (int i = 0; i < 300; ++i) {
		any = any || Step(s, true, false).active;
	}
	Check(!any, "B alone and A alone never make a chord");
}

void TestDtZero() {
	std::printf("No time\n");
	ChordHoldState s;
	Step(s, true, true, 0.0f);
	bool reached = false;
	for (int i = 0; i < 500; ++i) {
		reached = reached || Step(s, true, true, 0.0f).reached;
	}
	Check(!reached, "frames without time never reach");
	reached = false;
	for (int i = 0; i < 200; ++i) {
		reached = reached || Step(s, true, true, -0.01f).reached;
	}
	Check(!reached, "negative time counts as none");
}

}  // namespace

int main() {
	TestBOntoA();
	TestSameFrame();
	TestAOntoFreshB();
	TestAOntoOldB();
	TestCalledOff();
	TestOnlyOne();
	TestDtZero();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
