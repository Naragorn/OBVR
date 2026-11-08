#include <cstdio>
#include <limits>

#include "game/ImpactLogic.h"
#include "vr/HandSettingsOps.h"
#include "vr/HandMode.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 0.001f) { return a - b < tolerance && b - a < tolerance; }

using namespace obvr;
using namespace obvr::game;

void TestTier() {
	std::printf("One speed for every blow\n");
	const ImpactSettings s;
	Check(Near(s.staggerSpeed, 3.0f) && Near(s.knockdownSpeed, 6.0f), "3 m/s a stagger, 6 a knockdown by default");
	Check(ImpactTier(s, 2.9f) == 0, "below the stagger speed: nothing");
	Check(ImpactTier(s, 3.0f) == 1, "at the stagger speed: a stagger");
	Check(ImpactTier(s, 5.9f) == 1, "short of the knockdown speed: a stagger");
	Check(ImpactTier(s, 6.0f) == 2, "at the knockdown speed: a knockdown");
	Check(ImpactTier(s, std::numeric_limits<float>::quiet_NaN()) == 0, "no speed read: nothing");
}

void TestChance() {
	std::printf("Vanilla's chance (0x005475D0)\n");
	const KnockdownGmst g;
	Check(Near(KnockdownAgility(g, 50.0f, 50.0f), 50.0f), "agility 50, luck 50: 50 + 20 - 20");
	Check(Near(KnockdownAgility(g, 0.0f, 0.0f), 0.0f), "held at 0");
	Check(Near(KnockdownAgility(g, 100.0f, 100.0f), 100.0f), "held at 100");
	Check(Near(KnockdownFatigue(g, 1.0f), 1.0f), "rested: the fatigue's factor 1");
	Check(Near(KnockdownFatigue(g, 0.0f), 0.5f), "spent: 0.5");
	// damage 10, A 50, F 1: 10 * 0.3 / 50 = 0.06.
	Check(Near(VanillaKnockdownChance(g, 50.0f, 50.0f, 1.0f, 10.0f), 0.06f), "10 damage on a rested 50: 6 %");
	Check(Near(VanillaKnockdownChance(g, 50.0f, 50.0f, 0.0f, 10.0f), 0.12f), "spent, twice as likely");
	Check(Near(VanillaKnockdownChance(g, 50.0f, 50.0f, 1.0f, 10.9f), 0.06f), "the damage taken whole, as the engine's int");
	Check(Near(VanillaKnockdownChance(g, 20.0f, 0.0f, 1.0f, 30.0f), 0.3f), "held at fKnockdownChance");
	Check(Near(VanillaKnockdownChance(g, 0.0f, 0.0f, 1.0f, 5.0f), 0.3f), "no agility at all (a zero denominator): the cap");
	KnockdownGmst odd = g;
	odd.damageMult = -1.0f;
	Check(Near(VanillaKnockdownChance(odd, 50.0f, 50.0f, 1.0f, 10.0f), 0.0f), "a negative chance from a mod's settings: 0");
}

void TestFist() {
	std::printf("The fist's damage (UESP Hand to Hand)\n");
	Check(Near(FistDamage(0.0f, 0.0f, false), 1.0f), "no strength, no skill: 1");
	Check(Near(FistDamage(100.0f, 100.0f, false), 11.5f), "both at 100: 11.5");
	Check(Near(FistDamage(150.0f, 120.0f, false), 11.5f), "both held to 100");
	Check(Near(FistDamage(50.0f, 40.0f, true), (1.0f + 10.5f * 0.5f * 0.4f) * 2.5f), "a power blow 2.5 times");
}

void TestStands() {
	std::printf("The roll\n");
	Check(KnockdownStands(false, 0.0f, 0.99f), "VanillaKnockdown off: always down");
	Check(KnockdownStands(true, 0.1f, 0.05f), "the roll under the chance: down");
	Check(!KnockdownStands(true, 0.1f, 0.1f), "the roll at the chance: a stagger (vanilla's strict <)");
	Check(!KnockdownStands(true, 0.0f, 0.0f), "no chance: never down");
}

void TestApply() {
	std::printf("Every way of striking takes the one speed (vr::ApplyImpact)\n");
	vr::HandSettings h;
	h.impact.staggerSpeed = 2.5f;
	h.impact.knockdownSpeed = 5.5f;
	h.impact.vanillaKnockdown = false;
	vr::ApplyImpact(h);
	Check(Near(h.shove.speed, 2.5f) && Near(h.shove.hardSpeed, 5.5f), "the shove (with the off hand, torch and throw of a held one)");
	Check(!h.shove.vanillaKnockdown, "the roll's switch with the shove, which every blow lands through");
	Check(Near(h.shieldBash.speed, 2.5f) && Near(h.shieldBash.hardSpeed, 5.5f), "the shield bash");
	Check(Near(h.pommel.speed, 2.5f) && Near(h.pommel.hardSpeed, 5.5f), "the pommel");
	Check(Near(h.halfSword.hiltHardSpeed, 5.5f), "the half-sword's hilt");
	Check(Near(h.throwHit.staggerSpeed, 2.5f) && Near(h.throwHit.knockSpeed, 5.5f), "a thrown thing");
	Check(!h.torchKnockdownPowerOnly, "the torch by its speed too, by default");
}

}  // namespace

int main() {
	TestTier();
	TestChance();
	TestFist();
	TestStands();
	TestApply();
	std::printf(g_failures == 0 ? "All impact checks passed.\n" : "%d impact check(s) FAILED.\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
