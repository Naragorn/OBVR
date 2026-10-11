#include <cstdio>
#include <limits>
#include <string>

#include "game/CombatRingLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float within = 0.01f) { return a == a && a >= b - within && a <= b + within; }

using namespace obvr::game;

constexpr float kDt = 1.0f / 90.0f;

// The ring as it was first built (2026-10-10): turns of 6 s exactly, the
// longest waiting first - the flows below were laid out on it; the drawn
// lengths and the order by level have their own test.
CombatRingSettings FiveMetres(UInt32 atOnce = 1) {
	return CombatRingFrom(true, 5.0f, static_cast<int>(atOnce), 70.0f, 6.0f, 6.0f, false);
}

void TestSettings() {
	std::printf("the settings\n");
	CombatRingSettings s = CombatRingFrom(true, 5.0f, 1, 70.0f);
	Check(s.enabled && s.ringUnits == 350.0f && s.attackersAtOnce == 1 && s.bandUnits == 49.0f,
	      "5 m, 1 at once: 350 units, a 0.7 m band");
	Check(s.turnSeconds == 6.0f && s.turnMaxSeconds == 12.0f && s.idleTurnSeconds == 12.0f && s.byLevel,
	      "a turn: 6 to 12 s, the idle end at the longest, the weakest first");
	Check(FiveMetres().turnMaxSeconds == 6.0f && FiveMetres().idleTurnSeconds == 10.0f, "6 s exactly: 10 s idle");
	Check(CombatRingFrom(true, 5.0f, 1, 70.0f, 15.0f).idleTurnSeconds == 15.0f, "a 15 s turn: idle no shorter");
	Check(CombatRingFrom(true, 5.0f, 1, 70.0f, 1.0f).turnSeconds == 6.0f, "1 s, out of range: the default");
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
	// The ring's left is counter-clockwise seen from above; the legs face
	// the player, so that is a step to the actor's right.
	Check(RingFlags(runForward, RingOrder::Circle, true) == (kMoveRight | kMoveWalk | 0x1000),
	      "round the ring's left: the legs step to their right");
	Check(RingFlags(0, RingOrder::Circle, false) == (kMoveLeft | kMoveWalk), "standing, round the ring's right: their left");
	Check(RingFlags(kMoveForward | kMoveLeft, RingOrder::Back, false) == (kMoveBackward | kMoveWalk),
	      "forward and left: only back");
}

void TestSteps() {
	std::printf("the waiting fighters' steps\n");
	const CombatRingSettings s = FiveMetres();
	float ox = 0.0f;
	float oy = 0.0f;
	Check(RingStepOffset(100.0f, 0.0f, RingOrder::Back, true, s, 0.1f, ox, oy) && ox > 12.9f && ox < 13.1f && oy == 0.0f,
	      "deep inside, back: straight away, twice the walk");
	Check(RingStepOffset(300.0f, 0.0f, RingOrder::Back, true, s, 0.1f, ox, oy) && ox > 6.5f && ox < 6.6f,
	      "back at the band's inner edge: the walk, 65 units a second");
	Check(RingStepOffset(200.0f, 0.0f, RingOrder::Back, true, s, 0.1f, ox, oy) && ox > 10.2f && ox < 10.3f,
	      "back, halfway in: between");
	Check(RingStepOffset(0.0f, 350.0f, RingOrder::Circle, true, s, 0.1f, ox, oy) && ox < -6.4f && oy > -0.1f && oy < 0.1f,
	      "on the ring, round to the left: along the tangent");
	Check(RingStepOffset(0.0f, 350.0f, RingOrder::Circle, false, s, 0.1f, ox, oy) && ox > 6.4f, "to the right: the other way");
	Check(RingStepOffset(0.0f, 320.0f, RingOrder::Circle, true, s, 0.1f, ox, oy) && oy > 3.2f && oy < 3.3f,
	      "inside the line: pulled out, half a step at most");
	Check(!RingStepOffset(100.0f, 0.0f, RingOrder::Free, true, s, 0.1f, ox, oy) && ox == 0.0f, "free: no step");
	Check(!RingStepOffset(100.0f, 0.0f, RingOrder::Back, true, s, 0.0f, ox, oy), "no time: no step");
	Check(!RingStepOffset(100.0f, 0.0f, RingOrder::Back, true, s, 0.5f, ox, oy), "a long stall: no jump");
	Check(!RingStepOffset(0.5f, 0.0f, RingOrder::Back, true, s, 0.1f, ox, oy), "on the player: no direction, no step");
}

