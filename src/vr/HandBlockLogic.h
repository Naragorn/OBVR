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
	// The fists guard by contact (HandBlockFrame.h): both forearms read.
	bool fistsByContact = false;
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
	// The fists by contact, both up as the pose had it, each arm's punch.
	game::FistGuardPose fists;
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
	b.fists.byContact = f.fistsByContact && f.fistsUp && f.weaponDrawn;
	b.fists.rightSwinging = b.fists.byContact && f.rightSwinging;
	b.fists.leftSwinging = b.fists.byContact && f.leftSwinging;
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
		const bool guard = IsFistGuard(f.rightRelative, f.leftRelative, f.rightSwinging, f.leftSwinging, t);
		// By contact a forearm in the way blocks at the blow (game/BlockCone
		// JudgeFists); both fists up stay the fallback, holding no Block.
		b.fists.raised = b.fists.byContact && guard;
		b.blocking = guard && !b.fists.byContact;
		b.fistGuard = b.blocking;
		b.with = b.blocking ? game::BlockWith::Fists : b.with;
	}
	return b;
}

// The bow's block (moved out of HandMode::Update on 2026-11-27, when the bow
// joined the blocks by contact): held upright before the face with no arrow
// at it (IsBowGuard) - none while a menu is up, the controller or the limb
// unread, or the arms run by swinging. By contact ([Hands]
// WeaponBlockByContact, the drawn bow's limbs read - game/BowVisual.h
// BowLimbLine) the guard holds no Block: the bow is the weapon by contact,
// its upright guard kept as `raised` for the blows its limbs cannot judge,
// and an arrow out (nocked, drawn) as `swinging` - no say then, as the pose
// rule has it.
struct BowBlockFrame {
	bool valid = false;      // the bow hand's controller and the limb's axis read
	bool menuMode = false;
	bool armsRun = false;    // ArmSwingHoldsBlocks
	NiPoint3 bowHandRelative{0.0f, 0.0f, 0.0f};
	bool limbRead = false;   // the limb's axis has a length
	float limbUpright = 0.0f;
	bool arrowOut = false;   // an arrow nocked or drawn (archery's state not None)
	bool byContact = false;  // WeaponBlockByContact and the limbs' line read
};

struct BowBlock {
	bool blocking = false;   // the pose holds Block (the rule off)
	game::WeaponGuardPose pose;
};

inline BowBlock DecideBowBlock(const BowBlockFrame& f, const GestureThresholds& t) {
	BowBlock b;
	b.pose.byContact = f.byContact && !f.menuMode;
	b.pose.swinging = b.pose.byContact && f.arrowOut;
	const bool guard = f.valid && !f.menuMode && !f.armsRun && f.limbRead &&
	                   IsBowGuard(f.bowHandRelative, f.limbUpright, f.arrowOut, t);
	b.pose.raised = b.pose.byContact && guard;
	b.blocking = guard && !b.pose.byContact;
	return b;
}

}  // namespace obvr::vr
