#pragma once

#include "core/Types.h"
#include "game/TakeItem.h"
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

// The other hand's items, worn: the shield's base form (GetEquippedShieldData)
// and the torch's (GetEquippedLightData); nullptr with none, or when the
// getter is not the one read (logged once).
UInt8* WornShieldForm();
UInt8* WornTorchForm();

// The player's power attack grunt, the engine's own line and chance
// (vr::GruntDue). True when said.
bool PlayPowerAttackGrunt();

// The living actor (a Character or Creature, not the player, not dead)
// whose bound the point lies in - its radius times `factor` plus `padUnits`
// - the nearest if several; nullptr with none. `centreOut` gets its
// bound's centre. For the shove (game/Shove.h).
void* LivingActorAt(const NiPoint3& point, float factor, float padUnits, NiPoint3* centreOut,
                    float heightFactor = -1.0f);

// Whether a remembered actor is still a living Character or Creature (its
// reference checked before by game/RefFingerprint.h): the torch's blow held
// to the swing's end lands only on one (CameraHook, game/TorchBlowLogic.h).
bool ActorStillLiving(void* actor);

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

// The kick's damage (game/KickLogic.h, KickAnswersFists): the engine's own
// melee hit on `actor` as a fist - while it runs the player's equipped weapon
// reads as none - a power attack with `power`. False when refused (the hit
// function or the getter not as read, no high process); the log says why.
bool KickStrike(void* actor, bool power);

// The weapon form's weight, the item's own as the inventory shows it (its
// TESWeightForm, addr::kWeaponWeightOffset); false for no form, or a figure
// that cannot be a weight - not a number, negative, a thousand or more.
bool WeaponWeightOf(const UInt8* weapon, float* weight);

// Takes a swing's fatigue off the player with the engine's own cost and
// change functions (addr::kAttackFatigueCost, addr::kActorChangeFatigue;
// game::SwingFatigueFor decides when and with which weight), dearer with a
// two-hander in one hand (game/FatigueCost.h). `cost` gets what was
// charged. False when the player or the two functions are not as read.
bool ChargeSwingFatigue(float weight, bool power, float* cost);

// The weapon base form equipped on the player, as the game's EquipItem
// command does it (addr::kActorEquipItem with no extra data, count 1). For
// the holster gestures' weapon swap. False when the player or the form
// cannot be reached.
bool EquipWeaponForm(UInt8* weapon);

// Any item's base form equipped on the player the same way - a shield, a
// torch - with the game's item sound unless `silent`.
bool EquipItemForm(UInt8* item, bool silent);

// game::TakeIntoInventory with the item's pickup sound silenced the way the
// equip is (the item-sound picker nopped for the call): a weapon taken off
// its grip - pulled in, or carried by the blade - makes no sound; one taken
// at its grip keeps the game's pickup sound (the tester, 2026-10-24: "beim
// aufheben am griff wäre video gamey was ok ist. nicht am griff sollte
// keiner kommen").
TakeResult TakeIntoInventoryQuietly(UInt32 ref, UInt32* ownerId);

// Takes the equipped weapon off the player, as the game's UnequipItem command
// does, so the hands are bare (the fist's hand to hand). True once the call is
// made - whether the slot empties shows on the next frames. False with no weapon
// or when the worn stack cannot be found.
bool UnequipWeapon();

// Any worn item taken off the same way - a shield, a torch - with the game's
// item sound unless `silent`. False when the item is not worn.
bool UnequipItemForm(UInt8* item, bool silent);

// Whether the item's stack is worn in the player's inventory (equipped),
// whether or not the game shows it in the hand - a shield with the fists out
// is worn and off the hand.
bool PlayerWearsItem(UInt8* item);

// Whether the equipped weapon's base form is a quest item (TESForm flag
// 0x400): the game never lets one drop. False with no weapon.
bool EquippedWeaponIsQuestItem();

