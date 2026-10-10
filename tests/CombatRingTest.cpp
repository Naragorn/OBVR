#include <cstdio>
#include <limits>

#include "game/CombatRingLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr::game;

constexpr float kDt = 1.0f / 90.0f;

CombatRingSettings FiveMetres(UInt32 atOnce = 1) {
	return CombatRingFrom(true, 5.0f, static_cast<int>(atOnce), 70.0f);
}

void TestSettings() {
	std::printf("the settings\n");
	CombatRingSettings s = CombatRingFrom(true, 5.0f, 1, 70.0f);
	Check(s.enabled && s.ringUnits == 350.0f && s.attackersAtOnce == 1 && s.bandUnits == 49.0f,
	      "5 m, 1 at once: 350 units, a 0.7 m band");
	Check(!CombatRingFrom(true, 0.0f, 1, 70.0f).enabled, "0 m: off");
	Check(!CombatRingFrom(false, 5.0f, 1, 70.0f).enabled, "the switch off: off");
	Check(CombatRingFrom(true, 15.0f, 1, 70.0f).ringUnits == 700.0f, "past 10 m: 10");
	Check(CombatRingFrom(true, 5.0f, 0, 70.0f).attackersAtOnce == 1, "0 at once: 1");
	Check(CombatRingFrom(true, 5.0f, 9, 70.0f).attackersAtOnce == 5, "9 at once: 5");
	s = CombatRingFrom(true, 1.0f, 1, 70.0f);
	Check(s.bandUnits > 23.2f && s.bandUnits < 23.4f, "a 1 m ring: the band a third of it");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(!CombatRingFrom(true, nan, 1, 70.0f).enabled, "not a number: off");
}

void TestOrders() {
	std::printf("a waiting fighter's legs\n");
	const CombatRingSettings s = FiveMetres();
	Check(WaitingOrder(100.0f, s) == RingOrder::Back, "well inside: back");
	Check(WaitingOrder(300.0f, s) == RingOrder::Back, "just inside the band: back");
	Check(WaitingOrder(320.0f, s) == RingOrder::Circle, "in the band: round");
	Check(WaitingOrder(399.0f, s) == RingOrder::Circle, "the band's outer edge: round");
	Check(WaitingOrder(450.0f, s) == RingOrder::Free, "outside: walks in as it will");
	Check(WaitingOrder(-1.0f, s) == RingOrder::Free, "no distance: left alone");

	const UInt16 runForward = kMoveForward | kMoveRun | 0x1000;
	Check(RingFlags(runForward, RingOrder::Free, true) == runForward, "Free: untouched");
	Check(RingFlags(runForward, RingOrder::Back, true) == (kMoveBackward | kMoveWalk | 0x1000),
	      "back: a walk backwards, other bits kept");
	Check(RingFlags(runForward, RingOrder::Circle, true) == (kMoveLeft | kMoveWalk | 0x1000), "round: left");
	Check(RingFlags(0, RingOrder::Circle, false) == (kMoveRight | kMoveWalk), "standing, round: right");
	Check(RingFlags(kMoveForward | kMoveLeft, RingOrder::Back, false) == (kMoveBackward | kMoveWalk),
	      "forward and left: only back");
}

void TestTurns() {
	std::printf("the turns, one at a time\n");
	const CombatRingSettings s = FiveMetres();
	RingState state;
	RingFighter f[3];
	f[0] = RingFighter{0xA, 200.0f, false};
	f[1] = RingFighter{0xB, 150.0f, false};
	f[2] = RingFighter{0xC, 320.0f, false};
	RingVerdict v = StepCombatRing(state, s, f, 3, kDt);
	Check(v.turns == 1 && v.waiting == 2 && v.turnsGiven == 1, "three come: one turn, two wait");
	Check(RingOrderOf(state, 0xB) == RingOrder::Free, "the nearest has the turn");
	Check(RingOrderOf(state, 0xA) == RingOrder::Back, "the one inside the ring backs off");
	Check(RingOrderOf(state, 0xC) == RingOrder::Circle, "the one at the ring goes round");

	// B attacks; while the blow is in flight nothing changes.
	f[1].attacking = true;
	v = StepCombatRing(state, s, f, 3, kDt);
	Check(v.turnsEnded == 0 && RingOrderOf(state, 0xB) == RingOrder::Free, "mid-blow: B keeps the turn");
	// A waiting one that attacks finishes its blow.
	f[0].attacking = true;
	v = StepCombatRing(state, s, f, 3, kDt);
	Check(RingOrderOf(state, 0xA) == RingOrder::Free, "a waiting one mid-blow is left to finish it");
	f[0].attacking = false;
	// B's blow through: its turn ends, the longest waiting (A and C equal:
	// the nearer, A) steps in.
	f[1].attacking = false;
	v = StepCombatRing(state, s, f, 3, kDt);
	Check(v.turnsEnded == 1 && v.turnsGiven == 1 && v.turns == 1, "the blow through: a turn ends, one is given");
	Check(RingOrderOf(state, 0xA) == RingOrder::Free, "A, waited as long as C and nearer, steps in");
	Check(RingOrderOf(state, 0xB) == RingOrder::Back, "B waits now, inside the ring: back");
	// A's turn without a blow: after 8 s it gives way to C, who waited longest.
	for (int i = 0; i < 8 * 90 + 2; ++i) {
		v = StepCombatRing(state, s, f, 3, kDt);
	}
	Check(RingOrderOf(state, 0xC) == RingOrder::Free && RingOrderOf(state, 0xA) != RingOrder::Free,
	      "8 s without a blow: the turn passes to C, who waited longest");
}

