// Checks the block's angle rule (game/BlockAngleLogic.h): a blow at the
// player's weapon guard counts as blocked only when the blades cross near a
// right angle and meet - within the reach of each other at the blow - at the
// shield only when its face is turned to the blow and the blade comes within
// the reach of the disc; the fallbacks when the attacker has no blade, the
// reach as a setting (0 asks no distance), and every way the rule leaves the
// engine's own verdict alone.

#include <cstdio>
#include <limits>

#include "game/BlockAngleLogic.h"

using namespace obvr::game;
using obvr::NiPoint3;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s: %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.5f) { return a - b < eps && b - a < eps; }

// The player's blade held level across the body (along x), at chest height.
BlockGuard WeaponGuard() {
	BlockGuard g;
	g.with = BlockWith::Weapon;
	g.bladeValid = true;
	g.bladeA = NiPoint3{-20.0f, 0.0f, 100.0f};
	g.bladeB = NiPoint3{20.0f, 0.0f, 100.0f};
	return g;
}

// The shield's face turned ahead (+y), at chest height: a disc 20 units
// round and 12 thick, as the Arena Iron Shield reads.
BlockGuard ShieldGuard() {
	BlockGuard g;
	g.with = BlockWith::Shield;
	g.shieldValid = true;
	g.shieldCentre = NiPoint3{0.0f, 10.0f, 100.0f};
	g.shieldNormal = NiPoint3{0.0f, 1.0f, 0.0f};
	g.shieldRadius = 20.0f;
	g.shieldHalfThick = 6.0f;
	return g;
}

// An attacker ahead, their blade from `a` along `dir` for 50 units; their
// body a ball of 40 units ahead and a little below the guard.
BlockBlow Blade(const NiPoint3& a, const NiPoint3& dir) {
	BlockBlow b;
	b.bladeValid = true;
	b.bladeA = a;
	b.bladeB = a + dir * 50.0f;
	b.centreValid = true;
	b.centre = NiPoint3{0.0f, 100.0f, 60.0f};
	b.centreRadius = 40.0f;
	return b;
}

// Something without a blade: a body of `radius` about `centre`.
BlockBlow Body(const NiPoint3& centre, float radius = 40.0f) {
	BlockBlow b;
	b.centreValid = true;
	b.centre = centre;
	b.centreRadius = radius;
	return b;
}

void TestLineAngle() {
	std::printf("The angle between two lines\n");
	Check(Near(LineAngleDegrees(NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}), 90.0f), "across: 90");
	Check(Near(LineAngleDegrees(NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{-3.0f, 0.0f, 0.0f}), 0.0f),
	      "along, either way round: 0");
	Check(Near(LineAngleDegrees(NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{1.0f, 1.0f, 0.0f}), 45.0f), "diagonal: 45");
	Check(Near(LineAngleDegrees(NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{-1.0f, 1.0f, 0.0f}), 45.0f),
	      "the obtuse way round is the same line: 45");
	Check(!(LineAngleDegrees(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{1.0f, 0.0f, 0.0f}) ==
	        LineAngleDegrees(NiPoint3{0.0f, 0.0f, 0.0f}, NiPoint3{1.0f, 0.0f, 0.0f})),
	      "a line with no length: not a number");
}

void TestGaps() {
	std::printf("The gap from a body to the guard\n");
	const NiPoint3 a{-20.0f, 0.0f, 100.0f};
	const NiPoint3 b{20.0f, 0.0f, 100.0f};
	Check(Near(BallSegmentGap(NiPoint3{0.0f, 60.0f, 100.0f}, 40.0f, a, b), 20.0f), "a ball 60 off, 40 round: 20");
	Check(Near(BallSegmentGap(NiPoint3{0.0f, 30.0f, 100.0f}, 40.0f, a, b), 0.0f), "a ball reaching the blade: 0");
	BladeDisc d;
	d.centre = NiPoint3{0.0f, 10.0f, 100.0f};
	d.normal = NiPoint3{0.0f, 1.0f, 0.0f};
	d.radius = 20.0f;
	d.halfThick = 6.0f;
	Check(Near(BallDiscGap(NiPoint3{0.0f, 76.0f, 100.0f}, 40.0f, d), 20.0f), "a ball ahead of the disc's face: 20");
	Check(Near(BallDiscGap(NiPoint3{0.0f, 16.0f, 100.0f}, 40.0f, d), 0.0f), "a ball on the disc: 0");
}