void TestShortfall() {
	std::printf("the legs' walk measured, the shortfall placed\n");
	float sx = 0.0f;
	float sy = 0.0f;
	// A first step: the legs' walk unknown, the whole step placed.
	Check(RingStepShortfall(6.5f, 0.0f, false, 99.0f, 99.0f, sx, sy) && Near(sx, 6.5f) && sy == 0.0f,
	      "the walk unknown: the whole step");
	// The legs did it all (and more): nothing placed.
	Check(!RingStepShortfall(6.5f, 0.0f, true, 7.0f, 0.0f, sx, sy) && sx == 0.0f && sy == 0.0f,
	      "the legs walked the step: nothing placed");
	Check(!RingStepShortfall(6.5f, 0.0f, true, 6.5f, 0.0f, sx, sy), "exactly the step: nothing");
	// Half way: the other half.
	Check(RingStepShortfall(6.5f, 0.0f, true, 3.25f, 0.0f, sx, sy) && Near(sx, 3.25f), "half the step walked: half placed");
	// Across the step: counts for nothing along it.
	Check(RingStepShortfall(6.5f, 0.0f, true, 0.0f, 5.0f, sx, sy) && Near(sx, 6.5f) && sy == 0.0f,
	      "walked across it: the whole step along it, nothing across");
	// Against it: made up, at most twice the step.
	Check(RingStepShortfall(6.5f, 0.0f, true, -3.0f, 0.0f, sx, sy) && Near(sx, 9.5f), "walked 3 against it: the step and 3");
	Check(RingStepShortfall(6.5f, 0.0f, true, -40.0f, 0.0f, sx, sy) && Near(sx, 13.0f), "far against it: twice the step at most");
	// A diagonal step keeps its direction.
	Check(RingStepShortfall(3.0f, 4.0f, true, 0.6f, 0.8f, sx, sy) && Near(sx, 2.4f) && Near(sy, 3.2f),
	      "a diagonal step, a fifth walked: four fifths along it");
	// No step wanted, or nothing sane: nothing placed.
	Check(!RingStepShortfall(0.0f, 0.0f, true, 1.0f, 0.0f, sx, sy), "no step: nothing");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(!RingStepShortfall(6.5f, 0.0f, true, nan, 0.0f, sx, sy) && sx == 0.0f, "a walk of no number: nothing placed");

	// The walk measured from where the step before left them; unknown at
	// first and after a frame without a step.
	RingMember m;
	float ex = 0.0f;
	float ey = 0.0f;
	Check(!RingLegsWalked(m, 10.0f, 20.0f, ex, ey) && ex == 0.0f && ey == 0.0f, "nothing placed yet: the walk unknown");
	RingStepTaken(m, 10.0f, 20.0f, 0.0f, 6.5f, 0.0111f);
	Check(RingLegsWalked(m, 13.0f, 21.0f, ex, ey) && Near(ex, 3.0f) && Near(ey, 1.0f), "the next frame: the legs' walk since");
	RingStepTaken(m, 13.0f, 21.0f, 3.0f, 0.0f, 0.0111f);
	Check(Near(m.legsUnits, 3.0f) && Near(m.placedUnits, 6.5f) && m.steppedSeconds > 0.022f && m.steppedSeconds < 0.0223f,
	      "the books: the legs' walk, the placed, the time");
	RingStepTaken(m, 13.0f, 21.0f, 0.0f, 0.0f, -1.0f);
	Check(m.steppedSeconds > 0.022f && m.steppedSeconds < 0.0223f, "no time counted backwards");
	RingStepNone(m);
	Check(!RingLegsWalked(m, 14.0f, 21.0f, ex, ey), "after a frame without a step: unknown again");
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
	// A waiting one that attacks is still kept off.
	f[0].attacking = true;
	v = StepCombatRing(state, s, f, 3, kDt);
	Check(RingOrderOf(state, 0xA) == RingOrder::Back, "a waiting one mid-blow still backs off");
	f[0].attacking = false;
	// B's blow through early: the turn lasts its 6 s first.
	f[1].attacking = false;
	v = StepCombatRing(state, s, f, 3, kDt);
	Check(v.turnsEnded == 0 && RingOrderOf(state, 0xB) == RingOrder::Free,
	      "the blow through after a few frames: B keeps the turn (6 s at least)");
	// Past 6 s, the blow behind it: its turn ends, the longest waiting (A and C
	// equal: the nearer, A) steps in.
	UInt32 ended = 0;
	for (int i = 0; i < 6 * 90 && ended == 0; ++i) {
		v = StepCombatRing(state, s, f, 3, kDt);
		ended = v.turnsEnded;
	}
	Check(ended == 1 && v.turnsGiven == 1 && v.turns == 1, "6 s in, the blow behind it: a turn ends, one is given");
	Check(RingOrderOf(state, 0xA) == RingOrder::Free, "A, waited as long as C and nearer, steps in");
	Check(RingOrderOf(state, 0xB) == RingOrder::Back, "B waits now, inside the ring: back");
	// A's turn without a blow: at 9 s still A's, after 10 s C's, who waited longest.
	for (int i = 0; i < 9 * 90; ++i) {
		v = StepCombatRing(state, s, f, 3, kDt);
	}
	Check(RingOrderOf(state, 0xA) == RingOrder::Free, "9 s without a blow: still A's turn");
	for (int i = 0; i < 90 + 2; ++i) {
		v = StepCombatRing(state, s, f, 3, kDt);
	}
	Check(RingOrderOf(state, 0xC) == RingOrder::Free && RingOrderOf(state, 0xA) != RingOrder::Free,
	      "10 s without a blow: the turn passes to C, who waited longest");
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

void TestTurnLengths() {
	std::printf("the turns' lengths and the order by level\n");
	CombatRingSettings s = CombatRingFrom(true, 5.0f, 1, 70.0f, 5.0f, 15.0f, true);
	Check(s.turnSeconds == 5.0f && s.turnMaxSeconds == 15.0f && s.idleTurnSeconds == 15.0f && s.byLevel,
	      "5 to 15 s: as given, the idle no shorter than the longest");
	Check(CombatRingFrom(true, 5.0f, 1, 70.0f, 6.0f, 3.0f).turnMaxSeconds == 6.0f, "a longest under the shortest: the shortest");
	Check(CombatRingFrom(true, 5.0f, 1, 70.0f, 6.0f, 99.0f).turnMaxSeconds == 60.0f, "past a minute: a minute");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(CombatRingFrom(true, 5.0f, 1, 70.0f, 6.0f, nan).turnMaxSeconds == 12.0f, "not a number: 12");
	Check(CombatRingFrom(true, 5.0f, 1, 70.0f, 20.0f, nan).turnMaxSeconds == 20.0f, "not a number, a 20 s turn: 20");
	Check(!CombatRingFrom(true, 5.0f, 1, 70.0f, 6.0f, 12.0f, false).byLevel, "by level off");
	Check(TurnLengthFor(s, 0.0f) == 5.0f && TurnLengthFor(s, 1.0f) == 15.0f && TurnLengthFor(s, 0.5f) == 10.0f,
	      "the draw spans the two");
	Check(TurnLengthFor(s, -1.0f) == 5.0f && TurnLengthFor(s, 2.0f) == 15.0f && TurnLengthFor(s, nan) == 10.0f,
	      "a draw out of range is held in; not a number: the middle");

	Check(LeveledLevel(12, false, 0, 0, 30) == 12, "an absolute level: itself");
	Check(LeveledLevel(-3, true, 1, 0, 10) == 7, "the player's level minus 3");
	Check(LeveledLevel(-3, true, 5, 0, 2) == 5, "held up at the min");
	Check(LeveledLevel(10, true, 1, 15, 20) == 15, "held down at the max");
	Check(LeveledLevel(0, true, 0, 0, 25) == 25, "no bounds: the player's level");
	Check(LeveledLevel(-5, true, 0, 0, 2) == 1, "never under 1");
	Check(LeveledLevel(-5, false, 0, 0, 2) == 0, "an absolute level under 0: 0");

	// Three come: by level the weakest steps in first, whatever the distance.
	RingState state;
	RingFighter f[3];
	f[0] = RingFighter{0xA, 150.0f, false, 20};
	f[1] = RingFighter{0xB, 200.0f, false, 3};
	f[2] = RingFighter{0xC, 320.0f, false, 8};
	RingVerdict v = StepCombatRing(state, s, f, 3, kDt, 0.0f);
	Check(v.turnsGiven == 1 && RingOrderOf(state, 0xB) == RingOrder::Free, "the level-3 one has the first turn");
	const RingMember* b = nullptr;
	for (UInt32 i = 0; i < state.count; ++i) {
		if (state.member[i].actor == 0xB) {
			b = &state.member[i];
		}
	}
	Check(b != nullptr && b->turnLength == 5.0f, "its turn drawn at the shortest (draw 0)");
	// B attacks at once and the turn ends after 5 s: the level-8 one next, not
	// the level-20 one.
	f[1].attacking = true;
	StepCombatRing(state, s, f, 3, kDt, 0.9f);
	f[1].attacking = false;
	UInt32 ended = 0;
	for (int i = 0; i < 6 * 90 && ended == 0; ++i) {
		v = StepCombatRing(state, s, f, 3, kDt, 0.9f);
		ended = v.turnsEnded;
	}
	Check(ended == 1 && RingOrderOf(state, 0xC) == RingOrder::Free && RingOrderOf(state, 0xA) != RingOrder::Free,
	      "then the level-8 one; the level-20 one waits");
	const RingMember* c = nullptr;
	for (UInt32 i = 0; i < state.count; ++i) {
		if (state.member[i].actor == 0xC) {
			c = &state.member[i];
		}
	}
	Check(c != nullptr && c->turnLength > 13.99f && c->turnLength < 14.01f, "its turn drawn near the longest (draw 0.9: 14 s)");
	// The boss starved a minute: next whatever its level.
	for (UInt32 i = 0; i < state.count; ++i) {
		if (state.member[i].actor == 0xA) {
			state.member[i].waitingSeconds = kRingStarvedSeconds;
		}
	}
	f[2].attacking = true;
	StepCombatRing(state, s, f, 3, kDt, 0.5f);
	f[2].attacking = false;
	ended = 0;
	for (int i = 0; i < 16 * 90 && ended == 0; ++i) {
		v = StepCombatRing(state, s, f, 3, kDt, 0.5f);
		ended = v.turnsEnded;
	}
	Check(ended == 1 && RingOrderOf(state, 0xA) == RingOrder::Free, "starved a minute: the level-20 one goes next");

	// Without the order by level: the longest waiting first, as before.
	CombatRingSettings plain = CombatRingFrom(true, 5.0f, 1, 70.0f, 6.0f, 12.0f, false);
	RingState other;
	StepCombatRing(other, plain, f, 3, kDt);
	Check(RingOrderOf(other, 0xA) == RingOrder::Free, "by level off: the nearest of the equal waiters, whatever its level");
	// Two at once in one frame: two different lengths from one draw.
	RingState two;
	StepCombatRing(two, CombatRingFrom(true, 5.0f, 2, 70.0f, 5.0f, 15.0f), f, 3, kDt, 0.1f);
	float lengths[2] = {0.0f, 0.0f};
	UInt32 n = 0;
	for (UInt32 i = 0; i < two.count; ++i) {
		if (two.member[i].turn && n < 2) {
			lengths[n++] = two.member[i].turnLength;
		}
	}
	Check(n == 2 && lengths[0] != lengths[1], "two turns given at once: two lengths");
	Check(RingGoesBefore(RingMember{0, false, false, false, 0, 6, 10, false, 0, 5}, RingMember{0, false, false, false, 0, 6, 10, false, 0, 9}, true, 6.0f),
	      "the lower level goes before");
	Check(!RingGoesBefore(RingMember{0, false, false, false, 0, 6, 1, false, 0, 5}, RingMember{0, false, false, false, 0, 6, 10, false, 0, 9}, true, 6.0f),
	      "but not one whose turn just ended: the rested first");
	// One weak fighter and a boss: they alternate rather than the weak one
	// taking every turn.
	RingState pair;
	RingFighter two2[2] = {{0xA, 150.0f, false, 20}, {0xB, 200.0f, false, 3}};
	StepCombatRing(pair, s, two2, 2, kDt, 0.0f);
	Check(RingOrderOf(pair, 0xB) == RingOrder::Free, "the weak one first");
	two2[1].attacking = true;
	StepCombatRing(pair, s, two2, 2, kDt, 0.0f);
	two2[1].attacking = false;
	ended = 0;
	for (int i = 0; i < 6 * 90 && ended == 0; ++i) {
		ended = StepCombatRing(pair, s, two2, 2, kDt, 0.0f).turnsEnded;
	}
	Check(ended == 1 && RingOrderOf(pair, 0xA) == RingOrder::Free, "its turn over: the boss, not the weak one again at once");
	two2[0].attacking = true;
	StepCombatRing(pair, s, two2, 2, kDt, 0.0f);
	two2[0].attacking = false;
	ended = 0;
	for (int i = 0; i < 6 * 90 && ended == 0; ++i) {
		ended = StepCombatRing(pair, s, two2, 2, kDt, 0.0f).turnsEnded;
	}
	Check(ended == 1 && RingOrderOf(pair, 0xB) == RingOrder::Free, "then the weak one again, rested");
}

void TestStepProbe() {
	std::printf("a step probed against the world\n");
	RingStepProbe p;
	Check(RingStepClear(p), "nothing in the way, level ground: clear");
	p.riseUnits = 20.0f;
	Check(RingStepClear(p), "a stair's step up: clear");
	p.riseUnits = -20.0f;
	Check(RingStepClear(p), "and down");
	p.riseUnits = 40.0f;
	Check(!RingStepClear(p) && std::string(RingStepRefusal(p)).find("rise") != std::string::npos, "a fence's top: refused");
	p.riseUnits = -40.0f;
	Check(!RingStepClear(p) && std::string(RingStepRefusal(p)).find("drop") != std::string::npos, "a ledge: refused");
	p = RingStepProbe{};
	p.groundFound = false;
	Check(!RingStepClear(p) && std::string(RingStepRefusal(p)) == "no ground there", "no ground: refused");
	p = RingStepProbe{};
	p.blockedLow = true;
	Check(!RingStepClear(p) && std::string(RingStepRefusal(p)).find("knee") != std::string::npos, "a wall at the knees: refused");
	p = RingStepProbe{};
	p.blockedHigh = true;
	Check(!RingStepClear(p) && std::string(RingStepRefusal(p)).find("chest") != std::string::npos, "a rail at the chest: refused");
	p = RingStepProbe{};
	p.riseUnits = std::numeric_limits<float>::quiet_NaN();
	Check(!RingStepClear(p), "a rise that is not a number: refused");

	// The side a fighter walks round, and the turn at a wall.
	RingState state;
	RingFighter f[2] = {{0xA, 100.0f, false, 1}, {0xB, 320.0f, false, 1}};
	StepCombatRing(state, FiveMetres(), f, 2, kDt);
	Check(RingCircleLeft(state, 0xB, true) && RingCircleLeft(state, 0xB, false), "the side chosen once is kept");
	Check(RingStepRefused(state, 0xB) && !RingCircleLeft(state, 0xB, true), "refused: turned round");
	Check(!RingStepRefused(state, 0xB) && !RingCircleLeft(state, 0xB, true), "refused again at once: not turned back");
	for (int i = 0; i < 100; ++i) {
		StepCombatRing(state, FiveMetres(), f, 2, kDt);
	}
	Check(RingStepRefused(state, 0xB) && RingCircleLeft(state, 0xB, true), "a second later: turned again");
	Check(RingCircleLeft(state, 0x99, false) == false && !RingStepRefused(state, 0x99), "an unknown actor: as given, never turned");
	Check(RingRefusalLogged(state, 0xB) && !RingRefusalLogged(state, 0xB), "a refusal logged once, not again at once");
	for (int i = 0; i < 100; ++i) {
		StepCombatRing(state, FiveMetres(), f, 2, kDt);
	}
	Check(RingRefusalLogged(state, 0xB), "a second later: logged again");
	Check(!RingRefusalLogged(state, 0x99), "an unknown actor: never");
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
	TestSteps();
	TestShortfall();
	TestTurns();
	TestAlone();
	TestTwoAtOnce();
	TestComingAndGoing();
	TestTurnLengths();
	TestStepProbe();
	TestTableFull();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
