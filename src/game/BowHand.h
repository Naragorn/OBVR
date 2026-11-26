#pragma once

// A bow is the other hand's, like a torch (the tester, 2026-11-26, option (a)
// of free-weapons-spec "Open: which hand catches a bow": "yes"). The game holds
// a bow on the other hand's Torch node, so the other hand's grip takes it from
// the world - at its grip, or pulled in - it turns on the way into that hand's
// Torch node, and the take equips it there; the weapon hand only carries a bow,
// as the other hand carries a sword and the weapon hand a torch. Which hand,
// which hold and which side of a grab count for a thing is decided here, over
// plain values (covered by bow_hand_test); camera/hands/Grab.cpp reads the
// world and sends the calls.

#include "core/Types.h"

namespace obvr::game {

// The weapon type of a bow (TESObjectWEAP +0x90, game/WeaponGrab.h).
inline constexpr SInt32 kWeaponTypeBow = 5;

inline bool IsBowType(SInt32 weaponType) { return weaponType == kWeaponTypeBow; }

// A thing the other hand takes: a torch or a bow.
inline bool IsOtherHandsItem(bool torch, SInt32 weaponType) { return torch || IsBowType(weaponType); }

// The hold whose closing takes a weapon at its grip: the other hand's for a bow,
// the weapon hand's for any other weapon.
inline bool TakingHoldClosed(SInt32 weaponType, bool weaponHandHold, bool otherHandHold) {
	return IsBowType(weaponType) ? otherHandHold : weaponHandHold;
}

// Whether the hand a grab is with is the one that takes what it holds: the
// weapon hand a weapon, the other hand a bow (a weapon type below 0: no weapon).
inline bool GrabHandTakes(SInt32 weaponType, bool grabWithOtherHand) {
	return weaponType >= 0 && (IsBowType(weaponType) ? grabWithOtherHand : !grabWithOtherHand);
}

}  // namespace obvr::game