void TestWeapon() {
	std::printf("The weapon guard against a blade\n");
	BlockAngleSettings s;
	const BlockGuard g = WeaponGuard();
	// An overhead chop: their blade pointing down and towards the player,
	// ending 25 units off the guard.
	BlockAngleVerdict v = BlockAngleFor(s, g, Blade(NiPoint3{0.0f, 60.0f, 140.0f}, NiPoint3{0.0f, -0.7f, -0.7f}));
	Check(v.verdict == BlockAngle::Blocked && Near(v.degrees, 90.0f), "a chop across the level blade: blocked, 90");
	Check(v.across && v.met && Near(v.units, 25.5f), "the blades crossing and meeting, 25 units off");
	// A sideways slash: their blade along the player's, right at it.
	v = BlockAngleFor(s, g, Blade(NiPoint3{-30.0f, 5.0f, 100.0f}, NiPoint3{1.0f, 0.0f, 0.0f}));
	Check(v.verdict == BlockAngle::Through && Near(v.degrees, 0.0f), "a slash along the blade: through, 0");
	Check(!v.across && v.met && Near(v.units, 5.0f), "met, but not across");
	// A diagonal cut, 45 degrees to the blade, passing 10 units ahead of it.
	const BlockBlow diagonal = Blade(NiPoint3{-30.0f, 10.0f, 130.0f}, NiPoint3{0.7071f, 0.0f, -0.7071f});
	v = BlockAngleFor(s, g, diagonal);
	Check(v.verdict == BlockAngle::Through && Near(v.degrees, 45.0f), "a diagonal cut, 30 degrees of slack: through");
	s.bladeDegrees = 50.0f;
	v = BlockAngleFor(s, g, diagonal);
	Check(v.verdict == BlockAngle::Blocked, "with 50 degrees of slack: blocked");
	Check(v.why != nullptr && v.why[0] != '\0', "with a reason given");
}

void TestWeaponReach() {
	std::printf("The weapon guard has to meet the blade (the tester: the height of the crossing blades)\n");
	BlockAngleSettings s;
	// The guard held over the head: 80 units above the chest.
	BlockGuard high = WeaponGuard();
	high.bladeA.z += 80.0f;
	high.bladeB.z += 80.0f;
	// A chop at the chest, crossing the high blade's line at a right angle
	// but passing far under it.
	const BlockBlow chop = Blade(NiPoint3{0.0f, 60.0f, 140.0f}, NiPoint3{0.0f, -0.7f, -0.7f});
	BlockAngleVerdict v = BlockAngleFor(s, high, chop);
	Check(v.verdict == BlockAngle::Through && v.across && !v.met, "crossing at 90 but 71 units under the guard: through");
	Check(Near(v.units, 70.7f), "the distance measured between the blades");
	s.reachUnits = 100.0f;
	v = BlockAngleFor(s, high, chop);
	Check(v.verdict == BlockAngle::Blocked && v.met, "within a reach of 100: blocked");
	s.reachUnits = 0.0f;
	v = BlockAngleFor(s, high, chop);
	Check(v.verdict == BlockAngle::Blocked && v.met && Near(v.units, 70.7f),
	      "a reach of 0 asks no distance: blocked, the distance still measured");
	s.reachUnits = 40.0f;
	// A sideways slash at the belly, 70 under the level guard: along it, far
	// from it - through on both counts.
	v = BlockAngleFor(s, WeaponGuard(), Blade(NiPoint3{-30.0f, 5.0f, 30.0f}, NiPoint3{1.0f, 0.0f, 0.0f}));
	Check(v.verdict == BlockAngle::Through && !v.across && !v.met, "neither across nor met: through");
}

