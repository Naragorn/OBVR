#include <cstdio>

#include "game/DismemberLogic.h"

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

void TestEdge() {
	std::printf("an edge: a blade, or an axe by its model\n");
	Check(WeaponHasEdge(kCutWeaponBladeOneHand, "Weapons\\Iron\\LongSword.NIF"), "a one-handed blade");
	Check(WeaponHasEdge(kCutWeaponBladeTwoHand, "Weapons\\Steel\\Claymore.NIF"), "a two-handed blade");
	Check(WeaponHasEdge(kCutWeaponBluntOneHand, "Weapons\\Iron\\WarAxe.NIF"), "a war axe, typed blunt");
	Check(WeaponHasEdge(kCutWeaponBluntTwoHand, "weapons\\daedric\\battleaxe.nif"), "a battle axe, any case");
	Check(!WeaponHasEdge(kCutWeaponBluntOneHand, "Weapons\\Iron\\Mace.NIF"), "a mace: none");
	Check(!WeaponHasEdge(kCutWeaponBluntTwoHand, "Weapons\\Steel\\WarHammer.NIF"), "a warhammer: none");
	Check(!WeaponHasEdge(kCutWeaponBluntOneHand, nullptr), "blunt without a model path: none");
	Check(!WeaponHasEdge(kCutWeaponStaff, "Weapons\\Staff\\Axe.NIF"), "a staff never, whatever its model");
	Check(!WeaponHasEdge(kCutWeaponBow, "Weapons\\Bow\\Axe.NIF"), "a bow never");
	Check(!WeaponHasEdge(-1, "Weapons\\Axe.NIF"), "no weapon (fists): none");
}

void TestNeckTouched() {
	std::printf("the blade's sweep through the neck\n");
	const NiPoint3 spine{0.0f, 0.0f, 100.0f};
	const NiPoint3 head{0.0f, 0.0f, 115.0f};
	const float radius = 5.0f;
	// A blade across the neck from left to right, 60 units long.
	const NiPoint3 gripBefore{-30.0f, -40.0f, 108.0f};
	const NiPoint3 tipBefore{-30.0f, 20.0f, 108.0f};
	const NiPoint3 gripNow{30.0f, -40.0f, 108.0f};
	const NiPoint3 tipNow{30.0f, 20.0f, 108.0f};
	Check(NeckTouched(spine, head, radius, gripBefore, tipBefore, gripNow, tipNow),
	      "swept from one side of the neck to the other: through it");
	const NiPoint3 high{0.0f, 0.0f, 160.0f};
	Check(!NeckTouched(spine, head, radius, gripBefore + high - NiPoint3{0.0f, 0.0f, 108.0f},
	                   tipBefore + high - NiPoint3{0.0f, 0.0f, 108.0f}, gripNow + high - NiPoint3{0.0f, 0.0f, 108.0f},
	                   tipNow + high - NiPoint3{0.0f, 0.0f, 108.0f}),
	      "the same sweep over the head: not");
	Check(NeckTouched(spine, head, radius, gripNow, tipNow, gripNow, tipNow) == false,
	      "standing still beside the neck: not");
	const NiPoint3 inGrip{-30.0f, 0.0f, 108.0f};
	const NiPoint3 inTip{30.0f, 0.0f, 108.0f};
	Check(NeckTouched(spine, head, radius, inGrip, inTip, inGrip, inTip), "lying in the neck now: touched");
}

CutInput Lethal() {
	CutInput in;
	in.enabled = true;
	in.dead = true;
	in.onNeck = true;
	in.edge = true;
	in.speedMetres = 4.0f;
	return in;
}

void TestDecide() {
	std::printf("the rule: the killing blow or a corpse, on the neck, with an edge, fast enough\n");
	const CutSettings s;
	Check(DecideCut(s, Lethal()) == CutVerdict::Cut, "dead, the neck, an edge, fast: the head comes off");
	CutInput in = Lethal();
	in.enabled = false;
	Check(DecideCut(s, in) == CutVerdict::Off, "Dismemberment=0: never");
	in = Lethal();
	in.alreadyCut = true;
	Check(DecideCut(s, in) == CutVerdict::AlreadyCut, "a head already off: nothing more");
	in = Lethal();
	in.dead = false;
	Check(DecideCut(s, in) == CutVerdict::Living, "alive: the living keep their parts");
	in = Lethal();
	in.edge = false;
	Check(DecideCut(s, in) == CutVerdict::NoEdge, "a mace or the fists: nothing cut");
	in = Lethal();
	in.onNeck = false;
	Check(DecideCut(s, in) == CutVerdict::NotOnNeck, "the torso: nothing cut");
	in = Lethal();
	in.speedMetres = s.minSpeedMetres * 0.9f;
	Check(DecideCut(s, in) == CutVerdict::TooSlow, "slower than DismemberMinSpeed: nothing cut");
	in.speedMetres = s.minSpeedMetres;
	Check(DecideCut(s, in) == CutVerdict::Cut, "at the threshold: cut");
	CutSettings kills = s;
	kills.cutKills = true;
	in = Lethal();
	in.dead = false;
	Check(DecideCut(kills, in) == CutVerdict::Cut, "CutKills=1: the living too");
	in.essential = true;
	Check(DecideCut(kills, in) == CutVerdict::Essential, "but never an essential one");
	Check(CutVerdictName(CutVerdict::TooSlow)[0] != '\0', "every verdict has a name for the log");
}

void TestSides() {
	std::printf("which bone pointers each side's shapes give up\n");
	Check(SwapBoneOnBodySide(true, false), "the body's shapes: the head bone collapses at the neck");
	Check(SwapBoneOnBodySide(false, true), "and anything under the head");
	Check(!SwapBoneOnBodySide(false, false), "the body's own bones stay");
	Check(SwapBoneOnHeadSide(false, false), "the head's shapes: the neck, spine and clavicles collapse under it");
	Check(!SwapBoneOnHeadSide(true, false) && !SwapBoneOnHeadSide(false, true), "the head and its own bones stay");
}

}  // namespace

int main() {
	TestEdge();
	TestNeckTouched();
	TestDecide();
	TestSides();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
