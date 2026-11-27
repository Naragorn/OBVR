// Checks the blow's shield rule while the shield guards by contact
// (game/BlockAngleLogic.h ShieldByContactVerdict, game/ShieldPlateLogic.h;
// Nadi, 2026-11-27: blocking "oddly too generous"): the distance to the
// shield's own outline as a plate, not the circle of its longer spoke; its
// own reach (BlockShieldReachUnits); the shield standing between the blow
// and the chest (BlockShieldInterposeUnits) as the test for creatures and as
// a second yes for blades; the circle when no outline was read; and the
// shield by the pose (ShieldBlockByContact=0) exactly as before.

#include <cmath>
#include <cstdio>
#include <string_view>

#include "game/BlockAngleLogic.h"

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

template <typename HalfWidth>
ShieldOutline Shape(float low, float high, HalfWidth halfWidth) {
	OutlineBins bins;
	for (float a = low; a <= high + 1e-3f; a += 1.0f) {
		const float w = halfWidth(a);
		bins.Add(a, -w);
		bins.Add(a, w);
		bins.Add(a, 0.0f);
	}
	return OutlineFromBins(bins, low, high, 0.0f);
}

// A kite 80 long, tip down: 25 (half) wide from 10 up to the top, to a point
// at -40. A tower 80 long, 15 (half) wide.
float Kite(float a) { return a >= 10.0f ? 25.0f : 25.0f * (a + 40.0f) / 50.0f; }
float Tower(float) { return 15.0f; }

const ShieldOutline& KiteOutline() {
	static const ShieldOutline o = Shape(-40.0f, 40.0f, Kite);
	return o;
}
const ShieldOutline& TowerOutline() {
	static const ShieldOutline o = Shape(-40.0f, 40.0f, Tower);
	return o;
}

// The shield at `centre`, its face ahead (+y), its first axis up (+z) and
// its second +x, 6 thick, by contact; the player's chest behind it, from
// the neck (0, -15, 130) to the belly (0, -15, 80). An attacker ahead.
BlockGuard Shield(const ShieldOutline* outline, NiPoint3 centre = NiPoint3{0.0f, 10.0f, 100.0f}) {
	BlockGuard g;
	g.with = BlockWith::Shield;
	g.shieldValid = true;
	g.shieldCentre = centre;
	g.shieldNormal = NiPoint3{0.0f, 1.0f, 0.0f};
	g.shieldRadius = 40.0f;  // the longer spoke
	g.shieldHalfThick = 3.0f;
	g.shieldAxisA = NiPoint3{0.0f, 0.0f, 1.0f};
	g.shieldAxisB = NiPoint3{1.0f, 0.0f, 0.0f};
	g.shieldOutline = outline;
	g.shieldPose.byContact = true;
	g.bodyValid = true;
	g.body = NiPoint3{0.0f, -15.0f, 130.0f};
	g.bodyLow = NiPoint3{0.0f, -15.0f, 80.0f};
	return g;
}

BlockBlow Blade(const NiPoint3& a, const NiPoint3& b) {
	BlockBlow blow;
	blow.bladeValid = true;
	blow.bladeA = a;
	blow.bladeB = b;
	blow.centreValid = true;
	blow.centre = NiPoint3{0.0f, 120.0f, 90.0f};
	blow.centreRadius = 40.0f;
	return blow;
}

BlockBlow Creature(const NiPoint3& centre, float radius) {
	BlockBlow blow;
	blow.centreValid = true;
	blow.centre = centre;
	blow.centreRadius = radius;
	return blow;
}