// Takes the equipped weapon off the player and drops it into the world at
// `at` (world units), through the player's own RemoveItem with its drop flag
// - the call the console's Drop makes, with the position given. Returns the
// new world reference, 0 when nothing was dropped (no weapon, the stack not
// found, the engine refusing). The reference has no Havok body yet: that
// comes frames later (game::RefBodyOf).
// With `angles` (the reference's three angles, radians, as the engine keeps
// them at +0x20; game/DropRotationLogic.h) the drop is asked to turn the new
// reference so - RemoveItem's turn argument; null leaves the engine its own.
UInt32 DropEquippedWeapon(const NiPoint3& at, const NiPoint3* angles = nullptr);

// The same for the torch in the other hand (WornTorchForm): taken off
// without the item sound, as a dropped weapon is (Nadi, 2026-10-26: "wenn
// ich die fackel fallen lasse dann ohne sound"), and dropped at `at`; the
// new reference or 0.
UInt32 DropEquippedTorch(const NiPoint3& at, const NiPoint3* angles = nullptr);

// One of any item the player carries dropped into the world at `at`, the
// same RemoveItem call (the inventory's grab into the hand, game/InventoryGrab.h):
// taken off first when it is worn - the weapon by UnequipWeapon, anything
// else silently. The new reference, 0 for none; a quest item is never
// dropped (the game refuses them too).
UInt32 DropItemForm(UInt8* item, const NiPoint3& at);

// The dropped reference turned and placed as the item was in the hand
// (game/DropRotationLogic.h): its angles and position written, and its 3D's
// when it has one. Answers how far its 3D's turn lay from the wanted one
// before (`beforeDegrees`) - what the engine made of the drop - and after;
// -1 for a 3D not there.
void PlaceDroppedRef(UInt32 ref, const NiMatrix33& rot, const NiPoint3& pos, float& beforeDegrees,
                     float& afterDegrees);

// How far a reference's 3D's turn lies from `rot`, degrees; -1 without a 3D.
float DroppedRefTurnOff(UInt32 ref, const NiMatrix33& rot);

// Whether the player has died (Actor::IsDead through the same table entry the
// strike uses for its targets). False when the player cannot be read.
bool PlayerIsDead();

struct BladeBodies;

// The fist's own blade for the hit by touch: this far from the hand along
// its pointing axis - the fist itself, not the hand-to-hand reach.
inline constexpr float kFistTouchUnits = 12.0f;

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
	// The hit by touch ([Hands] MotionHitsByTouch): the blade has to come
	// within touchUnits of one of the actor's capsules (BladeContactLogic.h,
	// the living, as the contact steps collected them this frame) instead of
	// the bound's ball; an actor with no capsule in the list is not hit. No
	// list, an empty one or the setting off: the bound's ball as before. A
	// fist tests kFistTouchUnits of itself, with half the hand's width more.
	const BladeBodies* capsules = nullptr;
	bool byTouch = false;
	float touchUnits = 4.0f;
	// The drawn blade's own span in the world (the contact step's: the
	// weapon's geometry, 25 degrees off the hand's pointing axis on a
	// longsword), for the touch; without it the blade along the hand's axis.
	bool touchSpanValid = false;
	NiPoint3 touchBase{0.0f, 0.0f, 0.0f};
	NiPoint3 touchTip{0.0f, 0.0f, 0.0f};
	// The attack animation group the engine reads the strike as (vr::
	// PowerAttackGroup, kAnimGroupAttackLight); 0 leaves what is there.
	UInt8 attackGroup = 0;
	// The strike at the touch (StrikeAtContact): the hand's speed this frame
	// (m/s), the speed that makes a power attack ([Hands] PowerStrikeSpeed),
	// the group such a one is struck as (the swing's direction so far), and
	// how far the swing has come (m), for the log.
	float speedNow = 0.0f;
	float powerSpeed = 0.0f;
	UInt8 powerGroup = 0;
	float swingMetres = 0.0f;
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

// Forgets the swing's ledger, for when the mode stops.
void ForgetStrikes();

}  // namespace obvr::game
