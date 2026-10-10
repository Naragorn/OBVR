// Checks the parry's decisions (game/ParryLogic.h): which actions are an
// attack, their blade from their weapon node, two moving blades meeting, the
// ledger of parried blows, and the hit handler's three answers - the block
// forced, the cone, the share - for a parried blow, another attacker's, and
// everyone else's.

#include <cstdio>
#include <limits>

#include "game/ParryLogic.h"

namespace {

using namespace obvr::game;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.01f) { return a - b < eps && b - a < eps; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();

void TestActions() {
	std::printf("An attack in flight\n");
	Check(IsAttackAction(2) && IsAttackAction(3), "the swing and its follow-through");
	Check(!IsAttackAction(-1) && !IsAttackAction(4) && !IsAttackAction(6) && !IsAttackAction(7),
	      "nothing, a bow, a block, a recoil: no");
}

void TestTheirBlade() {
	std::printf("Their blade\n");
	NiPoint3 a;
	NiPoint3 b;
	Check(TheirBladeFromNode(NiPoint3{100, 0, 50}, NiPoint3{0, 1, 0}, NiPoint3{100, 30, 50}, 35.0f, a, b) &&
	          Near(a.x, 100.0f) && Near(b.y, 65.0f),
	      "a sword: from the node along its axis to the bound's far end");
	Check(!TheirBladeFromNode(NiPoint3{0, 0, 0}, NiPoint3{0, 2, 0}, NiPoint3{0, 30, 0}, 35.0f, a, b),
	      "an axis that is no unit vector: none");
	Check(!TheirBladeFromNode(NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0}, NiPoint3{0, 0, 0}, 0.0f, a, b),
	      "no bound (an empty hand): none");
	Check(!TheirBladeFromNode(NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0}, NiPoint3{0, -20, 0}, 5.0f, a, b),
	      "a bound behind the hand: none");
	Check(!TheirBladeFromNode(NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0}, NiPoint3{0, 300, 0}, 50.0f, a, b),
	      "longer than any weapon: none");
	Check(!TheirBladeFromNode(NiPoint3{0, 0, 0}, NiPoint3{0, 1, 0}, NiPoint3{0, 30, 0}, kNaN, a, b),
	      "a bound that is not a number: none");
}

void TestMeet() {
	std::printf("Two blades meeting\n");
	float s = -1.0f;
	NiPoint3 p;
	// Ours held still across, from x -30 to 30 at y 40; theirs swinging down
	// through it.
	const NiPoint3 oa{-30, 40, 0};
	const NiPoint3 ob{30, 40, 0};
	Check(BladesMeet(oa, ob, oa, ob, NiPoint3{0, 40, 60}, NiPoint3{0, 40, 120}, NiPoint3{0, 40, -60},
	                 NiPoint3{0, 40, 0}, kParryReachUnits, s, p) &&
	          s > 0.0f && s < 1.0f && Near(p.x, 0.0f) && Near(p.y, 40.0f),
	      "theirs swung down through ours held still: met during the frame, at the middle of ours");
	Check(BladesMeet(oa, ob, oa, ob, NiPoint3{0, 41, -10}, NiPoint3{0, 41, 10}, NiPoint3{0, 41, -10},
	                 NiPoint3{0, 41, 10}, kParryReachUnits, s, p) &&
	          s == 0.0f,
	      "touching from the start: met at once");
	Check(!BladesMeet(oa, ob, oa, ob, NiPoint3{0, 60, 60}, NiPoint3{0, 60, 120}, NiPoint3{0, 60, -60},
	                  NiPoint3{0, 60, 0}, kParryReachUnits, s, p),
	      "theirs swung down 20 units beside ours: no");
	Check(BladesMeet(NiPoint3{-30, 0, 0}, NiPoint3{30, 0, 0}, NiPoint3{-30, 80, 0}, NiPoint3{30, 80, 0},
	                 NiPoint3{0, 40, -30}, NiPoint3{0, 40, 30}, NiPoint3{0, 40, -30}, NiPoint3{0, 40, 30},
	                 kParryReachUnits, s, p) &&
	          Near(s, 0.5f),
	      "ours swung through theirs held still: met half way");
}

void TestShield() {
	std::printf("The shield\n");
	float r = 0.0f;
	Check(ShieldBall(true, 30.0f, r) && Near(r, 24.0f), "a worn shield's bound: a ball a little smaller");
	Check(!ShieldBall(false, 30.0f, r), "no shield worn: none");
	Check(!ShieldBall(true, 4.0f, r), "a bound a bare forearm's size: none");
	Check(!ShieldBall(true, 200.0f, r) && !ShieldBall(true, kNaN, r), "a bound no shield has: none");
	float s = -1.0f;
	Check(BladeMeetsBall(NiPoint3{-20, 30, 60}, NiPoint3{20, 30, 60}, NiPoint3{-20, 30, -60}, NiPoint3{20, 30, -60},
	                     NiPoint3{0, 30, 0}, 24.0f, s) &&
	          Near(s, 0.5f),
	      "their blade swung down onto the shield: met half way");
	Check(!BladeMeetsBall(NiPoint3{-20, 80, 60}, NiPoint3{20, 80, 60}, NiPoint3{-20, 80, -60}, NiPoint3{20, 80, -60},
	                      NiPoint3{0, 30, 0}, 24.0f, s),
	      "swung down 50 units beside it: no");
}