void TestWeaponWithoutTheirBlade() {
	std::printf("The weapon guard against something without a blade\n");
	BlockAngleSettings s;
	const BlockGuard g = WeaponGuard();
	BlockAngleVerdict v = BlockAngleFor(s, g, Body(NiPoint3{0.0f, 70.0f, 100.0f}));  // straight ahead, 30 off
	Check(v.verdict == BlockAngle::Blocked && Near(v.degrees, 90.0f),
	      "a creature ahead, the blade across its way: blocked");
	Check(v.across && v.met && Near(v.units, 30.0f), "its body 30 units off the blade");
	v = BlockAngleFor(s, g, Body(NiPoint3{70.0f, 0.0f, 100.0f}));  // from the side, along the blade
	Check(v.verdict == BlockAngle::Through && Near(v.degrees, 0.0f), "from the side, along the blade: through");
	// A wolf at the legs, the blade held at the chest: across its way, but
	// the body nowhere near the blade.
	v = BlockAngleFor(s, g, Body(NiPoint3{0.0f, 60.0f, 30.0f}, 25.0f));
	Check(v.verdict == BlockAngle::Through && v.across && !v.met && Near(v.units, 67.2f, 1.0f),
	      "a wolf at the legs under the chest guard: through, 67 units off");
	s.reachUnits = 0.0f;
	v = BlockAngleFor(s, g, Body(NiPoint3{0.0f, 60.0f, 30.0f}, 25.0f));
	Check(v.verdict == BlockAngle::Blocked, "with no reach asked: blocked by the angle alone");
	s.reachUnits = 40.0f;
	v = BlockAngleFor(s, g, Body(NiPoint3{0.0f, 0.0f, 100.0f}));  // on the blade itself
	Check(v.verdict == BlockAngle::Engine, "standing in the blade, no way to tell: the engine's");
	BlockBlow nothing;
	v = BlockAngleFor(s, g, nothing);
	Check(v.verdict == BlockAngle::Engine, "neither their blade nor where they are: the engine's");
}

void TestShield() {
	std::printf("The shield against a blow\n");
	BlockAngleSettings s;
	const BlockGuard g = ShieldGuard();
	// Their blade ahead of the shield's face, ending on it.
	BlockAngleVerdict v = BlockAngleFor(s, g, Blade(NiPoint3{0.0f, 60.0f, 100.0f}, NiPoint3{0.0f, -1.0f, 0.0f}));
	Check(v.verdict == BlockAngle::Blocked && Near(v.degrees, 0.0f), "the blade ahead of the face: blocked, 0 off");
	Check(v.across && v.met && Near(v.units, 0.0f), "the blade on the disc");
	// Their blade's middle 45 degrees off the face, passing 33 units from
	// the disc's rim.
	v = BlockAngleFor(s, g, Blade(NiPoint3{35.0f, 45.0f, 75.0f}, NiPoint3{0.0f, 0.0f, 1.0f}));
	Check(v.verdict == BlockAngle::Blocked && Near(v.degrees, 45.0f, 2.0f), "45 off, 70 allowed: blocked");
	Check(v.met && Near(v.units, 32.6f, 1.0f), "33 units from the rim, within the reach");
	// Their blade beside the shield, edge on.
	v = BlockAngleFor(s, g, Blade(NiPoint3{100.0f, 10.0f, 75.0f}, NiPoint3{0.0f, 0.0f, 1.0f}));
	Check(v.verdict == BlockAngle::Through && Near(v.degrees, 90.0f), "edge on: through, 90 off");
	s.shieldDegrees = 30.0f;
	v = BlockAngleFor(s, g, Blade(NiPoint3{35.0f, 45.0f, 75.0f}, NiPoint3{0.0f, 0.0f, 1.0f}));
	Check(v.verdict == BlockAngle::Through && !v.across && v.met, "45 off, 30 allowed: through");
	// The face's other side counts as the face: a disc has two.
	BlockGuard flipped = g;
	flipped.shieldNormal = NiPoint3{0.0f, -1.0f, 0.0f};
	s.shieldDegrees = 70.0f;
	v = BlockAngleFor(s, flipped, Blade(NiPoint3{0.0f, 60.0f, 100.0f}, NiPoint3{0.0f, -1.0f, 0.0f}));
	Check(v.verdict == BlockAngle::Blocked, "the normal the other way round: the same face, blocked");
	// A normal of any length is the same face.
	BlockGuard longNormal = g;
	longNormal.shieldNormal = NiPoint3{0.0f, 3.0f, 0.0f};
	v = BlockAngleFor(s, longNormal, Blade(NiPoint3{0.0f, 60.0f, 100.0f}, NiPoint3{0.0f, -1.0f, 0.0f}));
	Check(v.verdict == BlockAngle::Blocked && Near(v.units, 0.0f), "a normal three long: the blade still on the disc");
	// No blade: where they stand.
	v = BlockAngleFor(s, g, Body(NiPoint3{0.0f, 60.0f, 80.0f}));  // ahead, a little below, 4 off the disc
	Check(v.verdict == BlockAngle::Blocked && v.degrees < 30.0f, "a creature ahead, a little below: blocked");
	Check(v.met && Near(v.units, 4.0f, 1.0f), "its body 4 units off the disc");
	v = BlockAngleFor(s, g, Body(NiPoint3{100.0f, 10.0f, 100.0f}));
	Check(v.verdict == BlockAngle::Through, "a creature beside the shield: through");
	v = BlockAngleFor(s, g, Body(g.shieldCentre));
	Check(v.verdict == BlockAngle::Engine, "at the shield's own centre: the engine's");
}

