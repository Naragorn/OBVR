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

struct MotionStrike {
	UInt32 swingSerial = 0;  // the swing this frame belongs to
	bool heavy = false;      // a power attack
	// The right hand relative to the head, as HandMode answers it.
	NiMatrix33 handRotation = NiMatrix33::Identity();
	NiPoint3 handOffsetUnits{0.0f, 0.0f, 0.0f};
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
