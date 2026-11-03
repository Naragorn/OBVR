// Checks which skill each VR strike deals and trains with, which strikes the
// struck one reacts to as an attack, and the bonus a cut or a crushed head
// trains (game/StrikeSkillLogic.h) - one flow per row of Nadi's table of
// 2026-11-03.

#include <cstdio>

#include "game/StrikeSkillLogic.h"

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

bool Is(BlowKind kind, UInt32 skill, bool reacts, SInt32 weaponType = -1) {
	const StrikeRule r = StrikeRuleFor(kind, weaponType);
	return r.skill == skill && r.reacts == reacts;
}

void TestRules() {
	std::printf("The table\n");
	Check(Is(BlowKind::ShieldBash, kSkillBlock, true), "1 shield bash: Block, a crime");
	Check(Is(BlowKind::Pommel, kSkillBlunt, true), "2 pommel: Blunt, a crime");
	Check(Is(BlowKind::Hilt, kSkillBlunt, true), "3 hilt (half-sword): Blunt, a crime");
	Check(Is(BlowKind::Torch, kSkillHandToHand, true), "4 torch: hand to hand, a crime");
	Check(Is(BlowKind::OffHandPunch, kSkillHandToHand, true), "5 off-hand punch: hand to hand, a crime");
	Check(Is(BlowKind::Kick, kSkillHandToHand, true), "7 kick: hand to hand as before");
	Check(Is(BlowKind::Slap, kSkillHandToHand, true), "8 slap: hand to hand, every slap a crime");
	Check(Is(BlowKind::Shove, kSkillHandToHand, false), "9 shove: hand to hand, no crime from the hit");
	Check(Is(BlowKind::GrappleThrow, kSkillHandToHand, true), "10 grapple throw: hand to hand");
	Check(Is(BlowKind::ThrownThing, kSkillHandToHand, true), "12 another thrown thing: hand to hand");
	Check(Is(BlowKind::Kick, kSkillHandToHand, true, 0), "the weapon type is read for a thrown weapon only");
}

void TestThrownWeapon() {
	std::printf("11 a thrown weapon: its own skill\n");
	Check(Is(BlowKind::ThrownWeapon, kSkillBlade, true, 0), "a one-handed blade: Blade");
	Check(Is(BlowKind::ThrownWeapon, kSkillBlade, true, 1), "a two-handed blade: Blade");
	Check(Is(BlowKind::ThrownWeapon, kSkillBlunt, true, 2), "a one-handed blunt: Blunt");
	Check(Is(BlowKind::ThrownWeapon, kSkillBlunt, true, 3), "a two-handed blunt: Blunt");
	Check(Is(BlowKind::ThrownWeapon, kSkillBlunt, true, 4), "a staff (no skill of its own): Blunt");
	Check(Is(BlowKind::ThrownWeapon, kSkillMarksman, true, 5), "a bow: Marksman");
	Check(Is(BlowKind::ThrownWeapon, kSkillHandToHand, true, -1), "no type read: hand to hand");
	Check(Is(BlowKind::ThrownWeapon, kSkillHandToHand, true, 9), "a type the game does not have: hand to hand");
}

void TestNames() {
	std::printf("The names\n");
	bool all = true;
	for (UInt8 k = 0; k <= static_cast<UInt8>(BlowKind::ThrownThing); ++k) {
		const char* const name = BlowKindName(static_cast<BlowKind>(k));
		all = all && name[0] != '?';
	}
	Check(all, "every kind has a name for the log");
	Check(BlowKindName(static_cast<BlowKind>(200))[0] == '?', "an unknown kind: '?'");
}

void TestHandStrike() {
	std::printf("Which strike a hand is\n");
	Check(HandStrikeKind(true, true, true) == BlowKind::Torch, "the torch: a torch blow, wherever it lands");
	Check(HandStrikeKind(false, true, true) == BlowKind::OffHandPunch, "a fist: a punch, in the face too");
	Check(HandStrikeKind(false, false, true) == BlowKind::Slap, "an open hand in the face: a slap");
	Check(HandStrikeKind(false, false, false) == BlowKind::Shove, "an open hand on the body: a shove");
}

void TestSettingsAfterStrike() {
	std::printf("The shove after the engine's hit\n");
	ShoveSettings shove;
	shove.countsAsHit = true;
	shove.lightCountsAsHit = true;
	ShoveSettings s = SettingsAfterStrike(shove, true, true);
	Check(!s.countsAsHit && !s.lightCountsAsHit, "the hit told them: not told twice");
	Check(s.fatigueLight == shove.fatigueLight && s.dispositionHard == shove.dispositionHard,
	      "fatigue and liking stay the shove's");
	s = SettingsAfterStrike(shove, true, false);
	Check(s.countsAsHit && s.lightCountsAsHit, "a quiet hit (a shove): the shove's own rule stands");
	s = SettingsAfterStrike(shove, false, true);
	Check(s.countsAsHit && s.lightCountsAsHit, "no engine hit (refused): the shove's own rule, as before");
}

DismemberBonusInput Bonus(DismemberBonus what, SInt32 type) {
	DismemberBonusInput in;
	in.enabled = true;
	in.what = what;
	in.weaponType = type;
	return in;
}

void TestDismemberBonus() {
	std::printf("15-17 the cut's bonus\n");
	DismemberBonusPlan p = PlanDismemberBonus(Bonus(DismemberBonus::HeadOff, 0));
	Check(p.skill == kSkillBlade && p.uses == 5, "15 a head off with a blade: 5 uses of Blade");
	p = PlanDismemberBonus(Bonus(DismemberBonus::HeadOff, 2));
	Check(p.skill == kSkillBlunt && p.uses == 5, "15 a head off with an edged blunt weapon (an axe): Blunt");
	p = PlanDismemberBonus(Bonus(DismemberBonus::HandOff, 1));
	Check(p.skill == kSkillBlade && p.uses == 2, "16 a hand off: 2 uses of the weapon's skill");
	p = PlanDismemberBonus(Bonus(DismemberBonus::HeadCrushed, 0));
	Check(p.skill == kSkillBlunt && p.uses == 5, "17 a crushed head: 5 uses of Blunt, whatever the weapon");
	DismemberBonusInput in = Bonus(DismemberBonus::HeadOff, 0);
	in.headUses = 9;
	in.handUses = 1;
	Check(PlanDismemberBonus(in).uses == 9, "the head's uses from [Hands] DismemberHeadSkillUses");
	in.what = DismemberBonus::HandOff;
	Check(PlanDismemberBonus(in).uses == 1, "the hand's uses from [Hands] DismemberHandSkillUses");
	in.handUses = 0;
	Check(PlanDismemberBonus(in).uses == 0, "0 uses: none");
	in.handUses = -3;
	Check(PlanDismemberBonus(in).uses == 0, "a negative count: none");
	in.handUses = 5000;
	Check(PlanDismemberBonus(in).uses == static_cast<UInt32>(kMaxDismemberBonusUses), "an absurd count: capped");
	in = Bonus(DismemberBonus::HeadOff, 0);
	in.targetExcepted = true;
	Check(PlanDismemberBonus(in).uses == 0, "an excepted target (a walker): no bonus");
	in = Bonus(DismemberBonus::HeadCrushed, 0);
	in.enabled = false;
	Check(PlanDismemberBonus(in).uses == 0, "StrikesTrainSkills off: no bonus");
}

}  // namespace

int main() {
	TestRules();
	TestThrownWeapon();
	TestNames();
	TestHandStrike();
	TestSettingsAfterStrike();
	TestDismemberBonus();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
