#pragma once

// The torches the player let go of burning (game/TorchFire.h), kept by form
// id so OBVR can put the fire on again when the engine builds a torch's 3D
// anew - a cell left and come back to: the engine makes a carried light's
// reference dark ("off by default"), so its fire is OBVR's to keep. Its id
// finds no reference both when it was picked up and when its cell is
// unloaded (pcb, torch-drop-relight.txt 2026-10-26 08:58), so the cell it
// lay in is kept with it: not found while the player is in that cell, it is
// gone (LitTorchGone); not found elsewhere, it waits for its cell.
//
// Pure; covered by lit_torch_list_test.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kLitTorchMax = 16;

struct LitTorchList {
	UInt32 ids[kLitTorchMax] = {};
	UInt32 cells[kLitTorchMax] = {};
	UInt32 count = 0;

	UInt32 CellOf(UInt32 id) const {
		for (UInt32 i = 0; i < count; ++i) {
			if (ids[i] == id) {
				return cells[i];
			}
		}
		return 0;
	}

	bool Has(UInt32 id) const {
		for (UInt32 i = 0; i < count; ++i) {
			if (ids[i] == id) {
				return true;
			}
		}
		return false;
	}

	// Answers whether it was added: not 0, not already in, room left.
	bool Add(UInt32 id, UInt32 cell = 0) {
		if (id == 0 || Has(id) || count >= kLitTorchMax) {
			return false;
		}
		ids[count] = id;
		cells[count] = cell;
		++count;
		return true;
	}

	bool Remove(UInt32 id) {
		for (UInt32 i = 0; i < count; ++i) {
			if (ids[i] == id) {
				for (UInt32 j = i + 1; j < count; ++j) {
					ids[j - 1] = ids[j];
					cells[j - 1] = cells[j];
				}
				--count;
				ids[count] = 0;
				cells[count] = 0;
				return true;
			}
		}
		return false;
	}
};

// A torch whose id found no reference: gone for good when the player is in
// the cell it lay in (picked up), or when its cell is not known; kept while
// the player is elsewhere or between cells (a load).
inline bool LitTorchGone(UInt32 torchCell, UInt32 playerCell) {
	return torchCell == 0 || (playerCell != 0 && playerCell == torchCell);
}

}  // namespace obvr::game
