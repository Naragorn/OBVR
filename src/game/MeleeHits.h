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

// The swish of the weapon in hand (or of the fists) at the player, the
// engine's own choice of sound (MeleeHit.h, SwishDue). False when nothing
// is swung or the engine's function is not the one read.
bool PlaySwingSwish();

// Whether the player wears a shield (the process's GetEquippedShieldData, worn
// only). When the getter is not the one read: true, the block as before.
bool PlayerWearsShield();

// The player's power attack grunt, the engine's own line and chance
// (vr::GruntDue). True when said.
bool PlayPowerAttackGrunt();

// The living actor (a Character or Creature, not the player, not dead)
// whose bound the point lies in - its radius times `factor` plus `padUnits`
// - the nearest if several; nullptr with none. `centreOut` gets its
// bound's centre. For the shove (game/Shove.h).
void* LivingActorAt(const NiPoint3& point, float factor, float padUnits, NiPoint3* centreOut,
                    float heightFactor = -1.0f);

// The living actor a ray points at: its bound within `coneCos` of the
// direction seen from `origin` (the angle to the bound's edge, as the
// laser's miss of an item is taken), no farther than `maxUnits` from the
// origin to its centre - the nearest when several; nullptr with none.
// `centreOut` gets its bound's centre. For the pick (an NPC under the laser
// takes it from the items) and the insult (game/Insult.h).
void* ActorUnderRay(const NiPoint3& origin, const NiPoint3& direction, float maxUnits, float coneCos,
                    NiPoint3* centreOut);

// The equipped weapon's form ID (TESForm+0x0C, xOBSE GameForms.h: typeID, flags,
// refID), 0 with none - for the test runner's console lines.
UInt32 EquippedWeaponFormId();

// The equipped weapon's base form, nullptr with none; `type` its
// WeaponTypeCode (None with no weapon).
UInt8* EquippedWeaponForm(SInt32* type);

// The weapon form's weight, the item's own as the inventory shows it (its
// TESWeightForm, addr::kWeaponWeightOffset); false for no form, or a figure
// that cannot be a weight - not a number, negative, a thousand or more.
bool WeaponWeightOf(const UInt8* weapon, float* weight);

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
	// The attack animation group the engine reads the strike as (vr::
	// PowerAttackGroup, kAnimGroupAttackLight); 0 leaves what is there.
	UInt8 attackGroup = 0;
	// The hand: 0 the weapon hand, 1 the other - which strikes only with the
	// fists (hand to hand). Each hand keeps its own ledger and held bodies.
	UInt32 hand = 0;
	// The arrow in the weapon hand with a bow drawn (vr::ArrowStabs): the
	// blade is the arrow from its nock to its head, in the world, and each
	// body it meets is struck at once - no power attack to wait for.
	// All three on the first-person skeleton (game::ArrowInHandWorld), carried
	// over to the world by the hand (ArrowBladeInWorld, handOffsetUnits).
	bool arrow = false;
	NiPoint3 arrowGrip{0.0f, 0.0f, 0.0f};
	NiPoint3 arrowNock{0.0f, 0.0f, 0.0f};
	NiPoint3 arrowHead{0.0f, 0.0f, 0.0f};
};

// Strikes every actor the blade meets this frame that this swing has not
// struck yet. Answers how many were struck.
UInt32 StrikeByMotion(const MotionStrike& strike);

// Strikes the bodies a swing met before it was a power attack, once it is
// one or is over (MeleeHit.h, SettleHeldStrike). Each frame, after
// StrikeByMotion.
void SettleHeldStrikes(UInt32 currentSerial, bool swingActive, bool swingPower, bool endedPower, UInt8 powerGroup,
                       UInt32 hand = 0);

// Forgets the swing's ledger, for when the mode stops.
void ForgetStrikes();

}  // namespace obvr::game
