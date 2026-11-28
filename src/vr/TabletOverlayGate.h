#pragma once

// What of the tablet's menus may show where, frame by frame
// (docs/inventory-tablet-spec.md section 31; Nadi, headset 2026-11-28: "beim
// schließen egal ob B oder geste sehe ich für einen frame das komplette
// inventar + texte vor mir aufploppen" and "wenn ich beide hände nah habe
// direkt am anfang der geste ... das inv + texte vor mir aufploppt"). Pure;
// covered by tablet_overlay_gate_test.
//
// The 2D layer is one texture: the HUD in the world, the menus under a menu.
// Three places show it - the tablet drawn into the eyes (render/TabletQuad.h),
// the overlay on the holding controller when the eyes did not get the
// tablet, and the room's panel ahead of the head. The decisions here keep
// the menus' picture on the tablet or nowhere.

#include "core/Types.h"

namespace obvr::vr {

// The share of the tablet drawn this frame: 0 once the scroll rolled it up
// or back and sent the close (`rolledUp`) - the menus are still up for a
// frame or two, and showing it open again there put the whole page in the
// hands for a frame (the B's unrolling clock, kept at open from the scroll);
// the scroll's own share while it stands between the hands (`scrollShare`
// >= 0); else the B's unrolling by time (`timedShare`).
inline float TabletDrawnShare(bool rolledUp, float scrollShare, float timedShare) {
	if (rolledUp) {
		return 0.0f;
	}
	return scrollShare >= 0.0f ? scrollShare : timedShare;
}

// The room's panel coming back after frames it was kept off for the tablet
// (its place hidden, or its menus' close): that first frame the overlay is
// handed the layer's new picture and kept hidden; it shows from the next.
// Shown at once it showed, for a frame, the picture it was last given - the
// menus, from a frame the tablet was on the overlay - and the layer of a
// frame whose capture could still hold the menus' last draw.
struct OverlayReturnState {
	bool heldLast = false;
};

// `held`: the overlay is kept off for the tablet this frame. True: prime it -
// the picture handed over, nothing shown.
inline bool StepOverlayReturn(OverlayReturnState& s, bool held) {
	const bool prime = s.heldLast && !held;
	s.heldLast = held;
	return prime;
}

}  // namespace obvr::vr
