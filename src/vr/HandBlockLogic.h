#pragma once

// The block the hands' pose makes this frame (moved out of HandMode::Update
// on 2026-11-26, when the run by swung arms got its say over it):
// - the left hand raised with a shield on that arm (the tester, 2026-09-29:
//   "linke hand heben wenn kein schild equiped, darf nicht als block
//   zählen"), or with a torch in it ([Hands] TorchBlocks) - LeftHandBlocks,
//   IsBlockGesture;
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
};

struct HandBlock {
	bool blocking = false;
	game::BlockWith with = game::BlockWith::None;
	bool fistGuard = false;  // the block is both fists raised
};

inline HandBlock DecideHandBlock(const HandBlockFrame& f, const GestureThresholds& t) {
	HandBlock b;
	if (f.armsRun || f.menuMode) {
		return b;
	}
	b.blocking = LeftHandBlocks(f.leftValid && IsBlockGesture(f.leftRelative, t), f.shieldEquipped, f.torchBlocks);
	b.with = b.blocking ? LeftWithFor(f.shieldEquipped, f.torchBlocks) : game::BlockWith::None;
	if (!b.blocking && f.rightValid && f.rightHandValid && f.weaponDrawn) {
		b.blocking = IsWeaponGuard(f.rightRelative, f.blade, f.rightSwinging, t, f.blockByAngle);
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
