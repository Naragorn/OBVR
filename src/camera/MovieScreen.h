#pragma once

// A film the engine plays, on the headset (DeliverFrame's movieOwnsScreen,
// camera/FrameLogic.h).
//
// The new game's intro (OblivionIntro.bik) is drawn into the back buffer
// while menus are up: Loading, then the RaceSex menu opened hidden behind the
// film. Every menu rule of DeliverFrame then kept it off the headset - over a
// pair held from a loaded game (Windows, 2026-11-11: "held", the 2D pass
// drawing nothing) the old world, and with none held the overlay menu with
// black eyes (Linux: "cinema+overlay") - so the wearer saw black for the whole
// film, and the trigger pulled at the black skipped it. On a frame with no
// world render a film that is the picture takes the cinema screen, before
// every menu rule; a frame that drew the world stays stereo.

namespace obvr::camera {

// Whether a film the engine plays is the picture: one is playing
// (game::EngineMoviePlaying) and the main menu is not in the menu stack - the
// main menu's looping map film plays behind it, and the main menu keeps its
// own delivery (CinemaOverlay before any world, 2026-10-21). Pure.
constexpr bool MovieOwnsScreen(bool moviePlaying, bool mainMenuInStack) {
	return moviePlaying && !mainMenuInStack;
}

}  // namespace obvr::camera
