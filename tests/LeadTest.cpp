// Checks taking someone by the hand (game/LeadLogic.h): when a grip takes a
// hand, how far a stranger follows and what it costs, and why it ends.

#include <cstdio>

#include "game/LeadLogic.h"

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

void TestTake() {
	std::printf("Taking a hand\n");
	const LeadSettings s;
	Check(LeadTakes(s, true, true, 10.0f, false), "the grip closes next to their hand: taken");
	Check(!LeadTakes(s, true, true, 20.0f, false), "15 cm away: not");
	Check(!LeadTakes(s, false, true, 5.0f, false), "the grip was already closed: not");
	Check(!LeadTakes(s, true, false, 5.0f, false), "the hand holds something: not");
	Check(!LeadTakes(s, true, true, 5.0f, true), "they are fighting: not");
	LeadSettings off;
	off.enabled = false;
	Check(!LeadTakes(off, true, true, 5.0f, false), "switched off: not");
	volatile float zero = 0.0f;
	Check(!LeadTakes(s, true, true, zero / zero, false), "no distance: not");
}

void TestStranger() {
	std::printf("A stranger\n");
	const LeadSettings s;
	LeadState l;
	StartLead(l, false);
	SInt32 d = 0;
	Check(StepLead(l, s, true, true, 30.0f, 0.2f, d) == LeadEnd::None && d == 0,
	      "20 cm: still held, not yet a whole point");
	Check(StepLead(l, s, true, true, 30.0f, 0.2f, d) == LeadEnd::None && d == 1, "40 cm: a point off their liking");
	Check(StepLead(l, s, true, true, 30.0f, 3.0f, d) == LeadEnd::None && d == 9, "3 m more: 9 points");
	Check(StepLead(l, s, true, true, 30.0f, 1.6f, d) == LeadEnd::PulledFree && !l.active,
	      "5 m in all: they pull free");
	Check(StepLead(l, s, true, true, 30.0f, 1.0f, d) == LeadEnd::None && d == 0, "no longer held: nothing");
}

void TestFollower() {
	std::printf("A follower\n");
	const LeadSettings s;
	LeadState l;
	StartLead(l, true);
	SInt32 d = 0;
	for (int i = 0; i < 20; ++i) {
		StepLead(l, s, true, true, 30.0f, 1.0f, d);
	}
	Check(l.active && d == 0 && l.metres > 19.0f, "20 m: still held, liking untouched");
}

void TestEnds() {
	std::printf("How it ends\n");
	const LeadSettings s;
	LeadState l;
	SInt32 d = 0;
	StartLead(l, true);
	Check(StepLead(l, s, false, true, 30.0f, 0.0f, d) == LeadEnd::LetGo, "the grip opens: let go");
	StartLead(l, true);
	Check(StepLead(l, s, true, true, 400.0f, 0.0f, d) == LeadEnd::TooFar, "the hands 6 m apart: too far");
	StartLead(l, true);
	Check(StepLead(l, s, true, false, 30.0f, 0.0f, d) == LeadEnd::Gone, "they died or went: gone");
	StartLead(l, false);
	Check(StepLead(l, s, false, true, 30.0f, 2.0f, d) == LeadEnd::LetGo && d == 0,
	      "let go while moving: the last step costs nothing");
	Check(StepLead(l, s, true, true, 30.0f, 1.0f, d) == LeadEnd::None, "not held: nothing");
	Check(LeadEndName(LeadEnd::PulledFree)[0] == 'p' && LeadEndName(LeadEnd::None)[0] == 'h', "named for the log");
}

}  // namespace

int main() {
	TestTake();
	TestStranger();
	TestFollower();
	TestEnds();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
