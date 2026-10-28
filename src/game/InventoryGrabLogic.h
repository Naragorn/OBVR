#pragma once

// An item highlighted in the inventory taken into the hand whose hold closes
// (Nadi, 2026-10-28; docs/holding-objects-spec.md, "from the inventory into
// the hand"). Step 1: which item the pointer is on.
//
// The inventory's item tile carries the item's index in its user11 trait
// (xOBSE Commands_Menu.cpp GetActiveMenuElement: the active tile's
// kTileValue_user11 for an InventoryMenu; GameTiles.h: user11 = 0x0FB9), and
// the index names the entry through TESObjectREFR::GetInventoryItem
// (0x004D88F0, xOBSE GameObjects.cpp: thiscall (index, bGetWares), an
// ExtraContainerChanges::EntryData* whose form is at +8).
// Pure: game/InventoryGrab.cpp reads and calls; inventory_grab_test.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kTileTraitUser11 = 0x0FB9;
// More rows than any inventory holds means a value that is not an index.
inline constexpr UInt32 kInventoryIndexMax = 4096;

// The index from a tile's traits (id, float value): user11 as a whole
// number from 0 below kInventoryIndexMax; false for none.
inline bool InventoryIndexFromTraits(const UInt32* ids, const float* values, UInt32 count, UInt32& index) {
	for (UInt32 i = 0; i < count && ids != nullptr && values != nullptr; ++i) {
		if (ids[i] != kTileTraitUser11) {
			continue;
		}
		const float v = values[i];
		if (!(v >= 0.0f) || !(v < static_cast<float>(kInventoryIndexMax))) {
			return false;
		}
		const UInt32 whole = static_cast<UInt32>(v);
		if (static_cast<float>(whole) != v) {
			return false;
		}
		index = whole;
		return true;
	}
	return false;
}

// Step 2: the hold closing in the inventory takes the item.
//
// Which hand closed its hold this frame while the inventory is up. A hold
// already closed when the inventory opened counts only once it has opened
// again: the grip that was a holster's or a grab's in the world takes
// nothing. The hands are the roles (a left-handed player's are swapped
// before the frame, vr::AssignHandRoles): Right is the weapon hand.
enum class InventoryHand : UInt8 { None, Right, Left };

struct InventoryGrabTrigger {
	bool armedRight = false;  // seen open in the inventory
	bool armedLeft = false;
};

inline InventoryHand StepInventoryGrabTrigger(InventoryGrabTrigger& t, bool inventoryOpen, bool rightHold,
                                              bool leftHold) {
	if (!inventoryOpen) {
		t = InventoryGrabTrigger{};
		return InventoryHand::None;
	}
	InventoryHand hand = InventoryHand::None;
	if (rightHold && t.armedRight) {
		hand = InventoryHand::Right;
	} else if (leftHold && t.armedLeft) {
		hand = InventoryHand::Left;
	}
	t.armedRight = !rightHold;
	t.armedLeft = !leftHold;
	return hand;
}

// What the highlighted item is, for the take.
enum class InventoryItemKind : UInt8 {
	Other,     // food, a potion, clutter, armour, clothing: held as an object
	Weapon,    // a one-hander, a two-hander, a staff
	Bow,
	Shield,
	Torch,
};

// The form types (xOBSE GameForms.h's kFormType enum; WEAP 0x21 as
// GameAddresses.h kFormTypeWeapon, LIGH 0x1A as game/TorchGrab.h) and what a
// form carries: TESObjectWEAP's type at +0x90 (5 the bow), TESObjectARMO's
// biped slot mask at +0x68 (its TESBipedModelForm at +0x64, the mask a word
// at +4; the shield bit 13 - xOBSE kPart_Shield), TESObjectLIGH's flags at
// +0x7C (0x02 carried - a torch).
inline constexpr UInt8 kItemFormWeapon = 0x21;
inline constexpr UInt8 kItemFormArmor = 0x14;
inline constexpr UInt8 kItemFormLight = 0x1A;
inline constexpr UInt32 kItemWeaponTypeOffset = 0x90;
inline constexpr UInt32 kItemArmorSlotsOffset = 0x68;
inline constexpr UInt32 kItemLightFlagsOffset = 0x7C;
inline constexpr UInt16 kItemSlotShield = 1u << 13;
inline constexpr UInt32 kItemLightCarried = 0x02;
inline constexpr SInt32 kItemWeaponTypeBow = 5;

inline InventoryItemKind InventoryItemKindOf(UInt8 formType, SInt32 weaponType, UInt16 armorSlots,
                                             UInt32 lightFlags) {
	if (formType == kItemFormWeapon) {
		if (weaponType == kItemWeaponTypeBow) {
			return InventoryItemKind::Bow;
		}
		return weaponType >= 0 && weaponType <= 4 ? InventoryItemKind::Weapon : InventoryItemKind::Other;
	}
	if (formType == kItemFormArmor && (armorSlots & kItemSlotShield) != 0) {
		return InventoryItemKind::Shield;
	}
	if (formType == kItemFormLight && (lightFlags & kItemLightCarried) != 0) {
		return InventoryItemKind::Torch;
	}
	return InventoryItemKind::Other;
}

// What the take does (Nadi, 2026-10-28, decision (b): what the grabbing hand
// cannot be equipped with it holds as an object). The weapon hand equips a
// weapon; the other hand a bow, a shield, a torch - the hands OBVR shows
// them in (the bow is held in the other hand, the engine shows a shield and
// a torch only there). Everything else is held.
enum class InventoryTake : UInt8 {
	Nothing,
	EquipWeapon,  // into the take's pipeline: equipped and drawn into the hand
	EquipOther,   // a shield or a torch: equipped, in the other hand
	Hold,         // dropped at the hand and held there as an object
};

inline InventoryTake ChooseInventoryTake(InventoryItemKind kind, InventoryHand hand) {
	if (hand == InventoryHand::None) {
		return InventoryTake::Nothing;
	}
	const bool weaponHand = hand == InventoryHand::Right;
	switch (kind) {
	case InventoryItemKind::Weapon:
		return weaponHand ? InventoryTake::EquipWeapon : InventoryTake::Hold;
	case InventoryItemKind::Bow:
		return weaponHand ? InventoryTake::Hold : InventoryTake::EquipWeapon;
	case InventoryItemKind::Shield:
	case InventoryItemKind::Torch:
		return weaponHand ? InventoryTake::Hold : InventoryTake::EquipOther;
	default:
		return InventoryTake::Hold;
	}
}

}  // namespace obvr::game
