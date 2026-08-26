#pragma once

namespace obvr::game {

// Whether Oblivion currently has a menu up.
//
// "Menu" here is the game's own word for it and is broader than it sounds: the
// main menu, the loading screen, the inventory, the ESC menu, a dialogue, and
// the intro films, which run inside the main menu. In all of them the world is
// paused and the interface is what the wearer is looking at.
//
// Why OBVR asks at all. A frame reaches the headset one of two ways: as an eye
// of a stereo pair, drawn from a live pose, or as a flat rectangle hanging in
// the room at a frozen pose. Which one it is was decided by whether the camera
// hook had run that frame - and that turns out to be the wrong question. With
// a menu open Oblivion still draws the world behind it on some frames and not
// on others, so the answer flipped from frame to frame and the presentation
// flipped with it: full world, small rectangle, full world. On a monitor none
// of that is visible. In a headset it is the menu jumping open and shut, which
// is what was reported.
//
// So the question is asked of the game instead of inferred from its timing.
// This is a plain call into Oblivion's own code, made from the thread the game
// is already on, and it reads state rather than changing any.
//
// There is nothing here for a test to hold: the answer comes from an address
// inside Oblivion.exe and a test process has no Oblivion. What is worth
// testing is what OBVR does with the answer, and that lives in FrameLogic,
// where it is a pure function of two booleans.
bool IsMenuMode();

}  // namespace obvr::game
