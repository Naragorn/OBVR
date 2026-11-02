// Checks the pommel strike (game/PommelLogic.h): where the pommel is, when
// it leads, and when a strike with it makes them recoil or fall.

#include <cstdio>

#include "game/PommelLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.01f) { return a - b < eps && b - a < eps; }

using namespace obvr;
using namespace obvr::game;

PommelHand Hand(float speed, float lead) {
	PommelHand h;
	h.meleeDrawn = true;
	h.valid = true;
	h.towardsSpeed = speed;
	h.lead = lead;
	return h;
}

void TestPoint() {
	std::printf("Where the pommel is\n");
	const NiPoint3 hand{0.0f, 0.0f, 100.0f};
	const NiPoint3 grip{0.0f, 5.0f, 100.0f};
	const NiPoint3 tip{0.0f, 65.0f, 100.0f};  // the blade ahead (+y)
	const NiPoint3 p = PommelPoint(hand, grip, tip, 8.0f);
	Check(Near(p.y, -8.0f) && Near(p.x, 0.0f) && Near(p.z, 100.0f), "8 units behind the hand, back along the blade");
	const NiPoint3 q = PommelPoint(hand, grip, grip, 8.0f);
	Check(Near(q.y, 0.0f), "no blade to tell the way: the hand");
	const NiPoint3 r = PommelPoint(hand, grip, tip, -3.0f);
	Check(Near(r.y, -PommelSettings{}.units), "a length out of range: the default");
}

void TestLead() {
	std::printf("The pommel leads\n");
	const NiPoint3 grip{0.0f, 0.0f, 0.0f};
	const NiPoint3 tip{0.0f, 60.0f, 0.0f};
	Check(Near(PommelLead(NiPoint3{0.0f, -3.0f, 0.0f}, grip, tip), 1.0f), "moving straight back along the blade: it leads");
	Check(Near(PommelLead(NiPoint3{0.0f, 3.0f, 0.0f}, grip, tip), -1.0f), "a thrust (the tip leading): -1");
	Check(Near(PommelLead(NiPoint3{3.0f, 0.0f, 0.0f}, grip, tip), 0.0f), "a sideways slash: 0");
	Check(Near(PommelLead(NiPoint3{0.0f, 0.0f, 0.0f}, grip, tip), 0.0f), "still: 0");
}

void TestStrike() {
	std::printf("When it strikes\n");
	PommelSettings s;
	Check(s.enabled && s.speed == 3.0f && s.hardSpeed == 6.0f, "on by default, at the one speed: from 3 m/s, hard from 6");
	Check(PommelStrikeFor(s, Hand(1.0f, 1.0f)) == ShoveKind::None, "too slow: nothing");
	Check(PommelStrikeFor(s, Hand(3.5f, 0.9f)) == ShoveKind::Light, "fast, the pommel leading: they recoil");
	Check(PommelStrikeFor(s, Hand(6.5f, 0.9f)) == ShoveKind::Hard, "very fast: they go down");
	Check(PommelStrikeFor(s, Hand(5.0f, 0.2f)) == ShoveKind::None, "a slash with the pommel trailing: nothing");
	PommelHand bare = Hand(5.0f, 1.0f);
	bare.meleeDrawn = false;
	Check(PommelStrikeFor(s, bare) == ShoveKind::None, "no blade or blunt weapon drawn: nothing");
	PommelHand lost = Hand(5.0f, 1.0f);
	lost.valid = false;
	Check(PommelStrikeFor(s, lost) == ShoveKind::None, "the hand untracked: nothing");
	PommelSettings off;
	off.enabled = false;
	Check(PommelStrikeFor(off, Hand(5.0f, 1.0f)) == ShoveKind::None, "PommelStrikes=0: nothing");
}

}  // namespace

int main() {
	TestPoint();
	TestLead();
	TestStrike();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
