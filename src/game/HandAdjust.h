#pragma once

namespace obvr::game {

// The hand-adjusting session that the guided window (ui/NativeHandAdjust.h)
// starts and ends, shared between the native menus, which run as an xOBSE
// task, and the hand mode, which runs in the frame. Both on the game's
// thread; the flags are atomic anyway, as the menus' own are.

// The guide asked for, from the settings menu's "Adjust hands" row. The
// native menus open it once the settings menu has closed.
void RequestHandAdjustGuide();
bool TakeHandAdjustGuideRequest();

void StartHandAdjust();
void StopHandAdjust();
bool HandAdjustActive();

// Every frame while the hand mode runs: which hands committed a fit this
// frame, and whether any grip is closed. Asks for the finish page once both
// hands are done and the grips have rested (ui::StepHandAdjustSession).
void NoteHandAdjustFrame(bool rightCommitted, bool leftCommitted, bool anyGripDown,
                         float dtSeconds);
bool TakeHandAdjustFinishRequest();

// Both hands back to the defaults - angles and grips - in the running config
// and the INI. False when the INI could not be written.
bool ResetHandsToDefaults();

}  // namespace obvr::game
