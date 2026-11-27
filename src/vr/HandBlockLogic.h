#pragma once

// The block the hands' pose makes this frame (moved out of HandMode::Update
// on 2026-11-26, when the run by swung arms got its say over it):
// - the left hand raised with a shield on that arm (the tester, 2026-09-29:
//   "linke hand heben wenn kein schild equiped, darf nicht als block
//   zählen"), or with a torch in it ([Hands] TorchBlocks) - LeftHandBlocks,
//   IsBlockGesture; the shield not while it guards by contact
//   ([Hands] ShieldBlockByContact, Nadi 2026-11-27, "as Blade & Sorcery":
//   the shield blocks the blows it is in the way of, at the blow -
//   game/ParryLogic.h ForceBlockForShield - so a raised shield never walks
//   the player; the pose is kept as `shield.raised` for the blows the disc
//   cannot judge);
// - else the weapon raised across the body with a weapon or the fists drawn
//   (IsWeaponGuard; with the block's angle rule any raised blade not pointing
//   ahead);
// - else, with the fists up, both raised in front (IsFistGuard);
// - none of them while the arms run by swinging with ArmSwingBalance on
//   (vr/ArmSwing.h ArmSwingHoldsBlocks): a pump takes the hands past the
//   block's height at every turn, and the engine walks a blocking player.
// The bow's guard is decided with the archery, in HandMode::Update.
//
// Pure; hand_mode_test.

#include "game/BlockAngleLogic.h"
#include "game/NiMath.h"
#include "vr/HandInput.h"

namespace obvr::vr {

struct HandBlockFrame {
	bool menuMode = false;
	bool leftValid = false;       // the controllers tracked
	bool rightValid = false;
	bool rightHandValid = false;  // the weapon hand placed (HandModeResult::rightHandValid)
	bool weaponDrawn = false;     // a weapon or the fists drawn (WeaponSeen::Drawn)
	bool fistsUp = false;         // nothing equipped: the fists
	bool shieldEquipped = false;
	bool torchBlocks = false;     // a torch in the left hand, [Hands] TorchBlocks on
	NiPoint3 leftRelative{0.0f, 0.0f, 0.0f};   // the hands from the head, metres (Oblivion axes)
	NiPoint3 rightRelative{0.0f, 0.0f, 0.0f};
	NiPoint3 blade{0.0f, 0.0f, 0.0f};          // the weapon hand's forward axis
	bool rightSwinging = false;
	bool leftSwinging = false;
	bool blockByAngle = false;    // [Hands] BlockByAngle
	bool armsRun = false;         // ArmSwingHoldsBlocks
	// The shield guards by contact (ShieldBlocksByContact): the raised
	// shield hand holds no Block.
	bool shieldByContact = false;
	// The drawn weapon guards by contact (HandBlockFrame.h,
	// WeaponBlocksByContact): its guard pose holds no Block.
	bool weaponByContact = false;
	// The held torch guards by contact (HandBlockFrame.h): its raise holds no Block.
	bool torchByContact = false;
};

struct HandBlock {
	bool blocking = false;
	game::BlockWith with = game::BlockWith::None;
	bool fistGuard = false;  // the block is both fists raised
	// The shield by contact this frame, and its hand raised as the pose
	// rule had it (game/BlockAngleLogic.h, ShieldGuardPose).
	game::ShieldGuardPose shield;
	// The weapon by contact, its guard pose and its swing (WeaponGuardPose).
	game::WeaponGuardPose weapon;
	// The torch by contact, its raise and the left hand's swing.
	game::WeaponGuardPose torch;
};

inline HandBlock DecideHandBlock(const HandBlockFrame& f, const GestureThresholds& t) {
	HandBlock b;
	if (f.menuMode) {
		return b;
	}
	b.shield.byContact = f.shieldByContact && f.shieldEquipped;
	b.weapon.byContact = f.weaponByContact && f.weaponDrawn;
	b.weapon.swinging = b.weapon.byContact && f.rightSwinging;
	b.torch.byContact = f.torchByContact && f.torchBlocks;
	b.torch.swinging = b.torch.byContact && f.leftSwinging;
	if (f.armsRun) {
		return b;
	}
	const bool raised = f.leftValid && IsBlockGesture(f.leftRelative, t);
	b.shield.raised = b.shield.byContact && raised;
	b.torch.raised = b.torch.byContact && raised;
	const bool shieldPoses = f.shieldEquipped && !b.shield.byContact;
	const bool torchPoses = f.torchBlocks && !b.torch.byContact;
	b.blocking = LeftHandBlocks(raised, shieldPoses, torchPoses);
	b.with = b.blocking ? LeftWithFor(shieldPoses, torchPoses) : game::BlockWith::None;
	if (!b.blocking && f.rightValid && f.rightHandValid && f.weaponDrawn) {
		const bool guard = IsWeaponGuard(f.rightRelative, f.blade, f.rightSwinging, t, f.blockByAngle);
		// By contact the guard pose is kept for the blows the blade cannot
		// judge and holds no Block (game/ParryLogic.h, ForceBlockForWeapon).
		b.weapon.raised = b.weapon.byContact && guard;
		b.blocking = guard && !b.weapon.byContact;
		b.with = b.blocking ? game::BlockWith::Weapon : b.with;
	}
	if (!b.blocking && f.rightValid && f.leftValid && f.fistsUp && f.weaponDrawn) {
		b.blocking = IsFistGuard(f.rightRelative, f.leftRelative, f.rightSwinging, f.leftSwinging, t);
		b.fistGuard = b.blocking;
		b.with = b.blocking ? game::BlockWith::Fists : b.with;
	}
	return b;
}

}  // namespace obvr::vr
