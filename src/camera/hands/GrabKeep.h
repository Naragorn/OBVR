#pragma once

// The grab a hand keeps with its grip open, around the hand mode's step:
// - the inventory's item held in the hand that took it (game/InventoryGrab.h,
//   step 4): its grab wanted while that hand's hold stays closed (moved here
//   from camera/CameraHook.cpp's UpdateHandMode on 2026-11-27, as it was);
// - a weapon, bow or torch pulled to a hand under the grip toggle
//   (game/PullHold.h): the hand keeps it for the toggle from the pull on, and
//   the grab stays on while that hand's hold is closed.
// Harness: inventory-grab*.txt, weapon-pull-toggle.txt.

#include "camera/hands/Grab.h"
#include "camera/hands/Handover.h"
#include "game/PullHold.h"
#include "vr/HandMode.h"

namespace obvr::camera::hands {

// The pull's state of the last grab step, read for PullHold.h.
game::PullHoldInput PullHoldOf(const GrabState& grab);

// Before the hand mode's step: the hand the grab's line goes from (the
// inventory's item, a pull's under the toggle), and the hands a pull keeps
// (frame.pullRight/pullLeft).
void NoteGrabHands(vr::HandModeFrame& frame, const InventoryHold& inventoryHold, const GrabState& grab, bool active,
                   bool toggleOn);

// After it: the grab wanted for the inventory's item and for a pull under the
// toggle (`toggleOn`, [Hands] GripToggle) while the holding hand's hold stays
// closed; `dt` the frame's time.
void KeepGrabs(InventoryHold& inventoryHold, const GrabState& grab, vr::HandModeResult& hand, bool active,
               bool menuIsUp, float dt, bool toggleOn);

}  // namespace obvr::camera::hands