void TestLedger() {
	std::printf("The parried\n");
	ParryLedger l;
	Check(l.Add(7, kParryWindowSeconds) && l.Has(7) && !l.Add(7, kParryWindowSeconds), "parried once");
	l.Step(0.5f);
	Check(l.Has(7), "half a second on: still");
	l.Step(0.4f);
	Check(!l.Has(7), "past the window: no longer");
	l.Add(7, kParryWindowSeconds);
	l.Add(8, kParryWindowSeconds);
	Check(l.Take(7) && !l.Has(7) && l.Has(8), "their blow came: taken, the other kept");
	Check(!l.Take(7), "taken twice: no");
	ParryLedger full;
	for (UInt32 a = 1; a <= kParryLedgerMax; ++a) {
		full.Add(a, 1.0f);
	}
	Check(!full.Add(99, 1.0f) && !full.Has(99), "never more than eight");
	l.Step(kNaN);
	Check(l.Has(8), "a time that is not a number takes nothing off");
}

void TestHitHandler() {
	std::printf("The hit handler's answers\n");
	Check(ForceBlockForParry(true, false, true), "the player, not blocking, someone parried: forced");
	Check(!ForceBlockForParry(true, true, true), "the player blocking already: the engine's yes");
	Check(!ForceBlockForParry(true, false, false), "nobody parried: the engine's no");
	Check(!ForceBlockForParry(false, false, true), "someone else hit: never");
	Check(ConeForParry(true, true, true) == ParryCone::Blocked, "the parried attacker: blocked");
	Check(ConeForParry(true, true, false) == ParryCone::Blocked,
	      "the parried attacker while the player blocks too: blocked whatever the cone");
	Check(ConeForParry(true, false, true) == ParryCone::NotBlocked,
	      "another attacker under a block forced for a parry: a hit");
	Check(ConeForParry(true, false, false) == ParryCone::Original, "a real block, nobody parried: the cone decides");
	Check(ConeForParry(false, true, true) == ParryCone::Original, "not the player: the cone decides");
	Check(Near(ParriedShare(true, true, 0.25f), 1.0f), "a parried blow: all of it blocked");
	Check(Near(ParriedShare(true, false, 0.25f), 0.25f), "with ParryStopsAll off: the engine's share");
	Check(Near(ParriedShare(false, true, 0.25f), 0.25f), "any other blow: the engine's share");
	Check(Near(ParryFatigueCost(10.0f), 10.0f) && ParryFatigueCost(-1.0f) == 0.0f && ParryFatigueCost(kNaN) == 0.0f &&
	          ParryFatigueCost(5000.0f) == 0.0f,
	      "the cost: as set, nothing for one out of reason");
}

void TestHeld() {
	std::printf("Someone held by the off hand\n");
	const UInt32 held = 0x1000;
	const UInt32 other = 0x2000;
	BlowStop s = BlowStopFor(true, false, held, held);
	Check(s.held && !s.parried, "the one held strikes: stopped as held");
	s = BlowStopFor(false, false, held, held);
	Check(s.held && !s.parried, "held with the parries off or no weapon drawn: still stopped");
	s = BlowStopFor(true, true, held, held);
	Check(s.held && !s.parried, "held and in a parry's window too: held (nothing owed for a parry)");
	s = BlowStopFor(true, true, held, other);
	Check(!s.held && s.parried, "another, parried, while someone is held: parried");
	s = BlowStopFor(false, true, held, other);
	Check(!s.held && !s.parried, "another in a window with the parries off: not stopped");
	s = BlowStopFor(true, false, 0, other);
	Check(!s.held && !s.parried, "nobody held, nobody parried: not stopped");
	Check(AnyBlowStopped(true, 1, 0), "a parry in its window: a block forced");
	Check(!AnyBlowStopped(false, 1, 0), "a window left over with the parries off: nothing forced");
	Check(AnyBlowStopped(false, 0, held), "someone held, the parries off: a block forced");
	Check(!AnyBlowStopped(true, 0, 0), "nothing at all: nothing forced");
	BlowStop h;
	h.held = true;
	BlowStop p;
	p.parried = true;
	Check(Near(StoppedShare(h, false, 0.25f), 1.0f), "held: all of the blow, ParryStopsAll off or not");
	Check(Near(StoppedShare(p, true, 0.25f), 1.0f), "parried with ParryStopsAll: all of it");
	Check(Near(StoppedShare(p, false, 0.25f), 0.25f), "parried without: the engine's share");
	Check(Near(StoppedShare(BlowStop{}, true, 0.25f), 0.25f), "neither: the engine's share");
}

}  // namespace

int main() {
	TestActions();
	TestTheirBlade();
	TestMeet();
	TestShield();
	TestLedger();
	TestHitHandler();
	TestHeld();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