void TestBladeByOutline() {
	std::printf("A blade against the outline\n");
	const BlockAngleSettings s;
	BlockGuard kite = Shield(&KiteOutline());
	// Beside the kite's tip, 30 ahead: within the long spoke's circle and
	// its old reach of 40 - blocked by the circle - 40 off the outline now.
	const BlockBlow besideTip = Blade(NiPoint3{34.0f, 40.0f, 72.0f}, NiPoint3{34.0f, 40.0f, 52.0f});
	BlockAngleVerdict v = BlockAngleFor(s, kite, besideTip);
	Check(v.verdict == BlockAngle::Through && v.across, "a blade beside the kite's tip: through (the circle: blocked)");
	BlockGuard pose = kite;
	pose.shieldPose.byContact = false;
	Check(BlockAngleFor(s, pose, besideTip).verdict == BlockAngle::Blocked,
	      "the shield by the pose: the circle and the reach of 40, blocked as before");
	v = BlockAngleFor(s, kite, Blade(NiPoint3{5.0f, 30.0f, 135.0f}, NiPoint3{-5.0f, 30.0f, 125.0f}));
	Check(v.verdict == BlockAngle::Blocked && std::fabs(v.units - 17.0f) < 1.5f,
	      "a blade before the kite's wide top, 17 off its face: blocked");
	BlockGuard tower = Shield(&TowerOutline());
	const BlockBlow besideSide = Blade(NiPoint3{45.0f, 30.0f, 110.0f}, NiPoint3{45.0f, 30.0f, 90.0f});
	Check(BlockAngleFor(s, tower, besideSide).verdict == BlockAngle::Through,
	      "a blade beside the tower's side: through (the circle: blocked)");
	pose = tower;
	pose.shieldPose.byContact = false;
	Check(BlockAngleFor(s, pose, besideSide).verdict == BlockAngle::Blocked, "the same by the pose: blocked as before");
}

void TestReach() {
	std::printf("The shield's own reach\n");
	BlockAngleSettings s;
	BlockGuard g = Shield(&TowerOutline());
	g.bodyValid = false;  // the distance alone
	const auto at = [](float off) {
		return Blade(NiPoint3{-5.0f, 13.0f + off, 100.0f}, NiPoint3{5.0f, 13.0f + off, 100.0f});
	};
	Check(BlockAngleFor(s, g, at(s.shieldReachUnits - 0.5f)).verdict == BlockAngle::Blocked, "just within the reach: blocked");
	Check(BlockAngleFor(s, g, at(s.shieldReachUnits + 0.5f)).verdict == BlockAngle::Through, "just past it: through");
	s.shieldReachUnits = 0.0f;
	Check(BlockAngleFor(s, g, at(60.0f)).verdict == BlockAngle::Blocked, "a reach of 0 asks no distance");
	s.shieldReachUnits = std::nanf("");
	Check(BlockAngleFor(s, g, at(BlockAngleSettings{}.shieldReachUnits + 0.5f)).verdict == BlockAngle::Through,
	      "a reach that is no number: the default");
	s = BlockAngleSettings{};
	s.reachUnits = 200.0f;  // the weapon's reach has no say on the shield by contact
	Check(BlockAngleFor(s, g, at(30.0f)).verdict == BlockAngle::Through, "BlockReachUnits does not widen the shield's");
}

