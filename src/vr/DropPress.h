#pragma once

#include "core/Types.h"

namespace obvr::vr {

// Dropping from the inventory: vanilla drops the item under the cursor on
// Shift and a left click (help.bethesda.net answer 9938, UESP
// Oblivion:Controls "Drop: Shift + Left Click"; a click held instead drops
// the item into the grab, which this never does). On the drop button's
// press: Shift down, then the click down, then the click up, then Shift up -
// a frame each, so the game sees the modifier before the click and after it.
struct DropPressState {
	UInt32 phase = 0;  // 0 idle, 1 Shift down, 2 click down, 3 click up
};

// What a press of the drop button in a menu did: dropped (the inventory is
// the active menu), nothing because another menu is active, or nothing
// because no press began or no menu is up. Logged per press (ControlsOut), so
// a headset session where the drop "does nothing" says which it was.
enum class DropGate : UInt8 { None, Drop, NotInventory };

inline DropGate JudgeDropPress(bool pressBegan, bool menuMode, bool inventoryOpen) {
	if (!pressBegan || !menuMode) {
		return DropGate::None;
	}
	return inventoryOpen ? DropGate::Drop : DropGate::NotInventory;
}

struct DropPressVerdict {
	bool shift = false;
	bool click = false;
};

inline DropPressVerdict StepDropPress(DropPressState& s, bool pressed) {
	DropPressVerdict v;
	if (s.phase == 0) {
		if (!pressed) {
			return v;
		}
		s.phase = 1;
	} else {
		++s.phase;
	}
	switch (s.phase) {
	case 1:
		v.shift = true;
		break;
	case 2:
		v.shift = true;
		v.click = true;
		break;
	case 3:
		v.shift = true;
		break;
	default:
		s.phase = 0;
		break;
	}
	return v;
}

}  // namespace obvr::vr
