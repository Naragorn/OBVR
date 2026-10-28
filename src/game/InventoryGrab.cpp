#include "game/InventoryGrab.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "game/MenuType.h"
#include "game/QuickKeys.h"

namespace obvr::game {
namespace {

// TESObjectREFR::GetInventoryItem (xOBSE GameObjects.cpp): thiscall
// (index, bGetWares) - an ExtraContainerChanges::EntryData*, its form at +8
// (xOBSE GameExtraData.h: data +0, countDelta +4, type +8).
constexpr UInt32 kGetInventoryItem = 0x004D88F0;
constexpr UInt32 kEntryFormOffset = 0x08;
constexpr UInt32 kTraitsCapacity = 48;

using GetInventoryItemFn = UInt32(__fastcall*)(UInt32 ref, void* edx, UInt32 index, UInt8 getWares);

UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }

}  // namespace

bool InventoryItemUnderCursor(InventoryItemUnder& out) {
	out = InventoryItemUnder{};
	if (ActiveMenuId() != kMenuIdInventory) {
		return false;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!mem::LooksLikeObjectAddress(player)) {
		return false;
	}
	// The tile under the cursor itself, as xOBSE reads it: its parents carry
	// a user11 of their own (the list's frame read 0 off the list, harness
	// 2026-10-28), which is no item.
	UInt32 ids[kTraitsCapacity];
	float values[kTraitsCapacity];
	const UInt32 count = ActiveTileTraits(ids, values, kTraitsCapacity, 0);
	UInt32 index = 0;
	if (!InventoryIndexFromTraits(ids, values, count, index)) {
		return false;
	}
	const UInt32 entry = reinterpret_cast<GetInventoryItemFn>(kGetInventoryItem)(player, nullptr, index, 0);
	if (!mem::LooksLikeObjectAddress(entry)) {
		return false;
	}
	const UInt32 form = Read(entry + kEntryFormOffset);
	if (!mem::LooksLikeObjectAddress(form)) {
		return false;
	}
	out.index = index;
	out.form = form;
	out.formId = Read(form + addr::kFormIdOffset);
	out.formType = *reinterpret_cast<const UInt8*>(form + addr::kFormTypeOffset);
	ReadFormFullName(form, out.name, sizeof(out.name));
	return true;
}

InventoryItemKind InventoryItemKindOfForm(UInt32 form, SInt32& weaponType) {
	weaponType = -1;
	if (!mem::LooksLikeObjectAddress(form)) {
		return InventoryItemKind::Other;
	}
	const UInt8 type = *reinterpret_cast<const UInt8*>(form + addr::kFormTypeOffset);
	UInt16 slots = 0;
	UInt32 lightFlags = 0;
	if (type == kItemFormWeapon) {
		weaponType = static_cast<SInt32>(Read(form + kItemWeaponTypeOffset));
	} else if (type == kItemFormArmor) {
		slots = *reinterpret_cast<const UInt16*>(form + kItemArmorSlotsOffset);
	} else if (type == kItemFormLight) {
		lightFlags = Read(form + kItemLightFlagsOffset);
	}
	return InventoryItemKindOf(type, weaponType, slots, lightFlags);
}

}  // namespace obvr::game
