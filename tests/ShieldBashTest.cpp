// Checks the shield bash (game/ShieldBashLogic.h): when the shield hand
// bashes, how hard, what it is told as and what it costs.

#include <cstdio>

#include "game/ShieldBashLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::game;

ShieldBashHand Hand(float speed) {
	ShieldBashHand h;
	h.shieldWorn = true;
	h.valid = true;
	h.towardsSpeed = speed;
	return h;
}

void TestBash() {
	std::printf("When the shield bashes\n");
	ShieldBashSettings s;
	Check(s.enabled && s.speed == 1.5f && s.hardSpeed == 3.5f, "on by default, a bash from 1.5 m/s, a hard one from 3.5");
	Check(ShieldBashFor(s, Hand(1.0f)) == ShoveKind::None, "too slow: nothing");
	Check(ShieldBashFor(s, Hand(2.0f)) == ShoveKind::Light, "fast: a bash, they stagger");
	Check(ShieldBashFor(s, Hand(4.0f)) == ShoveKind::Hard, "very fast: a hard bash, they go down");
	ShieldBashHand bare = Hand(4.0f);
	bare.shieldWorn = false;
	Check(ShieldBashFor(s, bare) == ShoveKind::None, "no shield: no bash (the punch or the slap is the off hand's)");
	ShieldBashHand lost = Hand(4.0f);
	lost.valid = false;
	Check(ShieldBashFor(s, lost) == ShoveKind::None, "the hand untracked: nothing");
	ShieldBashHand busy = Hand(4.0f);
	busy.holdsSomething = true;
	Check(ShieldBashFor(s, busy) == ShoveKind::None, "the hand holding something: nothing");
	ShieldBashSettings off;
	off.enabled = false;
	Check(ShieldBashFor(off, Hand(4.0f)) == ShoveKind::None, "ShieldBash=0: nothing");
	ShieldBashHand away = Hand(-4.0f);
	Check(ShieldBashFor(s, away) == ShoveKind::None, "moving away: nothing");
}

void TestRules() {
	std::printf("What it is told as and costs\n");
	ShoveSettings shove;
	const ShoveSettings told = BashShoveSettings(shove, false);
	Check(told.countsAsHit && told.lightCountsAsHit && told.fatigueLight == 0.0f && told.fatigueHard == 0.0f,
	      "without the engine's blow: told as a hit, the shove's fatigue not paid");
	const ShoveSettings quiet = BashShoveSettings(shove, true);
	Check(!quiet.countsAsHit && !quiet.lightCountsAsHit, "the engine's blow landed: it told them already");
	ShieldBashSettings s;
	Check(ShieldBashFatigue(s, ShoveKind::Light) == 15.0f && ShieldBashFatigue(s, ShoveKind::Hard) == 35.0f &&
	          ShieldBashFatigue(s, ShoveKind::None) == 0.0f,
	      "a bash costs 15 fatigue, a hard one 35, none nothing");
	ShieldBashSettings odd;
	odd.fatigue = -5.0f;
	Check(ShieldBashFatigue(odd, ShoveKind::Light) == 0.0f, "a negative cost: nothing");
}

}  // namespace

int main() {
	TestBash();
	TestRules();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
