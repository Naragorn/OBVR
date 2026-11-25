#pragma once

// The game's attack control in Full VR: when a swing presses it, and when the
// weapon hand's trigger is kept from it. The control itself goes out with the
// other controls (vr::PlanHandControls, game::ApplyHandControls).
//
// Pure, covered by hand_mode_test.

#include "vr/HandInput.h"
#include "vr/Holster.h"

namespace obvr::vr {

// Whether a swing presses the attack control. Not when the swung weapon
// strikes by motion (the blade itself hits), and not with the weapon away:
// the attack control readies a sheathed weapon (a player on the gamesas
// forum, 2011: "if my weapon was sheathed, one left-click would draw
// (ready) my weapon"), so a hand moved quickly to reach for or throw
// something drew the fists (2026-09-25). Not while a grip is closed either:
// that hand is holding or reaching, not striking.
//
// `strikeByMotion` means: the swung thing belongs to the strike by motion
// (motionHits on, a melee weapon or the fists in hand) - whether this swing
// may strike or not. An open hand with the fists up may not strike
// (FistAllowsStrike), and it must not fall back to the attack control
// either: that ran vanilla's hand-to-hand attack, its sound and its step
// forward, for a hand waved through empty air (the tester, 2026-09-29: "wenn
// ich die touch controller schwinge im leeren raum kommt ein h2h attacke
// mit attack sound und mit ausfallschritt (nausea)"). SwingMayPressAttack
// is what HandMode asks.
inline bool SwingPressesAttack(bool strikeByMotion, bool weaponDrawn, bool gripHeld) {
	return !strikeByMotion && weaponDrawn && !gripHeld;
}

inline bool SwingMayPressAttack(bool motionHits, bool meleeInHand, bool weaponDrawn, bool gripHeld) {
	return SwingPressesAttack(motionHits && meleeInHand, weaponDrawn, gripHeld);
}

// Whether the weapon hand's trigger is kept from the attack control because
// what the player fights with is away. The attack control readies it (above),
// so a trigger pulled with the sword sheathed drew the sword - and on a Touch
// controller a fist is the grip and the trigger together, so a hand closed to
// box brought the equipped weapon into it instead (a Quest 2 tester,
// 2026-11-25: "Stop equipped weapon popping into hand when right trigger is
// pressed"; the fist's own way, vr/Fist.h, takes a sheathed weapon off and
// raises the fists, and never got that far).
//
// What is away has a way out of its own: a weapon from its place on the body
// (Holsters) or by the ready-weapon input (Holsters off), the bare fists by
// making a fist (`fistsByFist`: [Hands] Fists on and the controller reading
// the fingers, FistVerdict::known) or by that input. Only where there is
// none - nothing equipped, no fist to be made, and Holsters on, where the
// ready-weapon input opens the Rest menu - the trigger still raises the
// fists. With the weapon's state unreadable the trigger attacks as before.
inline bool TriggerHeldBackAway(WeaponSeen seen, EquippedKind equipped, bool fistsByFist, bool holsters) {
	if (seen != WeaponSeen::Sheathed) {
		return false;
	}
	const bool onlyTheTrigger = equipped == EquippedKind::Nothing && !fistsByFist && holsters;
	return !onlyTheTrigger;
}

}  // namespace obvr::vr