void TestShieldReach() {
	std::printf("The shield has to be met too\n");
	BlockAngleSettings s;
	const BlockGuard g = ShieldGuard();
	// A cut at the belly, 70 units under the shield, its blade's middle
	// still within 70 degrees of the face.
	const BlockBlow low = Blade(NiPoint3{-25.0f, 40.0f, 30.0f}, NiPoint3{1.0f, 0.0f, 0.0f});
	BlockAngleVerdict v = BlockAngleFor(s, g, low);
	Check(v.verdict == BlockAngle::Through && v.across && !v.met, "the face to it, but the blade 56 under the shield: through");
	Check(Near(v.units, 55.5f, 1.0f), "the distance from the blade to the disc, its radius and thickness counted");
	s.reachUnits = 0.0f;
	v = BlockAngleFor(s, g, low);
	Check(v.verdict == BlockAngle::Blocked, "with no reach asked: blocked by the face alone");
	s.reachUnits = 40.0f;
	// A wolf at the legs under the raised shield, 58 units off the disc.
	v = BlockAngleFor(s, g, Body(NiPoint3{0.0f, 60.0f, 10.0f}, 25.0f));
	Check(v.verdict == BlockAngle::Through && v.across && !v.met && Near(v.units, 57.7f, 1.0f),
	      "a wolf under the shield: through");
}

