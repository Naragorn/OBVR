#pragma once

// The block's frame (vr/HandBlockLogic.h, HandBlockFrame) from what the hand
// mode's frame and settings carry: the parts that are not the hands' pose
// this frame. Moved out of HandMode::Update on 2026-11-27, when the shield by
// contact joined them. Pure; hand_mode_test.

#include "vr/HandBlockLogic.h"
#include "vr/HandMode.h"

namespace obvr::vr {

// The shield guards by contact ([Hands] ShieldBlockByContact; Nadi,
// 2026-11-27): the rule on, a shield worn and its disc read from the model.
// Until the disc is read the raised shield blocks by the pose, as before,
// so a shield is never without a block.
inline bool ShieldBlocksByContact(bool settingOn, bool shieldEquipped, bool discRead) {
	return settingOn && shieldEquipped && discRead;
}

inline HandBlockFrame HandBlockFrameOf(const HandModeFrame& f, const HandSettings& s) {
	HandBlockFrame bf;
	bf.menuMode = f.menuMode;
	bf.leftValid = f.left.valid;
	bf.rightValid = f.right.valid;
	bf.weaponDrawn = f.weaponSeen == WeaponSeen::Drawn;
	bf.fistsUp = f.equipped == EquippedKind::Nothing;
	bf.shieldEquipped = f.shieldEquipped;
	bf.torchBlocks = s.torchBlocks && f.torchHeld;
	bf.blockByAngle = s.blockByAngle;
	bf.shieldByContact = ShieldBlocksByContact(s.shieldBlockByContact, f.shieldEquipped, f.shieldDiscRead);
	return bf;
}

}  // namespace obvr::vr
