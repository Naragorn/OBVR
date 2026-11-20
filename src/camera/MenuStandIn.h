#pragma once

// Which frames the menu's camera stand-in opens (CameraHook.cpp,
// PrepareMenuFrameIfNeeded): a menu frame the engine renders by itself has no
// camera pass, so the stand-in takes the pose - the frame's one
// WaitGetPoses - at the start of the render.
//
// The frame a menu closes on is one more of those: its camera update ran while
// the world was still paused (no camera pass), and by its render the menu is
// gone. Before 2026-11-20 nothing opened it; its present then waited for the
// poses itself, and the next frame's camera pass waited a second time - a
// 1.5 ms frame, then a 26 ms one, at every close (tablet-open-hitch.txt,
// docs/inventory-tablet-spec.md 13). So the frame after one the stand-in armed
// with a menu up is stood in for too - once: armed without a menu, it does not
// carry on. Pure; covered by frame_logic_test.

namespace obvr::camera {

struct MenuStandInFrames {
	bool armedThisFrame = false;      // the stand-in opened this frame
	bool menuArmedThisFrame = false;  // ... with a menu up
	bool menuArmedLastFrame = false;  // the last frame's, with a menu up
};

// Whether this frame's render is the stand-in's to open, as far as the menu
// goes: a menu is up, or the last frame was a menu frame it opened.
inline bool StandInAsMenu(bool menuIsUp, const MenuStandInFrames& f) {
	return menuIsUp || f.menuArmedLastFrame;
}

// The stand-in opened this frame.
inline void ArmStandIn(MenuStandInFrames& f, bool menuIsUp) {
	f.armedThisFrame = true;
	f.menuArmedThisFrame = menuIsUp;
}

// At the present: whether the stand-in opened the frame being delivered; this
// frame's becomes the last frame's.
inline bool EndStandInFrame(MenuStandInFrames& f) {
	const bool armed = f.armedThisFrame;
	f.menuArmedLastFrame = f.menuArmedThisFrame;
	f.armedThisFrame = false;
	f.menuArmedThisFrame = false;
	return armed;
}

}  // namespace obvr::camera
