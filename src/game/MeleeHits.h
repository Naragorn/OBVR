#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// Strikes by motion: the swung controller is the attack.
//
// The engine decides a melee hit at a moment in the attack animation, in a
// cone in front of the actor. In the hand-tracked mode neither is right: the
// animation is hidden and the blade is wherever the hand is. So the mode
// draws the blade itself - the hand along the controller's pointing axis for
// the weapon's reach - tests it against the actors the engine is animating,
// and hands each one it touches to the engine's own hit function with that
// actor as the target. Everything after that is the engine's: the damage
// from weapon and skill, the power attack's factor, the block, the sneak
// attack, the enchantment, the crime, the script events. The geometry is in
// MeleeHit.h and tested there; this file is the engine side.

// Whether what the player holds is swung: a drawn blade, blunt weapon or
// bare fists. A bow, a staff or a sheathed weapon is not. Answers the
// weapon's type code as well (-1 for none) for the log.
bool MeleeInHand(SInt32* weaponType);

// The equipped weapon's form ID (TESForm+0x0C, xOBSE GameForms.h: typeID, flags,
// refID), 0 with none - for the test runner's console lines.
UInt32 EquippedWeaponFormId();

// The equipped weapon's base form, nullptr with none; `type` its
// WeaponTypeCode (None with no weapon).
UInt8* EquippedWeaponForm(SInt32* type);

// The weapon base form equipped on the player, as the game's EquipItem
// command does it (addr::kActorEquipItem with no extra data, count 1). For
// the holster gestures' weapon swap. False when the player or the form
// cannot be reached.
bool EquipWeaponForm(UInt8* weapon);

// Takes the equipped weapon off the player, as the game's UnequipItem command
// does, so the hands are bare (the fist's hand to hand). True once the call is
// made - whether the slot empties shows on the next frames. False with no weapon
// or when the worn stack cannot be found.
bool UnequipWeapon();

// Whether the player has died (Actor::IsDead through the same table entry the
// strike uses for its targets). False when the player cannot be read.
bool PlayerIsDead();

struct MotionStrike {
	UInt32 swingSerial = 0;  // the swing this frame belongs to
	bool heavy = false;      // a power attack
	// The right hand relative to the head, as HandMode answers it.
	NiMatrix33 handRotation = NiMatrix33::Identity();
	NiPoint3 handOffsetUnits{0.0f, 0.0f, 0.0f};
	// The world camera the offset is measured from - the one the grab by
	// reach measures its hand from. Without it no strike is tried.
	bool cameraValid = false;
	NiMatrix33 cameraRotation = NiMatrix33::Identity();
	NiPoint3 cameraPosition{0.0f, 0.0f, 0.0f};
	// How close to a body's bound centre the blade has to pass: this fraction
	// of the bound's radius, plus this many units.
	float boundFactor = 0.7f;
	float padUnits = 8.0f;
};

// Strikes every actor the blade meets this frame that this swing has not
// struck yet. Answers how many were struck.
UInt32 StrikeByMotion(const MotionStrike& strike);

// Forgets the swing's ledger, for when the mode stops.
void ForgetStrikes();

}  // namespace obvr::game
