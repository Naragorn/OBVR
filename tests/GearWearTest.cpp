// Checks the armor and weapon wear's decisions (game/GearWearLogic.h).

#include <cstdio>
#include <limits>

#include "game/GearWearLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;
const float kNaN = std::numeric_limits<float>::quiet_NaN();

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return a - b < 1e-3f && b - a < 1e-3f; }

void TestSlots() {
	std::printf("The piece a struck part wears\n");
	WearSlots w = ArmorSlotsForPart(BodyPart::Head);
	Check(w.first == kSlotHead && w.second == kSlotNone, "head: the helmet only");
	w = ArmorSlotsForPart(BodyPart::Neck);
	Check(w.first == kSlotHead && w.second == kSlotUpperBody, "neck: the helmet, else the cuirass");
	w = ArmorSlotsForPart(BodyPart::Torso);
	Check(w.first == kSlotUpperBody && w.second == kSlotNone, "torso: the cuirass");
	w = ArmorSlotsForPart(BodyPart::Column);
	Check(w.first == kSlotUpperBody, "the body's column: the cuirass");
	w = ArmorSlotsForPart(BodyPart::LeftArm);
	Check(w.first == kSlotHand && w.second == kSlotUpperBody, "left arm: the gauntlets, else the cuirass");
	w = ArmorSlotsForPart(BodyPart::RightArm);
	Check(w.first == kSlotHand && w.second == kSlotUpperBody, "right arm: the same");
	w = ArmorSlotsForPart(BodyPart::LeftLeg);
	Check(w.first == kSlotLowerBody && w.second == kSlotFoot, "left leg: the greaves, else the boots");
	w = ArmorSlotsForPart(BodyPart::RightLeg);
	Check(w.first == kSlotLowerBody && w.second == kSlotFoot, "right leg: the same");
}

void TestMult() {
	std::printf("The multiplier\n");
	Check(Near(ClampWearMult(1.0f), 1.0f), "1: the engine's own");
	Check(Near(ClampWearMult(4.5f), 4.5f), "4.5 kept");
	Check(Near(ClampWearMult(10.0f), 10.0f), "10 kept");
	Check(Near(ClampWearMult(25.0f), 10.0f), "above 10: 10");
	Check(Near(ClampWearMult(0.2f), 1.0f), "below 1: 1, never less wear than the engine's");
	Check(Near(ClampWearMult(kNaN), 1.0f), "not a number: 1");
}

void TestGmst() {
	std::printf("The engine's setting scaled\n");
	GmstScale s;
	Check(Near(StepGmstScale(s, 9.0f, 1.0f), 9.0f), "first frame, x1: the game's 9 kept");
	Check(Near(StepGmstScale(s, 9.0f, 10.0f), 90.0f), "x10: 90");
	Check(Near(StepGmstScale(s, 90.0f, 10.0f), 90.0f), "our own 90 read back: not taken as a new base, no compounding");
	Check(Near(StepGmstScale(s, 90.0f, 2.0f), 18.0f), "the multiplier lowered: from the kept base");
	Check(Near(StepGmstScale(s, 12.0f, 2.0f), 24.0f), "a mod's setgs to 12: the new base, x2");
	Check(Near(StepGmstScale(s, 24.0f, 1.0f), 12.0f), "back to x1: the mod's 12");
	GmstScale w;
	const float weaponX3 = StepGmstScale(w, 0.06f, 3.0f);
	Check(Near(weaponX3, 0.18f), "the weapon's 0.06 x3");
	Check(Near(StepGmstScale(w, weaponX3, kNaN), 0.06f), "a broken multiplier: 1");
}

void TestModes() {
	std::printf("The player's mode\n");
	Check(GearBreakModeOf(0) == GearBreakMode::Vanilla, "0: vanilla");
	Check(GearBreakModeOf(1) == GearBreakMode::Knock, "1: knocked off");
	Check(GearBreakModeOf(2) == GearBreakMode::Inventory, "2: into the inventory");
	Check(GearBreakModeOf(7) == GearBreakMode::Inventory, "anything else: the default, inventory");
	const GearWearSettings d;
	Check(GearBreakModeOf(d.playerGearBreaksOff) == GearBreakMode::Inventory, "the default is inventory");
	Check(Near(d.armorWearMult, 1.0f) && Near(d.weaponWearMult, 1.0f), "the multipliers default to 1");
}

