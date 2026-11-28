#pragma once

#include "core/Types.h"
#include "vr/Holster.h"
#include "vr/WeaponWeight.h"

namespace obvr::vr {

// Every held item weighs, not only a swung weapon (Nadi, 2026-11-28: "all
// shields, bows, staffs now also must get weight as weapons have it. torches
// as well but give them a light weight"; docs/physical-combat-spec.md, "The
// weight of shields, bows, staffs and torches"). Which item in which hand
// lags, by which weight, and when it does not: pure, so every flow is checked
// in held_item_weight_test. The lag itself is the weapon's (vr/WeaponWeight.h)
// under the one slider ([Hands] WeaponWeight, WeaponSwingThrough).
//
// The hands: the weapon hand holds the swung weapon or the staff; the off
// hand the bow (OBVR draws the bow in the off hand and the string in the
// weapon hand, which never lags for it), the shield or the torch.
enum class WeighedItem : UInt8 { None, Weapon, Staff, Bow, Shield, Torch };

// The torch's weight for its lag. Not its LIGH form's (TESWeightForm at +0x60,
// the weight at +0x64): a torch weighs about 1, under kWeaponLagLightWeight,
// and would not lag at all. 12 is a guess for the headset - a light lag, a
// quarter of the way from the dagger to the heaviest weapon.
constexpr float kTorchLagWeight = 12.0f;

// What the weapon hand holds that weighs: a swung weapon drawn (a blade or a
// blunt one; the fists have no form, so no weight is read for them) or a staff
// drawn. A bow weighs in the off hand.
inline WeighedItem WeaponHandWeighed(bool meleeHeld, bool staffDrawn) {
	return meleeHeld ? WeighedItem::Weapon : staffDrawn ? WeighedItem::Staff : WeighedItem::None;
}

// What the off hand holds that weighs: the drawn bow first (with a bow drawn
// that hand is the bow's), then the worn shield, then the worn torch. An empty
// hand or a spell weighs nothing.
inline WeighedItem OffHandWeighed(bool bowDrawn, OffHandItem worn) {
	if (bowDrawn) {
		return WeighedItem::Bow;
	}
	return worn == OffHandItem::Shield ? WeighedItem::Shield
	       : worn == OffHandItem::Torch ? WeighedItem::Torch
	                                    : WeighedItem::None;
}

// The weight it lags by: the item's form's own (formRead, formWeight - the
// TESWeightForm of the WEAP or the ARMO), the torch kTorchLagWeight whatever
// its form says. None for nothing held, a weight not read, or one no item can
// have (not a number, below 0, 1000 or more - game::WeaponWeightOf's bounds).
inline float LagWeightOf(WeighedItem item, bool formRead, float formWeight) {
	if (item == WeighedItem::Torch) {
		return kTorchLagWeight;
	}
	if (item == WeighedItem::None || !formRead || !(formWeight >= 0.0f && formWeight < 1000.0f)) {
		return 0.0f;
	}
	return formWeight;
}

// Whether the weapon hand's drawn pose lags: a weight, a drawn hand (tracked,
// first person), and not while the hands are adjusted - the fit measures the
// controller.
inline bool WeaponHandLags(float weight, bool handValid, bool adjusting) {
	return weight > 0.0f && handValid && !adjusting;
}

// Whether the off hand's drawn pose lags this frame, and why not.
struct OffHandLagGate {
	float weight = 0.0f;     // LagWeightOf
	bool handValid = false;  // tracked, in first person: a drawn hand to lag
	bool adjusting = false;  // the hands being adjusted: the fit measures the controller
	bool menuMode = false;   // a menu up: the laser and the menus are the controller's
	bool tabletUp = false;   // the tablet held in that hand: held where the hand is
	bool onHandle = false;   // the hand on a weapon's handle: the handle places it
};

inline bool OffHandLags(const OffHandLagGate& g) {
	return g.weight > 0.0f && g.handValid && !g.adjusting && !g.menuMode && !g.tabletUp && !g.onHandle;
}

// The off hand's lag: the weapon's curve for a one-hander of that weight at
// the slider's setting - never the two-hander's heavier one, never stiffened
// by a second hand - made heavier when exhausted as the weapon is.
inline WeaponLagTuning OffHandLagTuning(float weight, float percent, bool swingThrough, float exhaustionEase,
                                        float exhaustionFactor) {
	return WeaponLagExhausted(WeaponLagFor(weight, percent, false, swingThrough), exhaustionEase, exhaustionFactor);
}

// For the log's once-per-equip line.
inline const char* WeighedItemWords(WeighedItem item) {
	switch (item) {
	case WeighedItem::Weapon:
		return "a weapon";
	case WeighedItem::Staff:
		return "a staff";
	case WeighedItem::Bow:
		return "a bow";
	case WeighedItem::Shield:
		return "a shield";
	case WeighedItem::Torch:
		return "a torch";
	default:
		return "nothing";
	}
}

}  // namespace obvr::vr
