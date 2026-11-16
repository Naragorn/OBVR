#pragma once

// Where the sheathed weapon the game draws on the player's body hangs, for
// its own reach (vr/SheathReach.h): the weapon model's origin - its grip -
// under the drawn body's SideWeapon (one-handers) or BackWeapon (two-handers,
// staffs, bows) node, taken from the world into the tracking space and from
// there into the frame the hands' places are in (frame.body). Read every
// third frame while a weapon is sheathed and kept in between: it walks the
// body's tree for the two nodes.

#include "core/Config.h"
#include "vr/HandMode.h"
#include "vr/OpenVRBackend.h"

namespace obvr::camera::hands {

// Fills frame.body.sheathValid / sheath. Called once the frame's head, hands
// and body frame are read (ReadFrameHeadAndHands).
void ReadSheathIntoFrame(vr::HandModeFrame& frame, const vr::OpenVRBackend& backend, const Config& config);

}  // namespace obvr::camera::hands
