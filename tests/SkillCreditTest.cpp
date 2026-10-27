// Checks the weapon skill a strike by motion trains (game/SkillCreditLogic.h):
// the engine's pending hits credited and cleared, an excepted target, the
// engine's no-skill value, the setting off, and the exception list another
// plugin replaces by message.

#include <cstdio>

#include "game/SkillCreditLogic.h"

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

SkillCreditInput In(UInt32 pending, UInt32 skill, bool excepted = false) {
	SkillCreditInput in;
	in.enabled = true;
	in.pendingHits = pending;
	in.skill = skill;
	in.targetExcepted = excepted;
	return in;
}

void TestPlan() {
	std::printf("The credit\n");
	constexpr UInt32 kBlade = 14;
	SkillCreditPlan p = PlanSkillCredit(In(1, kBlade));
	Check(p.uses == 1 && p.clearPending, "a landed hit: one use of the weapon's skill, the count cleared");
	p = PlanSkillCredit(In(0, kBlade));
	Check(p.uses == 0 && !p.clearPending, "no hit landed (blocked, missed): nothing, nothing cleared");
	p = PlanSkillCredit(In(3, kBlade));
	Check(p.uses == 3 && p.clearPending, "three pending: three uses, as the engine's loop");
	p = PlanSkillCredit(In(500, kBlade));
	Check(p.uses == kMaxSkillUsesPerStrike && p.clearPending, "an absurd count: capped");
	p = PlanSkillCredit(In(1, kBlade, true));
	Check(p.uses == 0 && p.clearPending, "an excepted target: no use, the count still cleared");
	p = PlanSkillCredit(In(1, kNoAttackSkill));
	Check(p.uses == 0 && p.clearPending, "the engine's no-skill value: no use, as the engine skips it");
	SkillCreditInput off = In(1, kBlade);
	off.enabled = false;
	p = PlanSkillCredit(off);
	Check(p.uses == 0 && !p.clearPending, "switched off: the count left to the game, as before");
}

void TestSlots() {
	std::printf("The table's slots\n");
	constexpr UInt32 kText0 = 0x00401000;
	constexpr UInt32 kText1 = 0x00A27C39;
	Check(SlotUsable(0x0066B0D0, 0x0066B0D0, kText0, kText1), "the engine's own function: called");
	Check(SlotUsable(0x6A001230, 0x0066B0D0, kText0, kText1),
	      "another plugin's hook on the slot (Oblivion Walkers on +0x210 and +0x39C): called through it");
	Check(!SlotUsable(0x0066B0F0, 0x0066B0D0, kText0, kText1),
	      "some other function inside the game: another build or a patch - refused");
	Check(!SlotUsable(0, 0x0066B0D0, kText0, kText1) && !SlotUsable(0x00001000, 0x0066B0D0, kText0, kText1),
	      "nothing, or no address: refused");
}

void TestExceptions() {
	std::printf("The exception list\n");
	SkillExceptions list;
	Check(!list.Contains(0x00069AD1) && list.Count() == 0, "empty at first");
	const UInt32 ids[3] = {0x11000801, 0x11000802, 0x00069AD1};
	Check(list.Replace(ids, sizeof(ids)), "three bases by message");
	Check(list.Count() == 3 && list.Contains(0x11000802) && !list.Contains(0x11000803), "found and not found");
	const UInt32 one[1] = {0x11000803};
	Check(list.Replace(one, sizeof(one)) && list.Count() == 1 && !list.Contains(0x11000801) &&
	          list.Contains(0x11000803),
	      "a second message replaces the list");
	Check(list.Replace(nullptr, 0) && list.Count() == 0, "an empty message: no exceptions");
	Check(!list.Replace(ids, 5) && list.Count() == 0, "a length that is no whole number of ids: refused, unchanged");
	Check(!list.Contains(0), "0 is never excepted");
	static UInt32 many[kMaxSkillExceptions + 10];
	for (UInt32 i = 0; i < kMaxSkillExceptions + 10; ++i) {
		many[i] = 0x01000000u + i;
	}
	Check(list.Replace(many, sizeof(many)) && list.Count() == kMaxSkillExceptions,
	      "more than it holds: the first kMaxSkillExceptions kept");
}

}  // namespace

int main() {
	TestPlan();
	TestSlots();
	TestExceptions();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
