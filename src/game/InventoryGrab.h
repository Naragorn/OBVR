#pragma once

#include "game/InventoryGrabLogic.h"

namespace obvr::game {

// The inventory's item under the pointer (InventoryGrabLogic.h has the
// reading): its index, its form, the form's id and type, its name.
struct InventoryItemUnder {
	UInt32 index = 0;
	UInt32 form = 0;
	UInt32 formId = 0;
	UInt8 formType = 0;
	char name[64] = {};
};

// False unless the inventory is the active menu and the tile under the
// cursor names an item the player carries.
bool InventoryItemUnderCursor(InventoryItemUnder& out);

// What an item's base form is, for the take (InventoryItemKindOf), and a
// weapon's type (TESObjectWEAP +0x90; -1 for no weapon).
InventoryItemKind InventoryItemKindOfForm(UInt32 form, SInt32& weaponType);

}  // namespace obvr::game