void TestZero() {
	std::printf("A piece at 0\n");
	// NPCs: every piece goes, never lootable.
	Check(ZeroHealthAction(false, GearBreakMode::Inventory, false, false, true) == ZeroAction::KnockDropped,
	      "NPC, the engine dropped it (a drawn weapon, a shield): sent off and gone");
	Check(ZeroHealthAction(false, GearBreakMode::Vanilla, true, true, false) == ZeroAction::DropAndKnock,
	      "NPC, still worn (a cuirass the engine leaves on): dropped, sent off, gone");
	Check(ZeroHealthAction(false, GearBreakMode::Knock, false, true, false) == ZeroAction::Leave,
	      "NPC, carried but not worn: left");
	Check(ZeroHealthAction(false, GearBreakMode::Knock, false, false, false) == ZeroAction::Leave,
	      "NPC, already gone: left");
	// The player, mode by mode, armor/shield/weapon alike.
	Check(ZeroHealthAction(true, GearBreakMode::Vanilla, true, true, false) == ZeroAction::Leave,
	      "player, vanilla, worn armor: left as the engine leaves it");
	Check(ZeroHealthAction(true, GearBreakMode::Vanilla, false, true, false) == ZeroAction::Leave,
	      "player, vanilla, a weapon the engine unequipped: left in the inventory");
	Check(ZeroHealthAction(true, GearBreakMode::Knock, true, true, false) == ZeroAction::DropAndKnock,
	      "player, knock, worn: dropped, sent off, gone");
	Check(ZeroHealthAction(true, GearBreakMode::Knock, false, true, false) == ZeroAction::DropAndKnock,
	      "player, knock, a weapon the engine already unequipped: still knocked off");
	Check(ZeroHealthAction(true, GearBreakMode::Knock, false, false, false) == ZeroAction::Leave,
	      "player, knock, no longer carried: left");
	Check(ZeroHealthAction(true, GearBreakMode::Inventory, true, true, false) == ZeroAction::ToInventory,
	      "player, inventory, worn: unequipped, kept");
	Check(ZeroHealthAction(true, GearBreakMode::Inventory, false, true, false) == ZeroAction::Leave,
	      "player, inventory, already unequipped by the engine: left, it is in the inventory");
	Check(ZeroHealthAction(true, GearBreakMode::Inventory, true, true, true) == ZeroAction::ToInventory,
	      "player: the engine never drops the player's piece, the mode decides");
}

void TestHelmetBlow() {
	std::printf("A hard blunt blow on a damaged helmet\n");
	Check(HelmetKnockedByBlow(true, true, 0.2f, 0.25f), "blunt, power, 20 %: off");
	Check(!HelmetKnockedByBlow(true, true, 0.3f, 0.25f), "blunt, power, 30 %: stays");
	Check(!HelmetKnockedByBlow(true, false, 0.1f, 0.25f), "blunt, light: stays");
	Check(!HelmetKnockedByBlow(false, true, 0.1f, 0.25f), "a blade: stays (it wears, it does not knock)");
	Check(!HelmetKnockedByBlow(true, true, 0.0f, 0.25f), "already at 0: the zero path takes it, not the blow");
}

void TestFlight() {
	std::printf("The flight and the end\n");
	NiPoint3 v = KnockOffVelocity(NiPoint3{3.0f, 4.0f, 9.0f}, 100.0f);
	Check(Near(v.x, 60.0f) && Near(v.y, 80.0f) && Near(v.z, 50.0f), "along the blow across the ground, half as much up");
	v = KnockOffVelocity(NiPoint3{0.0f, 0.0f, -1.0f}, 100.0f);
	Check(Near(v.x, 0.0f) && Near(v.y, 0.0f) && Near(v.z, 100.0f), "a blow straight down: straight up");
	KnockedPiece k;
	KnockStep st = StepKnockedPiece(k, 0.1f, false, 2.5f);
	Check(!st.send && !st.remove, "no body yet: waits");
	st = StepKnockedPiece(k, 0.1f, true, 2.5f);
	Check(st.send && !st.remove, "the body there: sent, once");
	st = StepKnockedPiece(k, 0.1f, true, 2.5f);
	Check(!st.send && !st.remove, "flying: not sent again");
	st = StepKnockedPiece(k, 2.5f, true, 2.5f);
	Check(st.remove && !st.send, "the flight over: disabled and deleted");
	KnockedPiece never;
	st = StepKnockedPiece(never, 3.0f, false, 2.5f);
	Check(st.remove && !st.send, "a body that never came: still taken away at the flight's end, not left as an item");
	KnockedPiece back;
	st = StepKnockedPiece(back, -1.0f, true, 2.5f);
	Check(st.send && Near(back.age, 0.0f), "a negative frame time: no age taken");
}

}  // namespace

int main() {
	TestSlots();
	TestMult();
	TestGmst();
	TestModes();
	TestZero();
	TestHelmetBlow();
	TestFlight();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
