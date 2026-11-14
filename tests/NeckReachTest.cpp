#include <cmath>
#include <cstdio>
#include <limits>

#include "game/NeckReachLogic.h"

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

const float kNan = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();

void TestBound() {
	std::printf("an actor's bound against the reach\n");
	const NiPoint3 blade{1000.0f, 2000.0f, 300.0f};
	const float within = 160.0f;
	const float radius = 70.0f;
	const float reach = within + radius + kNeckBoundSlackUnits;
	Check(BoundCanHoldNeck(blade, radius, blade, within), "the bound around the blade itself");
	Check(BoundCanHoldNeck(NiPoint3{1000.0f + reach - 1.0f, 2000.0f, 300.0f}, radius, blade, within),
	      "a bound just inside the reach, its radius and the slack");
	Check(!BoundCanHoldNeck(NiPoint3{1000.0f + reach + 1.0f, 2000.0f, 300.0f}, radius, blade, within),
	      "a bound just outside them: no bone of it is in reach");
	Check(!BoundCanHoldNeck(NiPoint3{1000.0f, 2000.0f, 300.0f - reach - 1.0f}, radius, blade, within),
	      "the same below the blade");
	Check(!BoundCanHoldNeck(NiPoint3{-90000.0f, 40000.0f, 0.0f}, radius, blade, within), "an actor cells away");
	Check(BoundCanHoldNeck(NiPoint3{1000.0f + 5000.0f, 2000.0f, 300.0f}, 5000.0f, blade, within),
	      "a far centre with a radius that reaches the blade");

	std::printf("nothing to go by: the bones are asked\n");
	const NiPoint3 far{50000.0f, 50000.0f, 0.0f};
	Check(BoundCanHoldNeck(far, 0.0f, blade, within), "a bound of no radius (never set)");
	Check(BoundCanHoldNeck(far, -3.0f, blade, within), "a negative radius");
	Check(BoundCanHoldNeck(far, kNan, blade, within), "a radius that is no number");
	Check(BoundCanHoldNeck(far, kInf, blade, within), "an endless radius");
	Check(BoundCanHoldNeck(NiPoint3{kNan, 0.0f, 0.0f}, radius, blade, within), "a centre that is no number");
	Check(BoundCanHoldNeck(far, radius, NiPoint3{0.0f, kNan, 0.0f}, within), "a blade that is no number");
	Check(BoundCanHoldNeck(far, radius, blade, kNan), "a reach that is no number");
	Check(BoundCanHoldNeck(far, radius, blade, kInf), "an endless reach");
	Check(BoundCanHoldNeck(far, radius, blade, -1000.0f), "a reach below nothing");
}

// The rule must never leave out an actor whose head is in reach while the
// head is within the slack of the bound: made-up actors, the head anywhere
// in the grown bound, the blade anywhere within the reach of the head.
void TestNoHeadInReachLeftOut() {
	std::printf("a head within the slack of its bound and in reach is never left out\n");
	unsigned seed = 12345;
	auto next = [&seed]() {
		seed = seed * 1664525u + 1013904223u;
		return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;  // 0..1
	};
	auto direction = [&next]() {
		NiPoint3 d{next() - 0.5f, next() - 0.5f, next() - 0.5f};
		const float length = std::sqrt(d.LengthSquared());
		return length > 0.001f ? d * (1.0f / length) : NiPoint3{1.0f, 0.0f, 0.0f};
	};
	bool all = true;
	for (int i = 0; i < 20000; ++i) {
		const NiPoint3 centre{(next() - 0.5f) * 400000.0f, (next() - 0.5f) * 400000.0f, (next() - 0.5f) * 20000.0f};
		const float radius = 1.0f + next() * 300.0f;
		const float within = next() * 500.0f;
		// 0.999: the sums of a float this far from the origin are a unit coarse.
		const NiPoint3 head = centre + direction() * ((radius + kNeckBoundSlackUnits) * next() * 0.999f);
		const NiPoint3 blade = head + direction() * (within * next() * 0.999f);
		all = all && BoundCanHoldNeck(centre, radius, blade, within);
	}
	Check(all, "20000 made-up actors");
}

void TestAsk() {
	std::printf("who is asked for bones\n");
	const NiPoint3 blade{0.0f, 0.0f, 0.0f};
	const NiPoint3 near{100.0f, 0.0f, 60.0f};
	const NiPoint3 far{3000.0f, 0.0f, 60.0f};
	Check(AsksBones(near, 70.0f, blade, 160.0f, false) == NeckAsk::Yes, "an actor in reach");
	Check(AsksBones(near, 70.0f, blade, 160.0f, true) == NeckAsk::Yes, "the same in a call that checks the rule");
	Check(AsksBones(far, 70.0f, blade, 160.0f, false) == NeckAsk::No, "an actor out of reach: left out");
	Check(AsksBones(far, 70.0f, blade, 160.0f, true) == NeckAsk::YesToCheck,
	      "out of reach in a checking call: asked, to see the rule hold");
	Check(AsksBones(far, 0.0f, blade, 160.0f, false) == NeckAsk::Yes, "no bound to go by: asked");
}

