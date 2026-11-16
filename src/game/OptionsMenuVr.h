#pragma once

// "VR Options" and "VR Guide" in the game's Options menu and what their
// clicks raise (ui/OptionsVrButtons.h): the buttons put into each Options
// menu the game builds, its frame made taller for them, their clicks noticed -
// OBVR's own menu opened, or the start guide asked for again. The guide's ask
// itself - RequestStartGuide, StartGuideAsked, ClearStartGuideAsked - is
// declared with the guide's host (game/NativeMenuPrototype.h), which takes it.

namespace obvr::game {

// Once a frame, on the game's thread (render/StageHost.cpp). `enabled`:
// [Onboarding] OptionsButtons. Nothing is done without an Options menu.
void StepOptionsVrButtons(bool enabled);

// A hand's click went out with the cursor on this tile (camera/hands/ControlsOut.cpp).
void NoteMenuClickTile(const char* tileName);

}  // namespace obvr::game