void TestBetween() {
	std::printf("The shield between the blow and the chest\n");
	const BlockAngleSettings s;
	// A creature low before the player, the shield held up and aside: it
	// faces the creature within 70 degrees and its ball comes within 40 of
	// the circle (blocked before) - but it is not between.
	BlockGuard aside = Shield(&KiteOutline(), NiPoint3{45.0f, 10.0f, 100.0f});
	const BlockBlow wolf = Creature(NiPoint3{0.0f, 40.0f, 70.0f}, 30.0f);
	BlockAngleVerdict v = BlockAngleFor(s, aside, wolf);
	Check(v.verdict == BlockAngle::Through && v.across && v.betweenUnits > 6.0f,
	      "a creature, the shield facing it but held aside: through (before: blocked)");
	BlockGuard pose = aside;
	pose.shieldPose.byContact = false;
	Check(BlockAngleFor(s, pose, wolf).verdict == BlockAngle::Blocked, "the same by the pose: blocked as before");
	BlockGuard front = Shield(&KiteOutline());
	v = BlockAngleFor(s, front, Creature(NiPoint3{0.0f, 40.0f, 90.0f}, 30.0f));
	Check(v.verdict == BlockAngle::Blocked && v.betweenUnits == 0.0f, "a creature with the shield between: blocked");
	// A blade 40 off the plate at the blow but the shield squarely between
	// its middle and the chest: blocked by the second yes.
	v = BlockAngleFor(s, front, Blade(NiPoint3{0.0f, 60.0f, 115.0f}, NiPoint3{0.0f, 60.0f, 95.0f}));
	Check(v.verdict == BlockAngle::Blocked && v.units > 25.0f && v.betweenUnits >= 0.0f,
	      "a blade still 40 away, the shield between it and the chest: blocked");
	// The margin past the outline.
	BlockAngleSettings wide;
	wide.shieldInterposeUnits = 60.0f;
	Check(BlockAngleFor(wide, aside, wolf).verdict == BlockAngle::Blocked, "a wider margin takes the aside shield");
	// No chest read: the creature's ball within the shield's reach of the plate.
	BlockGuard noBody = aside;
	noBody.bodyValid = false;
	v = BlockAngleFor(s, noBody, wolf);
	Check(v.verdict == BlockAngle::Blocked && v.betweenUnits < 0.0f && v.units < 25.0f,
	      "no chest: the ball's distance to the plate (18), within the reach: blocked");
}

void TestCircleWithoutOutline() {
	std::printf("No outline: the circle, with the shield's own numbers\n");
	const BlockAngleSettings s;
	BlockGuard g = Shield(nullptr);
	const BlockBlow besideTip = Blade(NiPoint3{34.0f, 40.0f, 72.0f}, NiPoint3{34.0f, 40.0f, 52.0f});
	BlockAngleVerdict v = BlockAngleFor(s, g, besideTip);
	Check(v.verdict == BlockAngle::Blocked && std::fabs(v.units - 27.3f) < 1.5f,
	      "the circle: 27 off it, past the reach - but the wide circle stands between: blocked");
	ShieldOutline invalid;
	g.shieldOutline = &invalid;
	Check(BlockAngleFor(s, g, besideTip).units == v.units, "an outline not read: the circle the same");
	Check(BlockAngleFor(s, Shield(nullptr), Creature(NiPoint3{0.0f, 40.0f, 90.0f}, 30.0f)).verdict ==
	          BlockAngle::Blocked,
	      "a creature with the circle between: blocked");
}

void TestTests() {
	std::printf("What decided, for the log\n");
	const BlockAngleSettings s;
	BlockAngleVerdict v = BlockAngleFor(s, Shield(&KiteOutline()),
	                                    Blade(NiPoint3{5.0f, 30.0f, 135.0f}, NiPoint3{-5.0f, 30.0f, 125.0f}));
	Check(v.test != nullptr && std::string_view(v.test).find("outline") != std::string_view::npos,
	      "the outline's distance named");
	v = BlockAngleFor(s, Shield(nullptr), Creature(NiPoint3{0.0f, 40.0f, 90.0f}, 30.0f));
	Check(std::string_view(v.test).find("between") != std::string_view::npos &&
	          std::string_view(v.test).find("circle") != std::string_view::npos,
	      "between, by the circle, named");
	BlockGuard pose = Shield(&KiteOutline());
	pose.shieldPose.byContact = false;
	Check(std::string_view(BlockAngleFor(s, pose, Creature(NiPoint3{0.0f, 40.0f, 90.0f}, 30.0f)).test).empty(),
	      "by the pose: nothing new named");
}

}  // namespace

int main() {
	TestBladeByOutline();
	TestReach();
	TestBetween();
	TestCircleWithoutOutline();
	TestTests();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
