#pragma once

// Which hand holds the tablet once it is open (docs/inventory-tablet-spec.md
// sections 30 and 31.4; Nadi, headset 2026-11-28, the second run: "das inv
// soll egal rechtshänder oder link immer auf die hand gehen die am ende der
// geste NICHT loslässt").
//
// Opened by the scroll: the hand that keeps its grip as the other lets go at
// full width holds it, right- or left-handed, the weapon hand too - the
// other one points and taps. Both let go in the same frame, or no keeper to
// tell: the off hand, as for the B. Opened by the left B: the off hand - the
// left role, the left controller for a right-hander, the right one for a
// left-hander (vr::AssignHandRoles). The morning's rule of the same day was
// the off hand always; before it, the hand that kept its grip, as now.
//
// The grips are read in this frame's roles, so a switch of the sides during
// the gesture (a handover, vr/GripClaim.h) changes nothing: the hand that
// keeps its grip is the one that physically does.
//
// Pure; covered by tablet_holder_test (and through the scroll by
// tablet_scroll_test and menu_place_test).

#include "core/Types.h"

namespace obvr::vr {

enum class TabletOpenedBy : UInt8 { Button, Scroll };

struct TabletHolder {
	bool weaponHand = false;    // held by the frame's weapon role (the right role)
	bool onTop = false;         // held at its top edge (unrolled up and down), else its bottom or its side
	bool physicalLeft = true;   // on the left controller
};

// `offKept`, `weaponKept`: each role's grip still closed as the scroll is let
// go (read for the scroll only). `offUp`, `weaponUp`: the two hands' heights
// then - the holder held the top edge when it was the upper one.
// `rolesSwapped`: the roles swapped this frame (vr::PhysicalLeftOf).
inline TabletHolder TabletHolderOnceOpen(TabletOpenedBy by, bool offKept, bool weaponKept, float offUp, float weaponUp,
                                         bool rolesSwapped) {
	TabletHolder h;
	const bool scroll = by == TabletOpenedBy::Scroll;
	h.weaponHand = scroll && weaponKept && !offKept;
	h.onTop = scroll && (h.weaponHand ? weaponUp > offUp : offUp > weaponUp);
	h.physicalLeft = (!h.weaponHand) != rolesSwapped;  // vr::PhysicalLeftOf(!weaponHand, rolesSwapped)
	return h;
}

}  // namespace obvr::vr
