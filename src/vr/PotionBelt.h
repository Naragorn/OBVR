#pragma once

// Potions kept at the holster places (Nadi, 2026-11-05: "go 3"; [Hands]
// PotionBelt). A potion held in the hand and let go at a place that keeps no
// weapon, shield or torch goes into the pack, as at the chest, and the place
// keeps its form; a grip closing there with an empty hand drops one of it into
// that hand (the inventory grab's drop, game::DropItemForm) and the hand holds
// it - to the mouth to drink (vr/Stow.h). One potion kind per place, one place
// per kind; a weapon kept at a place takes it over (the potion stays in the
// pack). The places are the holster's (vr/Holster.h SlotAt), so the hips and
// the shoulders hold potions when nothing else is there.
//
// Pure, covered by potion_belt_test; the release at a place is the stow's
// (StowInput::potionPlace).

#include "core/Types.h"
#include "vr/Holster.h"

namespace obvr::vr {

// The place a held potion goes to when let go with the hand at `handSlot`:
// that place, when it keeps no holster item and no other potion; None else.
inline HolsterSlot PotionPlaceFor(HolsterSlot handSlot, const UInt32 (&holsterIds)[kHolsterSlotCount],
                                  const UInt32 (&potionIds)[kHolsterSlotCount], UInt32 potion) {
	const UInt32 at = static_cast<UInt32>(handSlot);
	if (potion == 0 || at == 0 || at >= kHolsterSlotCount || holsterIds[at] != 0) {
		return HolsterSlot::None;
	}
	return potionIds[at] == 0 || potionIds[at] == potion ? handSlot : HolsterSlot::None;
}

// `potion` kept at `slot`, and at no other place.
inline void KeepPotionAt(UInt32 (&potionIds)[kHolsterSlotCount], HolsterSlot slot, UInt32 potion) {
	const UInt32 at = static_cast<UInt32>(slot);
	if (potion == 0 || at == 0 || at >= kHolsterSlotCount) {
		return;
	}
	for (UInt32 i = 1; i < kHolsterSlotCount; ++i) {
		if (potionIds[i] == potion) {
			potionIds[i] = 0;
		}
	}
	potionIds[at] = potion;
}

// The potions at places a holster item now keeps, forgotten. Answers how many.
inline UInt32 ForgetPotionsUnderItems(UInt32 (&potionIds)[kHolsterSlotCount],
                                      const UInt32 (&holsterIds)[kHolsterSlotCount]) {
	UInt32 forgotten = 0;
	for (UInt32 i = 1; i < kHolsterSlotCount; ++i) {
		if (potionIds[i] != 0 && holsterIds[i] != 0) {
			potionIds[i] = 0;
			++forgotten;
		}
	}
	return forgotten;
}

// Whether the holster's reach at a place did something itself (sheathe,
// draw, stow, take, or a refusal it shows): then the potion is not drawn by
// the same grip. A reach that found the place empty leaves it to the potion.
inline bool HolsterTookTheReach(SlotOutcome outcome) {
	return outcome != SlotOutcome::None && outcome != SlotOutcome::Empty;
}

struct PotionReachInput {
	bool allowed = false;    // Full VR, in the world, no menu, PotionBelt on
	bool closedNow = false;  // the hand's grip closed this frame
	bool handBusy = false;   // the hand holds something, or the holster took the reach
	HolsterSlot slot = HolsterSlot::None;  // the place the hand is at
};

// The potion the reach draws (its form id), 0 for none.
inline UInt32 PotionReachAt(const PotionReachInput& in, const UInt32 (&potionIds)[kHolsterSlotCount]) {
	const UInt32 at = static_cast<UInt32>(in.slot);
	if (!in.allowed || !in.closedNow || in.handBusy || at == 0 || at >= kHolsterSlotCount) {
		return 0;
	}
	return potionIds[at];
}

}  // namespace obvr::vr
