#pragma once

// Half-swording with a two-handed sword (Nadi, 2026-11-01: "Bauen wir nun half
// swording nur bei 2 hand Schwertern. Es gibt eine neue Greifposition an der
// klinge mit links gleich wie beim griff. Mit Annäherung der hand."). The left
// hand takes the blade as it takes the handle (vr/TwoHandLogic.h): shaped on
// the way in, closed in the game's own grip, slid with the trigger held. The
// blade is a second span of the handle above the right hand
// (HandleSpan::above, a staff's way), from past the guard to short of the tip.
// Two-handed blades only (weapon type 1): not a one-hander, not a blunt
// two-hander, not a staff (whose whole shaft is a handle already).
//
// Distances are game units along the weapon from the right palm, as in
// TwoHandLogic.h.
//
// Pure, covered by half_sword_test.

#include "core/Types.h"
#include "vr/TwoHandLogic.h"

namespace obvr::vr {

struct HalfSwordSettings {
	bool enabled = true;          // [Hands] HalfSwording
	float fromUnits = 12.0f;      // the blade from this far above the right palm (past the guard)
	float tipInsetUnits = 8.0f;   // to this far short of the tip
};

// The weapon type is a two-handed blade (0 Blade1H, 1 Blade2H, 2 Blunt1H,
// 3 Blunt2H, 4 Staff, 5 Bow).
inline bool HalfSwordWeapon(const HalfSwordSettings& s, SInt32 weaponType) { return s.enabled && weaponType == 1; }

// The handle with the blade added above the right hand: from fromUnits up to
// the tip (`tipAxial`, from the right palm) less tipInsetUnits. Unchanged when
// the handle was not measured, the tip is not a number, or the blade is too
// short to leave room between the two.
inline HandleSpan WithBlade(const HalfSwordSettings& s, HandleSpan h, float tipAxial) {
	if (!h.valid || !(tipAxial == tipAxial)) {
		return h;
	}
	const float from = s.fromUnits > 0.0f ? s.fromUnits : HalfSwordSettings{}.fromUnits;
	const float inset = s.tipInsetUnits >= 0.0f ? s.tipInsetUnits : HalfSwordSettings{}.tipInsetUnits;
	const float high = tipAxial - inset;
	if (!(high > from)) {
		return h;
	}
	h.above = true;
	h.aboveLow = from;
	h.aboveHigh = high;
	return h;
}

}  // namespace obvr::vr