void TestCheckDue() {
	std::printf("the calls that check the rule\n");
	Check(NeckRuleCheckDue(0), "the first call");
	bool none = true;
	for (UInt32 call = 1; call < kNeckRuleCheckedEvery; ++call) {
		none = none && !NeckRuleCheckDue(call);
	}
	Check(none, "none of the calls up to the next");
	Check(NeckRuleCheckDue(kNeckRuleCheckedEvery), "the next");
	Check(NeckRuleCheckDue(kNeckRuleCheckedEvery * 7), "every one after at that distance");
	Check(!NeckRuleCheckDue(kNeckRuleCheckedEvery * 7 + 1), "and not the call after it");
}

void TestOutside() {
	std::printf("how far outside its bound a head is\n");
	const NiPoint3 centre{10.0f, 20.0f, 30.0f};
	Check(std::fabs(HeadOutsideBound(centre, 50.0f, centre) + 50.0f) < 0.001f, "at the centre: inside by the radius");
	Check(std::fabs(HeadOutsideBound(centre, 50.0f, NiPoint3{60.0f, 20.0f, 30.0f})) < 0.001f, "on the sphere: 0");
	Check(std::fabs(HeadOutsideBound(centre, 50.0f, NiPoint3{10.0f, 20.0f, 110.0f}) - 30.0f) < 0.001f,
	      "30 units above the sphere");
	Check(HeadOutsideBound(centre, 0.0f, NiPoint3{10.0f, 20.0f, 110.0f}) == 0.0f, "a bound of no radius: 0");
	Check(HeadOutsideBound(centre, kNan, NiPoint3{10.0f, 20.0f, 110.0f}) == 0.0f, "a radius of no number: 0");
	Check(HeadOutsideBound(NiPoint3{kNan, 0.0f, 0.0f}, 50.0f, centre) == 0.0f, "a centre of no number: 0");
	Check(HeadOutsideBound(centre, 50.0f, NiPoint3{kInf, 0.0f, 0.0f}) == 0.0f, "a head at no place: 0");
}

void TestCounts() {
	std::printf("the rule's counts for its line\n");
	NeckRule rule;
	Check(BeginNeckCall(rule, 40), "the first call checks the rule");
	Check(rule.calls == 1 && rule.checks == 1 && rule.loaded == 40, "counted: a call, a check, its loaded actors");
	Check(!BeginNeckCall(rule, 42), "the second does not");
	Check(rule.calls == 2 && rule.checks == 1 && rule.loaded == 82, "counted: a call and its actors, no check");

	NoteNeckTaken(rule, -12.0f, false);
	Check(rule.taken == 1 && rule.missed == 0 && rule.mostOutside == -12.0f,
	      "a head inside its bound: the first sets the most");
	NoteNeckTaken(rule, -30.0f, false);
	Check(rule.taken == 2 && rule.mostOutside == -12.0f, "one deeper inside leaves it");
	NoteNeckTaken(rule, 25.0f, false);
	Check(rule.taken == 3 && rule.mostOutside == 25.0f && rule.missed == 0, "one outside raises it");
	NoteNeckTaken(rule, 260.0f, true);
	Check(rule.taken == 4 && rule.missed == 1 && rule.mostOutside == 260.0f, "one the bound had left out: a miss");

	std::printf("when the line is due\n");
	NeckRule lines;
	Check(!NeckRuleLineDue(lines), "not before a call");
	lines.calls = kNeckRuleFirstLineAfter - 1;
	Check(!NeckRuleLineDue(lines), "not one call before the first line");
	lines.calls = kNeckRuleFirstLineAfter;
	Check(NeckRuleLineDue(lines), "the first line early");
	lines.checks = 3;
	lines.loaded = 900;
	lines.asked = 12;
	lines.taken = 5;
	lines.missed = 1;
	lines.mostOutside = 44.0f;
	NeckRuleLineSaid(lines);
	Check(lines.said == kNeckRuleFirstLineAfter && lines.calls == kNeckRuleFirstLineAfter, "said: the calls kept");
	Check(lines.checks == 0 && lines.loaded == 0 && lines.asked == 0 && lines.taken == 0 && lines.missed == 0 &&
	          lines.mostOutside == 0.0f,
	      "the counts since the line cleared");
	Check(!NeckRuleLineDue(lines), "not due right after");
	lines.calls = kNeckRuleFirstLineAfter + kNeckRuleLineEvery - 1;
	Check(!NeckRuleLineDue(lines), "not one call before the next");
	lines.calls = kNeckRuleFirstLineAfter + kNeckRuleLineEvery;
	Check(NeckRuleLineDue(lines), "the next after the long distance");
	NoteNeckTaken(lines, -3.0f, false);
	Check(lines.mostOutside == -3.0f, "after a line the first taken head sets the most again");
}

void TestCollectedOnce() {
	std::printf("the necks of a frame collected once\n");
	Check(NecksStillToCollect(kNecksNotCollected), "not collected by the blade's step: collected at the frame's end");
	Check(!NecksStillToCollect(0), "collected, none in reach: not again");
	Check(!NecksStillToCollect(8), "collected, eight: not again");
}

}  // namespace

int main() {
	TestBound();
	TestNoHeadInReachLeftOut();
	TestAsk();
	TestCheckDue();
	TestOutside();
	TestCounts();
	TestCollectedOnce();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
