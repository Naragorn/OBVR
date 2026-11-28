#pragma once

// Which hand holds the tablet once it is open (docs/inventory-tablet-spec.md
// section 30; Nadi, headset 2026-11-28: "wenn rechts meine main hand ist
// sollte sie auf die linke gehen denn rechts brauche ich zum tippen/zeigen").
//
// Always the off hand - the left role, the left controller for a right-hander
// and the right one for a left-hander (vr::AssignHandRoles; the role follows
// the weapon's side, so after a handover to the other hand it is the hand
// without the weapon) - however it was opened: by the left B, or by the
// scroll at the inventory ring whichever hand let go once it was fully open.
// The weapon hand stays free to point, tap and scroll. Before 2026-11-28 the
// scroll left it in the hand that kept its grip (vr/TabletScroll.h
// StepUnrolling), so a right-hander letting go with the left had it in the
// right hand, pointed at by the left.
//
// Pure; covered by tablet_holder_test (and through the scroll by
// tablet_scroll_test and menu_place_test).

#include "core/Types.h"

namespace obvr::vr {

enum class TabletOpenedBy : UInt8 { Button, Scroll };

struct TabletHolder {
	bool weaponHand = false;    // held by the weapon hand (the right role): never
	bool onTop = false;         // held at its top edge (unrolled up and down), else its bottom or its side
	bool physicalLeft = true;   // on the left controller
};

// `offUp` and `weaponUp`: the two hands' heights as the scroll let go (read
// for the scroll only - the upper hand holds the top edge). `rolesSwapped`:
// the roles swapped this frame (vr::PhysicalLeftOf).
inline TabletHolder TabletHolderOnceOpen(TabletOpenedBy by, float offUp, float weaponUp, bool rolesSwapped) {
	TabletHolder h;
	h.weaponHand = false;
	h.onTop = by == TabletOpenedBy::Scroll && offUp > weaponUp;
	h.physicalLeft = !rolesSwapped;  // vr::PhysicalLeftOf(true, rolesSwapped)
	return h;
}

}  // namespace obvr::vr
