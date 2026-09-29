// Checks the shove's decisions (game/ShoveLogic.h): when a hand shoves, how
// hard, its speed towards the actor, the reach, and the cooldown.

#include <cstdio>

#include "game/ShoveLogic.h"

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

bool Near(float a, float b) { return a - b < 1e-3f && b - a < 1e-3f; }

ShoveHand Open(float speed) {
	ShoveHand h;
	h.valid = true;
	h.open = true;
	h.towardsSpeed = speed;
	return h;
}

void TestKind() {
	std::printf("When a hand shoves\n");
	const ShoveSettings s;
	Check(ShoveFor(s, false, Open(1.0f)) == ShoveKind::None, "slower than the shove speed: nothing");
	Check(ShoveFor(s, false, Open(2.5f)) == ShoveKind::Light, "from 2 m/s: a light shove");
	Check(ShoveFor(s, false, Open(4.5f)) == ShoveKind::Hard, "from 3.6 m/s: a hard one");
	Check(ShoveFor(s, true, Open(4.5f)) == ShoveKind::None, "a weapon or the fists up: a swing, no shove");
	ShoveHand fist = Open(4.5f);
	fist.open = false;
	Check(ShoveFor(s, false, fist) == ShoveKind::None, "a fist: no shove");
	ShoveHand gripping = Open(4.5f);
	gripping.gripHeld = true;
	Check(ShoveFor(s, false, gripping) == ShoveKind::None, "the grip closed (holding, reaching): no shove");
	ShoveHand lost = Open(4.5f);
	lost.valid = false;
	Check(ShoveFor(s, false, lost) == ShoveKind::None, "the hand not tracked: nothing");
	ShoveSettings off;
	off.enabled = false;
	Check(ShoveFor(off, false, Open(9.0f)) == ShoveKind::None, "switched off: nothing");
	volatile float zero = 0.0f;
	Check(ShoveFor(s, false, Open(zero / zero)) == ShoveKind::None, "a speed that is not a number: nothing");
}

void TestSpeedTowards() {
	std::printf("The speed towards the actor\n");
	const NiPoint3 hand{0.0f, 0.0f, 100.0f};
	const NiPoint3 target{0.0f, 50.0f, 100.0f};
	Check(Near(SpeedTowards(NiPoint3{0.0f, 3.0f, 0.0f}, hand, target), 3.0f), "straight at it: all of it");
	Check(Near(SpeedTowards(NiPoint3{3.0f, 0.0f, 0.0f}, hand, target), 0.0f), "across it: none");
	Check(Near(SpeedTowards(NiPoint3{0.0f, -2.0f, 0.0f}, hand, target), -2.0f), "away from it: negative");
	Check(Near(SpeedTowards(NiPoint3{0.0f, 0.0f, -5.0f}, hand, target), 0.0f),
	      "straight down (onto a head): no part across the ground");
	Check(Near(SpeedTowards(NiPoint3{0.0f, 3.0f, 0.0f}, hand, NiPoint3{0.0f, 0.0f, 50.0f}), 0.0f),
	      "the actor right below the hand: no direction, none");
}

void TestReach() {
	std::printf("The reach\n");
	const NiPoint3 centre{0.0f, 0.0f, 60.0f};
	Check(HandAtActor(NiPoint3{0.0f, 20.0f, 60.0f}, centre, 60.0f, 0.5f, 4.0f), "inside half the bound plus 4: at it");
	Check(!HandAtActor(NiPoint3{0.0f, 40.0f, 60.0f}, centre, 60.0f, 0.5f, 4.0f), "further: not at it");
}

void TestPush() {
	std::printf("The light shove's push\n");
	const NiPoint3 p = ShovePush(NiPoint3{0.0f, 0.0f, 100.0f}, NiPoint3{0.0f, 40.0f, 60.0f}, 30.0f);
	Check(Near(p.x, 0.0f) && Near(p.y, 30.0f) && Near(p.z, 0.0f), "30 units away from the hand, along the ground");
	const NiPoint3 none = ShovePush(NiPoint3{5.0f, 5.0f, 0.0f}, NiPoint3{5.0f, 5.0f, 90.0f}, 30.0f);
	Check(none.LengthSquared() == 0.0f, "straight above each other: no push");
	Check(ShovePush(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 10.0f, 0.0f}, 0.0f).LengthSquared() == 0.0f,
	      "a distance of 0: no push");
}

void TestCooldown() {
	std::printf("The cooldown\n");
	ShoveCooldown c;
	int a = 0;
	int b = 0;
	Check(ShoveAllowed(c, &a), "nobody shoved yet: allowed");
	Check(!ShoveAllowed(c, nullptr), "no actor: not allowed");
	StartShoveCooldown(c, &a, 1.0f);
	Check(!ShoveAllowed(c, &a) && ShoveAllowed(c, &b), "the same actor waits, another does not");
	StepShoveCooldown(c, 0.6f);
	Check(!ShoveAllowed(c, &a), "0.6 s later: still waiting");
	StepShoveCooldown(c, 0.6f);
	Check(ShoveAllowed(c, &a) && c.actor == nullptr, "after the second: allowed again");
}

void TestCountsAsHit() {
	std::printf("A shove counts as a hit\n");
	ShoveSettings s;
	Check(ShoveCountsAsHit(s, ShoveKind::Light) && ShoveCountsAsHit(s, ShoveKind::Hard), "light or hard: a hit");
	Check(!ShoveCountsAsHit(s, ShoveKind::None), "no shove: nothing");
	s.countsAsHit = false;
	Check(!ShoveCountsAsHit(s, ShoveKind::Hard), "switched off: only the disposition");
}

}  // namespace

int main() {
	TestKind();
	TestSpeedTowards();
	TestReach();
	TestCooldown();
	TestPush();
	TestCountsAsHit();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
