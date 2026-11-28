// Checks the shield bash (game/ShieldBashLogic.h): when the shield hand
// bashes, how hard, what it is told as and what it costs.

#include <cstdio>
#include <limits>

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
	Check(s.enabled && s.speed == 3.0f && s.hardSpeed == 6.0f, "on by default, at the one speed: a bash from 3 m/s, a hard one from 6");
	Check(ShieldBashFor(s, Hand(1.0f)) == ShoveKind::None, "too slow: nothing");
	Check(ShieldBashFor(s, Hand(4.0f)) == ShoveKind::Light, "fast: a bash, they stagger");
	Check(ShieldBashFor(s, Hand(6.5f)) == ShoveKind::Hard, "very fast: a hard bash, they go down");
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

bool Near(float a, float b, float eps = 0.0001f) { return a - b < eps && b - a < eps; }

// The bash's speed in 3D along the line from the shield's middle to their
// bound's centre (BashSpeedTowards), against the shove's across the ground.
void TestBashSpeed() {
	std::printf("The bash's speed towards them\n");
	const NiPoint3 shield{0.0f, 0.0f, 0.0f};
	const NiPoint3 level{0.0f, 40.0f, 0.0f};
	const NiPoint3 ahead{0.0f, 3.5f, 0.0f};
	Check(Near(BashSpeedTowards(ahead, shield, level), SpeedTowards(ahead, shield, level)) &&
	          Near(BashSpeedTowards(ahead, shield, level), 3.5f),
	      "a level target: the same number as the ground measure");
	const NiPoint3 oblique{2.0f, 3.0f, 0.0f};
	Check(Near(BashSpeedTowards(oblique, shield, level), SpeedTowards(oblique, shield, level)),
	      "a level target, the hand at a slant: the same number too");
	const NiPoint3 above{0.0f, 12.0f, 33.0f};  // the imp's middle over the shield's rest (OBVR.log, 2026-11-28)
	const float length = obvr::math::Sqrt(12.0f * 12.0f + 33.0f * 33.0f);
	const NiPoint3 atIt{0.0f, 3.5f * 12.0f / length, 3.5f * 33.0f / length};
	Check(Near(BashSpeedTowards(atIt, shield, above), 3.5f, 0.001f), "a target above: the whole approach speed");
	Check(SpeedTowards(atIt, shield, above) < 3.0f, "where the ground measure lost its rise (under the 3.0 gate)");
	const NiPoint3 rat{0.0f, 20.0f, -40.0f};
	const NiPoint3 down{0.0f, 2.0f, -4.0f};
	Check(Near(BashSpeedTowards(down, shield, rat), obvr::math::Sqrt(20.0f), 0.001f) &&
	          SpeedTowards(down, shield, rat) < BashSpeedTowards(down, shield, rat),
	      "a target below: the drop counts");
	Check(BashSpeedTowards(NiPoint3{0.0f, -3.5f, 0.0f}, shield, level) < 0.0f, "moving away: negative, no bash");
	Check(Near(BashSpeedTowards(ahead, shield, shield), 0.0f), "no line (the shield at their middle): no speed");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Check(Near(BashSpeedTowards(NiPoint3{nan, 3.0f, 0.0f}, shield, level), 0.0f), "a velocity not a number: no speed");
	Check(Near(BashSpeedTowards(ahead, NiPoint3{nan, 0.0f, 0.0f}, level), 0.0f), "a place not a number: no speed");
	Check(ShieldBashFor(ShieldBashSettings{}, Hand(BashSpeedTowards(atIt, shield, above))) == ShoveKind::Light,
	      "and the imp's jab bashes");
}

}  // namespace

int main() {
	TestBash();
	TestRules();
	TestBashSpeed();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
