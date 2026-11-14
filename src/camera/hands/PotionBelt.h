#pragma once

// The potions kept at the holster places (vr/PotionBelt.h; [Hands]
// PotionBelt): a grip closing at a place that keeps one, the hand empty and
// the holster doing nothing there, drops one of that potion into the hand
// (game::DropItemForm, as the inventory's grab does) and the hand holds it
// while it stays closed (InventoryHold). The release at a place is the held
// object's stow (camera/hands/Held.cpp). Harness: potion-belt.txt.

#include "camera/hands/HandStep.h"
#include "camera/hands/Handover.h"

namespace obvr::camera::hands {

struct PotionBeltState {
	bool rightGripWas = false;
	bool leftGripWas = false;
	UInt32 lines = 24;
	UInt32 draws = 0;  // potions drawn from a place into the hand, for the hints
};

void StepPotionBelt(PotionBeltState& state, const HandStep& step, const HandShared& shared,
                    const vr::HandModeFrame& frame, InventoryHold& hold);

}  // namespace obvr::camera::hands
