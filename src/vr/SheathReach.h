#pragma once

// The sheathed weapon the game draws on the player's body can always be taken
// (Nadi, 2026-11-16: "die ingame gezeichnete waffe am player body [muss] immer
// greifbar sein egal wie der body ausgerichtet ist oder ob er nicht im holster
// spot ist"). The places (vr/Holster.h) sit where the settings put them, in
// the body's frame; the weapon one sees hangs on the drawn body, which may be
// turned another way or hang the weapon outside its place's circle. So a grip
// that closes at the seen weapon's hilt draws it, whatever the places say
// about that spot - unless the place the hand is in has its own word for the
// grip that matters more:
//   * the place draws (the same weapon), or equips and draws the weapon it
//     keeps: that stands;
//   * the place is empty, or keeps something of the other hand's (WrongHand),
//     or the hand is at no place: the seen weapon is drawn.
// The weapon hand takes a one- or two-hander, the other hand the bow, as at
// the places. Only while sheathed: a drawn weapon is not on the body.
// Pure, covered by holster_test.

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/Holster.h"

namespace obvr::vr {

// How close to the hilt the gripping hand must be, metres: a hand's reach
// around a grip, a little under a place's own radius (0.18 to 0.22).
inline constexpr float kSheathReachMetres = 0.17f;

struct SheathReach {
	bool valid = false;  // the body is drawn with the sheathed weapon on it, its hilt read this frame
	NiPoint3 relative{0.0f, 0.0f, 0.0f};  // the hilt in the frame the hands are in (PlaceRelative), metres
};

inline bool SheathReached(const SheathReach& sheath, bool handValid, const NiPoint3& handRelative) {
	if (!sheath.valid || !handValid) {
		return false;
	}
	return (handRelative - sheath.relative).LengthSquared() <= kSheathReachMetres * kSheathReachMetres;
}

// StepHolster, then the seen weapon's own reach. The verdict's `fromBody`
// is set when the draw came from the hilt and not from a place.
inline HolsterVerdict StepHolsterAndSheath(HolsterState& s, const HolsterInput& in, const HolsterSettings& settings,
                                           const SheathReach& sheath) {
	const bool rightPress = in.rightGrip && !s.rightGripWas;
	const bool leftPress = in.leftGrip && !s.leftGripWas;
	HolsterVerdict v = StepHolster(s, in, settings);
	if (!settings.enabled || !in.allowed || in.seen != WeaponSeen::Sheathed) {
		return v;
	}
	const SlotItem item = SlotItemOf(in.equipped);
	const bool bow = item == SlotItem::Bow;
	if (!bow && !WeaponHandItem(item)) {
		return v;
	}
	const bool press = bow ? leftPress : rightPress;
	if (!press || !SheathReached(sheath, bow ? in.leftValid : in.rightValid, bow ? in.leftRelative : in.rightRelative)) {
		return v;
	}
	// What the places did with this frame's grips stands when it acts.
	const bool placeSpoke = v.outcome != SlotOutcome::None && v.gestureLeft == bow;
	const bool placeYields = !placeSpoke || v.outcome == SlotOutcome::Empty || v.outcome == SlotOutcome::WrongHand;
	if (!placeYields || v.readyClick || v.equip != HolsterSlot::None || v.offHandStow != HolsterSlot::None ||
	    v.offHandTake != HolsterSlot::None) {
		return v;
	}
	v.readyClick = true;
	s.drawAfterEquip = HolsterSlot::None;
	if (bow) {
		s.leftClaimed = true;
		s.leftTook = true;
		v.leftMissed = false;
	} else {
		s.rightClaimed = true;
		v.rightMissed = false;
	}
	v.rightClaimed = s.rightClaimed;
	v.leftClaimed = s.leftClaimed;
	v.leftTook = s.leftTook;
	v.gesture = in.equippedSlot;
	v.gestureLeft = bow;
	v.outcome = SlotOutcome::Draw;
	v.fromBody = true;
	return v;
}

}  // namespace obvr::vr
