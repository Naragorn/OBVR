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
//
// The same for a weapon kept at a hip and hung there by OBVR (Model B at the
// hips, game/KeptOnBodyLogic.h): the weapon hand's grip at its hilt equips
// and draws it, as the place's own reach does (StepHolster's EquipDraw) -
// with no weapon drawn, and unless the place the hand is in has its word.
// The seen sheathed weapon goes first; of two kept hilts the nearer.
// Pure, covered by holster_test.

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/BodyFrame.h"
#include "vr/Holster.h"

namespace obvr::vr {

// How close to the hilt the gripping hand must be, metres: a hand's reach
// around a grip, a little under a place's own radius (0.18 to 0.22).
inline constexpr float kSheathReachMetres = 0.17f;

struct SheathReach {
	bool valid = false;  // the body is drawn with the sheathed weapon on it, its hilt read this frame
	NiPoint3 relative{0.0f, 0.0f, 0.0f};  // the hilt in the frame the hands are in (PlaceRelative), metres
	bool keptValid[2] = {false, false};    // a weapon kept at the left (0), the right (1) hip hangs there
	NiPoint3 kept[2] = {};                 // its hilt, as `relative`
};

inline SheathReach SheathReachOf(const BodyFrame& body) {
	SheathReach r;
	r.valid = body.sheathValid;
	r.relative = body.sheath;
	for (UInt32 hip = 0; hip < 2; ++hip) {
		r.keptValid[hip] = body.keptValid[hip];
		r.kept[hip] = body.kept[hip];
	}
	return r;
}

inline bool SheathReached(const SheathReach& sheath, bool handValid, const NiPoint3& handRelative) {
	if (!sheath.valid || !handValid) {
		return false;
	}
	return (handRelative - sheath.relative).LengthSquared() <= kSheathReachMetres * kSheathReachMetres;
}

// Whether the places let a hilt's reach act on this frame's grip of one hand
// (`left`): they did nothing with it, or only found it empty or the wrong hand.
inline bool PlacesYieldToHilt(const HolsterVerdict& v, bool left) {
	const bool placeSpoke = v.outcome != SlotOutcome::None && v.gestureLeft == left;
	const bool placeYields = !placeSpoke || v.outcome == SlotOutcome::Empty || v.outcome == SlotOutcome::WrongHand;
	return placeYields && !v.readyClick && v.equip == HolsterSlot::None && v.offHandStow == HolsterSlot::None &&
	       v.offHandTake == HolsterSlot::None;
}

// The kept hip whose hilt the weapon hand's grip closed at: the nearer within
// reach, None for none.
inline HolsterSlot KeptHiltReached(const SheathReach& sheath, const HolsterInput& in) {
	HolsterSlot best = HolsterSlot::None;
	float bestDistance = kSheathReachMetres * kSheathReachMetres;
	for (UInt32 hip = 0; in.rightValid && hip < 2; ++hip) {
		const float d = (in.rightRelative - sheath.kept[hip]).LengthSquared();
		if (sheath.keptValid[hip] && d <= bestDistance) {
			best = hip == 0 ? HolsterSlot::HipLeft : HolsterSlot::HipRight;
			bestDistance = d;
		}
	}
	return best;
}

// The weapon kept at a hip and seen there taken at its hilt: equipped, then
// drawn once the game shows it (StepHolster's EquipDraw). False when it does
// nothing: no press, a weapon drawn, no kept hilt in reach, the place no longer
// keeping a weapon of the weapon hand's, or the places having their word.
inline bool TakeKeptAtHilt(HolsterState& s, const HolsterInput& in, const HolsterSettings& settings,
                           const SheathReach& sheath, bool rightPress, HolsterVerdict& v) {
	if (!rightPress || in.seen == WeaponSeen::Drawn) {
		return false;
	}
	const HolsterSlot slot = KeptHiltReached(sheath, in);
	if (slot == HolsterSlot::None || !WeaponHandItem(in.slotItem[static_cast<UInt32>(slot)]) ||
	    !PlacesYieldToHilt(v, false)) {
		return false;
	}
	v.equip = slot;
	s.drawAfterEquip = slot;
	s.drawWait = settings.drawWaitSeconds;
	s.rightClaimed = true;
	v.rightClaimed = true;
	v.rightMissed = false;
	v.gesture = slot;
	v.gestureLeft = false;
	v.outcome = SlotOutcome::EquipDraw;
	v.fromBody = true;
	return true;
}

// StepHolster, then the seen weapon's own reach, then a kept hip weapon's. The
// verdict's `fromBody` is set when the draw came from a hilt and not from a place.
inline HolsterVerdict StepHolsterAndSheath(HolsterState& s, const HolsterInput& in, const HolsterSettings& settings,
                                           const SheathReach& sheath) {
	const bool rightPress = in.rightGrip && !s.rightGripWas;
	const bool leftPress = in.leftGrip && !s.leftGripWas;
	HolsterVerdict v = StepHolster(s, in, settings);
	if (!settings.enabled || !in.allowed) {
		return v;
	}
	const SlotItem item = SlotItemOf(in.equipped);
	const bool bow = item == SlotItem::Bow;
	const bool press = bow ? leftPress : rightPress;
	if (in.seen != WeaponSeen::Sheathed || (!bow && !WeaponHandItem(item)) || !press ||
	    !SheathReached(sheath, bow ? in.leftValid : in.rightValid, bow ? in.leftRelative : in.rightRelative) ||
	    !PlacesYieldToHilt(v, bow)) {
		TakeKeptAtHilt(s, in, settings, sheath, rightPress, v);
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
