#pragma once

#include "core/Types.h"
#include "vr/TabletLogic.h"

// The frames the tablet's menus close on (docs/inventory-tablet-spec.md 24.2;
// Nadi, Windows 2026-11-26: "wenn ich das tablet schließe dann kam es noch im
// cinema mode vor mir schwebend kurz für paar frames. diese weg."). Pure;
// tablet_close_test.
//
// Measured (harness, 2026-11-26): from the menu key on, the game keeps menu
// mode on for about 17 frames (0.24 s at 72 Hz) with no menu in the stack
// but the HUD's, and its 2D pass still draws the menus as they fade out (108
// draws a frame; the tablet's 227, the world's HUD 27). The tablet needs the
// pages' entry to read its page, so it was put down at the first of them:
// the menus' place "as before" hung the room's overlay - those fading
// pictures - ahead of the head, 1.6 m wide, and the menu shade, off while
// the tablet is up, tinted the eyes sepia. Those frames are the tablet's
// still: nothing in the room, no shade - they show what the tablet shows
// once it is gone.

namespace obvr::vr {

// No menu in the stack but the HUD's: the pages have left it.
inline bool OnlyHudMenus(const UInt32* stack, UInt32 count) {
	for (UInt32 i = 0; stack != nullptr && i < count; ++i) {
		if (!IsHudMenu(stack[i])) {
			return false;
		}
	}
	return true;
}

struct TabletCloseState {
	bool up = false;       // the tablet in the hand last frame
	bool closing = false;  // this frame is one of the closing frames
	UInt32 frames = 0;     // the closing frames so far, 0 when not closing
};

// One frame: `tabletUp` the tablet in the hand now (vr::PlaceMenu),
// `menusGone` menu mode on with only the HUD's menus in the stack
// (TabletFrame::menusGone). Closing from a frame with the tablet up for as
// long as that lasts; another menu in the stack (the Esc menu over the
// pages) is no closing - its own place takes it - and neither is a menu
// that closes without the tablet having been up.
inline bool StepTabletClose(TabletCloseState& s, bool tabletUp, bool menusGone) {
	s.closing = !tabletUp && menusGone && (s.up || s.closing);
	s.frames = s.closing ? s.frames + 1 : 0;
	s.up = tabletUp;
	return s.closing;
}

}  // namespace obvr::vr
