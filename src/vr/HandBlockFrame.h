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

// The block's angle rule's settings from [Hands] (game/BlockAngleLogic.h),
// with the shield by contact's own reach and margin.
inline game::BlockAngleSettings BlockAngleSettingsOf(const HandSettings& h) {
	game::BlockAngleSettings angle;
	angle.enabled = h.blockByAngle;
	angle.bladeDegrees = h.blockBladeDegrees;
	angle.shieldDegrees = h.blockShieldDegrees;
	angle.reachUnits = h.blockReachUnits;
	angle.bodyDegrees = h.blockBodyDegrees;
	angle.interposeUnits = h.blockInterposeUnits;
	angle.shieldReachUnits = h.contactBlock.shieldReachUnits;
	angle.shieldInterposeUnits = h.contactBlock.shieldInterposeUnits;
	return angle;
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
	bf.shieldByContact = ShieldBlocksByContact(s.contactBlock.shield, f.shieldEquipped, f.guardReads.shieldDisc);
	// The weapon by contact ([Hands] WeaponBlockByContact): on, and the drawn
	// melee weapon's blade read (the angle rule's span), or the drawn staff's line
	// (game/StaffLine.h); the fists or a line not read keep the pose. Until then
	// its guard pose holds Block.
	bf.weaponByContact = s.contactBlock.weapon && (f.guardReads.weaponBlade || f.guardReads.staffLine);
	// The torch by contact (the same switch): TorchBlocks on, a torch held and its
	// shaft read in the world (game/TorchShaftLogic.h); until then its raise holds Block.
	bf.torchByContact = s.contactBlock.weapon && s.torchBlocks && f.torchHeld && f.guardReads.torchShaft;
	// The fists by contact: both forearms read with the fists readied (game/ForearmLines.h).
	bf.fistsByContact = s.contactBlock.weapon && f.guardReads.forearms;
	return bf;
}

// The block the pose made, into the hand mode's result.
inline void CopyHandBlock(HandModeResult& r, const HandBlock& block) {
	r.blocking = block.blocking;
	r.blockWith = block.with;
	r.fistGuard = block.fistGuard;
	r.shieldPose = block.shield;
	r.weaponPose = block.weapon;
	r.torchPose = block.torch;
	r.fistPose = block.fists;
}

// The guards the frame's block judges with (game/BlockCone, SetBlockGuard):
// the pose's while one holds Block, else the shield and the weapon by
// contact (game/BlockAngleLogic.h, GuardWithFor), with their poses.
inline void GuardPosesInto(const HandModeResult& hand, game::BlockGuard& g) {
	g.with = game::GuardWithFor(hand.blocking, hand.blockWith, hand.shieldPose.byContact, hand.weaponPose.byContact);
	g.shieldPose = hand.shieldPose;
	g.weaponPose = hand.weaponPose;
	g.torchPose = hand.torchPose;
	g.fistPose = hand.fistPose;
}

// The bow's block into the hand mode's result (vr/HandBlockLogic.h, DecideBowBlock):
// the pose's Block, or the bow as the weapon by contact.
inline void ApplyBowBlock(HandModeResult& r, const BowBlock& bow) {
	r.blocking = bow.blocking;
	r.blockWith = bow.blocking ? game::BlockWith::Bow : game::BlockWith::None;
	r.bowGuard = bow.blocking;
	r.weaponPose = bow.pose;
}

}  // namespace obvr::vr
