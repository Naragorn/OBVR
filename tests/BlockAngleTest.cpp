// Checks the block's angle rule (game/BlockAngleLogic.h): a blow at the
// player's weapon guard counts as blocked only when the blades cross near a
// right angle, at the shield only when its face is turned to the blow; the
// fallbacks when the attacker has no blade, and every way the rule leaves
// the engine's own verdict alone.

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

// The shield's face turned ahead (+y), at chest height.
BlockGuard ShieldGuard() {
	BlockGuard g;
	g.with = BlockWith::Shield;
	g.shieldValid = true;
	g.shieldCentre = NiPoint3{0.0f, 10.0f, 100.0f};
	g.shieldNormal = NiPoint3{0.0f, 1.0f, 0.0f};
	return g;
}

// An attacker ahead, their blade from `a` along `dir` for 50 units.
BlockBlow Blade(const NiPoint3& a, const NiPoint3& dir) {
	BlockBlow b;
	b.bladeValid = true;
	b.bladeA = a;
	b.bladeB = a + dir * 50.0f;
	b.centreValid = true;
	b.centre = NiPoint3{0.0f, 100.0f, 60.0f};
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

void TestWeapon() {
	std::printf("The weapon guard against a blade\n");
	BlockAngleSettings s;
	const BlockGuard g = WeaponGuard();
	// An overhead chop: their blade pointing down and towards the player.
	BlockAngleVerdict v = BlockAngleFor(s, g, Blade(NiPoint3{0.0f, 60.0f, 140.0f}, NiPoint3{0.0f, -0.7f, -0.7f}));
	Check(v.verdict == BlockAngle::Blocked && Near(v.degrees, 90.0f), "a chop across the level blade: blocked, 90");
	// A sideways slash: their blade along the player's.
	v = BlockAngleFor(s, g, Blade(NiPoint3{-30.0f, 50.0f, 100.0f}, NiPoint3{1.0f, 0.0f, 0.0f}));
	Check(v.verdict == BlockAngle::Through && Near(v.degrees, 0.0f), "a slash along the blade: through, 0");
	// A diagonal cut, 45 degrees to the blade.
	const BlockBlow diagonal = Blade(NiPoint3{-30.0f, 50.0f, 130.0f}, NiPoint3{0.7071f, 0.0f, -0.7071f});
	v = BlockAngleFor(s, g, diagonal);
	Check(v.verdict == BlockAngle::Through && Near(v.degrees, 45.0f), "a diagonal cut, 30 degrees of slack: through");
	s.bladeDegrees = 50.0f;
	v = BlockAngleFor(s, g, diagonal);
	Check(v.verdict == BlockAngle::Blocked, "with 50 degrees of slack: blocked");
	Check(v.why != nullptr && v.why[0] != '\0', "with a reason given");
}

void TestWeaponWithoutTheirBlade() {
	std::printf("The weapon guard against something without a blade\n");
	BlockAngleSettings s;
	const BlockGuard g = WeaponGuard();
	BlockBlow b;
	b.centreValid = true;
	b.centre = NiPoint3{0.0f, 100.0f, 100.0f};  // straight ahead
	BlockAngleVerdict v = BlockAngleFor(s, g, b);
	Check(v.verdict == BlockAngle::Blocked && Near(v.degrees, 90.0f),
	      "a creature ahead, the blade across its way: blocked");
	b.centre = NiPoint3{100.0f, 0.0f, 100.0f};  // from the side, along the blade
	v = BlockAngleFor(s, g, b);
	Check(v.verdict == BlockAngle::Through && Near(v.degrees, 0.0f), "from the side, along the blade: through");
	b.centre = NiPoint3{0.0f, 0.0f, 100.0f};  // on the blade itself
	v = BlockAngleFor(s, g, b);
	Check(v.verdict == BlockAngle::Engine, "standing in the blade, no way to tell: the engine's");
	BlockBlow nothing;
	v = BlockAngleFor(s, g, nothing);
	Check(v.verdict == BlockAngle::Engine, "neither their blade nor where they are: the engine's");
}

void TestShield() {
	std::printf("The shield against a blow\n");
	BlockAngleSettings s;
	const BlockGuard g = ShieldGuard();
	// Their blade ahead of the shield's face.
	BlockAngleVerdict v = BlockAngleFor(s, g, Blade(NiPoint3{0.0f, 60.0f, 100.0f}, NiPoint3{0.0f, -1.0f, 0.0f}));
	Check(v.verdict == BlockAngle::Blocked && Near(v.degrees, 0.0f), "the blade ahead of the face: blocked, 0 off");
	// Their blade's middle 45 degrees off the face.
	v = BlockAngleFor(s, g, Blade(NiPoint3{35.0f, 45.0f, 75.0f}, NiPoint3{0.0f, 0.0f, 1.0f}));
	Check(v.verdict == BlockAngle::Blocked && Near(v.degrees, 45.0f, 2.0f), "45 off, 70 allowed: blocked");
	// Their blade beside the shield, edge on.
	v = BlockAngleFor(s, g, Blade(NiPoint3{100.0f, 10.0f, 75.0f}, NiPoint3{0.0f, 0.0f, 1.0f}));
	Check(v.verdict == BlockAngle::Through && Near(v.degrees, 90.0f), "edge on: through, 90 off");
	s.shieldDegrees = 30.0f;
	v = BlockAngleFor(s, g, Blade(NiPoint3{35.0f, 45.0f, 75.0f}, NiPoint3{0.0f, 0.0f, 1.0f}));
	Check(v.verdict == BlockAngle::Through, "45 off, 30 allowed: through");
	// The face's other side counts as the face: a disc has two.
	BlockGuard flipped = g;
	flipped.shieldNormal = NiPoint3{0.0f, -1.0f, 0.0f};
	s.shieldDegrees = 70.0f;
	v = BlockAngleFor(s, flipped, Blade(NiPoint3{0.0f, 60.0f, 100.0f}, NiPoint3{0.0f, -1.0f, 0.0f}));
	Check(v.verdict == BlockAngle::Blocked, "the normal the other way round: the same face, blocked");
	// No blade: where they stand.
	BlockBlow creature;
	creature.centreValid = true;
	creature.centre = NiPoint3{0.0f, 100.0f, 60.0f};
	v = BlockAngleFor(s, g, creature);
	Check(v.verdict == BlockAngle::Blocked && v.degrees < 30.0f, "a creature ahead, a little below: blocked");
	creature.centre = NiPoint3{100.0f, 10.0f, 100.0f};
	v = BlockAngleFor(s, g, creature);
	Check(v.verdict == BlockAngle::Through, "a creature beside the shield: through");
	creature.centre = g.shieldCentre;
	v = BlockAngleFor(s, g, creature);
	Check(v.verdict == BlockAngle::Engine, "at the shield's own centre: the engine's");
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
	Check(BlockAngleFor(s, g, Blade(NiPoint3{-30.0f, 50.0f, 100.0f}, NiPoint3{1.0f, 0.0f, 0.0f})).verdict ==
	          BlockAngle::Blocked,
	      "90 degrees of slack: even a slash along the blade is blocked");
}

}  // namespace

int main() {
	TestLineAngle();
	TestWeapon();
	TestWeaponWithoutTheirBlade();
	TestShield();
	TestLeftToTheEngine();
	TestSlack();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
