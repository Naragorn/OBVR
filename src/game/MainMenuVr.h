#pragma once

// The "VR" button in the main menu and the ask it raises (ui/MainMenuButton.h):
// the button put into each main menu the game builds, its click noticed, the
// start guide asked for again. The ask itself - RequestStartGuide,
// StartGuideAsked, ClearStartGuideAsked - is declared with the guide's host
// (game/NativeMenuPrototype.h), which takes it.

namespace obvr::game {

// Once a frame, on the game's thread (render/StageHost.cpp). `enabled`:
// [Onboarding] MainMenuButton. Nothing is done outside the main menu.
void StepMainMenuVrButton(bool enabled);

// A hand's click went out with the cursor on this tile (camera/hands/ControlsOut.cpp).
void NoteMenuClickTile(const char* tileName);

}  // namespace obvr::game