void TestAlone() {
	std::printf("one fighter alone keeps the turn\n");
	const CombatRingSettings s = FiveMetres();
	RingState state;
	RingFighter f{0xA, 100.0f, true};
	StepCombatRing(state, s, &f, 1, kDt);
	f.attacking = false;
	RingVerdict v = StepCombatRing(state, s, &f, 1, kDt);
	Check(v.turns == 1 && v.turnsEnded == 0, "its blow through and nobody waiting: still its turn");
	for (int i = 0; i < 20 * 90; ++i) {
		v = StepCombatRing(state, s, &f, 1, kDt);
	}
	Check(v.turns == 1 && RingOrderOf(state, 0xA) == RingOrder::Free, "20 s idle, nobody waiting: still its turn");
}

void TestTwoAtOnce() {
	std::printf("two at once\n");
	RingState state;
	RingFighter f[3] = {{0xA, 300.0f, false}, {0xB, 100.0f, false}, {0xC, 200.0f, false}};
	RingVerdict v = StepCombatRing(state, FiveMetres(2), f, 3, kDt);
	Check(v.turns == 2 && v.waiting == 1, "two turns, one waits");
	Check(RingOrderOf(state, 0xA) == RingOrder::Back, "the farthest waits");
}

void TestComingAndGoing() {
	std::printf("fighters come and go\n");
	const CombatRingSettings s = FiveMetres();
	RingState state;
	RingFighter f[2] = {{0xA, 100.0f, false}, {0xB, 200.0f, false}};
	StepCombatRing(state, s, f, 2, kDt);
	Check(RingOrderOf(state, 0xA) == RingOrder::Free, "A has the turn");
	// A dies or flees: gone from the list; B steps in at once.
	RingVerdict v = StepCombatRing(state, s, &f[1], 1, kDt);
	Check(state.count == 1 && v.turnsGiven == 1 && RingOrderOf(state, 0xB) == RingOrder::Free,
	      "A gone: forgotten, B has the turn");
	Check(RingOrderOf(state, 0xA) == RingOrder::Free, "an unknown actor: left alone");
	// Switched off: everybody free, all forgotten.
	CombatRingSettings off = s;
	off.enabled = false;
	v = StepCombatRing(state, off, f, 2, kDt);
	Check(state.count == 0 && v.turns == 0, "off: nobody held");
	// No fighters: nothing.
	v = StepCombatRing(state, s, nullptr, 0, kDt);
	Check(state.count == 0 && v.turns == 0 && v.waiting == 0, "nobody fighting: nothing");
	// A zero actor is skipped.
	RingFighter zero{0, 100.0f, false};
	v = StepCombatRing(state, s, &zero, 1, kDt);
	Check(state.count == 0, "actor 0: skipped");
}

void TestTableFull() {
	std::printf("more fighters than the table holds\n");
	RingState state;
	RingFighter f[kRingMembersMax + 2];
	for (UInt32 i = 0; i < kRingMembersMax + 2; ++i) {
		f[i] = RingFighter{0x100 + i, 100.0f + static_cast<float>(i), false};
	}
	StepCombatRing(state, FiveMetres(), f, kRingMembersMax + 2, kDt);
	Check(state.count == kRingMembersMax, "the table full: the rest left to the game");
	Check(RingOrderOf(state, 0x100 + kRingMembersMax + 1) == RingOrder::Free, "one past it: free");
}

}  // namespace

int main() {
	TestSettings();
	TestOrders();
	TestTurns();
	TestAlone();
	TestTwoAtOnce();
	TestComingAndGoing();
	TestTableFull();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