void TestLeftToTheEngine() {
	std::printf("When the rule leaves the engine's verdict alone\n");
	BlockAngleSettings s;
	const BlockBlow blow = Blade(NiPoint3{0.0f, 60.0f, 140.0f}, NiPoint3{0.0f, -0.7f, -0.7f});
	s.enabled = false;
	Check(BlockAngleFor(s, WeaponGuard(), blow).verdict == BlockAngle::Engine, "switched off");
	s.enabled = true;
	BlockGuard g = WeaponGuard();
	g.with = BlockWith::None;
	Check(BlockAngleFor(s, g, blow).verdict == BlockAngle::Engine, "no guard of ours (the engine's own block)");
	g.with = BlockWith::Fists;
	Check(BlockAngleFor(s, g, blow).verdict == BlockAngle::Engine, "the fists: as the game has it");
	g.with = BlockWith::Bow;
	Check(BlockAngleFor(s, g, blow).verdict == BlockAngle::Engine, "the bow: as the game has it");
	g = WeaponGuard();
	g.bladeValid = false;
	Check(BlockAngleFor(s, g, blow).verdict == BlockAngle::Engine, "our blade not read (the fists raised as a guard)");
	g = WeaponGuard();
	g.bladeB = g.bladeA;
	Check(BlockAngleFor(s, g, blow).verdict == BlockAngle::Engine, "our blade with no length");
	BlockBlow flat = blow;
	flat.bladeB = flat.bladeA;
	flat.centreValid = false;
	Check(BlockAngleFor(s, WeaponGuard(), flat).verdict == BlockAngle::Engine, "their blade with no length, no centre");
	BlockGuard shield = ShieldGuard();
	shield.shieldValid = false;
	Check(BlockAngleFor(s, shield, blow).verdict == BlockAngle::Engine, "the shield's disc not read");
	shield = ShieldGuard();
	shield.shieldNormal = NiPoint3{0.0f, 0.0f, 0.0f};
	Check(BlockAngleFor(s, shield, blow).verdict == BlockAngle::Engine, "a shield normal with no length");
	shield = ShieldGuard();
	shield.shieldRadius = 0.0f;
	Check(BlockAngleFor(s, shield, blow).verdict == BlockAngle::Engine, "a shield disc without a size");
	BlockBlow nothing;
	Check(BlockAngleFor(s, ShieldGuard(), nothing).verdict == BlockAngle::Engine,
	      "the shield, neither their blade nor where they are");
}

void TestSlack() {
	std::printf("The slack, as a setting\n");
	Check(Near(BlockSlackDegrees(30.0f, 45.0f), 30.0f), "a plain value is taken");
	Check(Near(BlockSlackDegrees(200.0f, 45.0f), 90.0f), "over 90: 90, everything passes");
	Check(Near(BlockSlackDegrees(-5.0f, 45.0f), 0.0f), "under 0: 0, only the exact angle");
	Check(Near(BlockSlackDegrees(std::numeric_limits<float>::quiet_NaN(), 45.0f), 45.0f),
	      "not a number: the default");
	BlockAngleSettings s;
	s.bladeDegrees = 200.0f;
	const BlockGuard g = WeaponGuard();
	Check(BlockAngleFor(s, g, Blade(NiPoint3{-30.0f, 5.0f, 100.0f}, NiPoint3{1.0f, 0.0f, 0.0f})).verdict ==
	          BlockAngle::Blocked,
	      "90 degrees of slack: even a slash along the blade is blocked");
}

void TestReachSetting() {
	std::printf("The reach, as a setting\n");
	Check(Near(BlockReachUnits(40.0f, 40.0f), 40.0f), "a plain value is taken");
	Check(Near(BlockReachUnits(0.0f, 40.0f), 0.0f), "0: no distance asked");
	Check(Near(BlockReachUnits(-5.0f, 40.0f), 0.0f), "under 0: the same as 0");
	Check(Near(BlockReachUnits(std::numeric_limits<float>::quiet_NaN(), 40.0f), 40.0f), "not a number: the default");
	BlockAngleSettings s;
	s.reachUnits = std::numeric_limits<float>::quiet_NaN();
	BlockGuard high = WeaponGuard();
	high.bladeA.z += 80.0f;
	high.bladeB.z += 80.0f;
	Check(BlockAngleFor(s, high, Blade(NiPoint3{0.0f, 60.0f, 140.0f}, NiPoint3{0.0f, -0.7f, -0.7f})).verdict ==
	          BlockAngle::Through,
	      "a reach that is none: the default's 40, the high guard through");
}

}  // namespace

int main() {
	TestLineAngle();
	TestGaps();
	TestWeapon();
	TestWeaponReach();
	TestWeaponWithoutTheirBlade();
	TestShield();
	TestShieldReach();
	TestLeftToTheEngine();
	TestSlack();
	TestReachSetting();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
