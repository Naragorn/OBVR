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

}  // namespace obvr::game
