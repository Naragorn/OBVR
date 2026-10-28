#include <cstdio>
#include <limits>

#include "game/InventoryGrabLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr::game;

void TestIndex() {
	std::printf("the item's index from the tile's traits\n");
	UInt32 index = 99;
	const UInt32 ids[3] = {0x0FA1, kTileTraitUser11, 0x0FAD};
	const float values[3] = {2.0f, 7.0f, 120.0f};
	Check(InventoryIndexFromTraits(ids, values, 3, index) && index == 7, "user11 reads 7: the index 7");
	const float first[3] = {2.0f, 0.0f, 120.0f};
	Check(InventoryIndexFromTraits(ids, first, 3, index) && index == 0, "user11 reads 0: the first item");
	const UInt32 none[2] = {0x0FA1, 0x0FAD};
	Check(!InventoryIndexFromTraits(none, values, 2, index), "no user11: no item");
	const float half[3] = {2.0f, 3.5f, 0.0f};
	Check(!InventoryIndexFromTraits(ids, half, 3, index), "a fraction is no index");
	const float negative[3] = {2.0f, -1.0f, 0.0f};
	Check(!InventoryIndexFromTraits(ids, negative, 3, index), "below 0 is no index");
	const float huge[3] = {2.0f, 100000.0f, 0.0f};
	Check(!InventoryIndexFromTraits(ids, huge, 3, index), "past the list's room is no index");
	const float nan[3] = {2.0f, std::numeric_limits<float>::quiet_NaN(), 0.0f};
	Check(!InventoryIndexFromTraits(ids, nan, 3, index), "not a number is no index");
	Check(!InventoryIndexFromTraits(nullptr, nullptr, 0, index), "no traits: no item");
}

void TestTrigger() {
	std::printf("the hold closing in the inventory\n");
	InventoryGrabTrigger t;
	Check(StepInventoryGrabTrigger(t, true, true, false) == InventoryHand::None,
	      "a hold closed as the inventory opens takes nothing");
	Check(StepInventoryGrabTrigger(t, true, true, false) == InventoryHand::None, "held on: still nothing");
	Check(StepInventoryGrabTrigger(t, true, false, false) == InventoryHand::None, "opened: nothing");
	Check(StepInventoryGrabTrigger(t, true, true, false) == InventoryHand::Right, "closed again: the right hand");
	Check(StepInventoryGrabTrigger(t, true, true, false) == InventoryHand::None, "held: once only");
	Check(StepInventoryGrabTrigger(t, true, false, true) == InventoryHand::Left, "the left closing: the left hand");
	InventoryGrabTrigger both;
	StepInventoryGrabTrigger(both, true, false, false);
	Check(StepInventoryGrabTrigger(both, true, true, true) == InventoryHand::Right, "both at once: the weapon hand");
	Check(StepInventoryGrabTrigger(both, false, false, false) == InventoryHand::None, "no inventory: nothing");
	Check(StepInventoryGrabTrigger(both, true, true, false) == InventoryHand::None,
	      "the inventory again with the hold closed: nothing");
}

void TestKinds() {
	std::printf("what the item is\n");
	Check(InventoryItemKindOf(kItemFormWeapon, 0, 0, 0) == InventoryItemKind::Weapon, "a blade");
	Check(InventoryItemKindOf(kItemFormWeapon, 3, 0, 0) == InventoryItemKind::Weapon, "a two-handed blunt");
	Check(InventoryItemKindOf(kItemFormWeapon, 4, 0, 0) == InventoryItemKind::Weapon, "a staff");
	Check(InventoryItemKindOf(kItemFormWeapon, 5, 0, 0) == InventoryItemKind::Bow, "a bow");
	Check(InventoryItemKindOf(kItemFormWeapon, 9, 0, 0) == InventoryItemKind::Other, "an unknown weapon type: held");
	Check(InventoryItemKindOf(kItemFormArmor, 0, kItemSlotShield, 0) == InventoryItemKind::Shield, "a shield");
	Check(InventoryItemKindOf(kItemFormArmor, 0, 0x0004, 0) == InventoryItemKind::Other, "a cuirass: held");
	Check(InventoryItemKindOf(kItemFormLight, 0, 0, kItemLightCarried) == InventoryItemKind::Torch, "a torch");
	Check(InventoryItemKindOf(kItemFormLight, 0, 0, 0) == InventoryItemKind::Other, "a light not carried");
	Check(InventoryItemKindOf(0x29, 0, kItemSlotShield, kItemLightCarried) == InventoryItemKind::Other,
	      "another type: held, whatever its bytes");
}

void TestTakes() {
	std::printf("what the take does by hand (decision (b))\n");
	using K = InventoryItemKind;
	using H = InventoryHand;
	using T = InventoryTake;
	Check(ChooseInventoryTake(K::Weapon, H::Right) == T::EquipWeapon, "a sword in the weapon hand: equipped");
	Check(ChooseInventoryTake(K::Weapon, H::Left) == T::Hold, "a sword in the other hand: held");
	Check(ChooseInventoryTake(K::Bow, H::Left) == T::EquipWeapon, "a bow in the other hand: equipped");
	Check(ChooseInventoryTake(K::Bow, H::Right) == T::Hold, "a bow in the weapon hand: held");
	Check(ChooseInventoryTake(K::Shield, H::Left) == T::EquipOther, "a shield in the other hand: equipped");
	Check(ChooseInventoryTake(K::Shield, H::Right) == T::Hold, "a shield in the weapon hand: held");
	Check(ChooseInventoryTake(K::Torch, H::Left) == T::EquipOther, "a torch in the other hand: equipped");
	Check(ChooseInventoryTake(K::Torch, H::Right) == T::Hold, "a torch in the weapon hand: held");
	Check(ChooseInventoryTake(K::Other, H::Right) == T::Hold && ChooseInventoryTake(K::Other, H::Left) == T::Hold,
	      "anything else: held in either hand");
	Check(ChooseInventoryTake(K::Weapon, H::None) == T::Nothing, "no hand: nothing");
}

}  // namespace

int main() {
	TestIndex();
	TestTrigger();
	TestKinds();
	TestTakes();
	std::printf("%s (%d failed)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
	return g_failures == 0 ? 0 : 1;
}
