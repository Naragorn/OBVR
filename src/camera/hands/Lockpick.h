#pragma once

// The lock's minigame by the beam and the sticks ([Hands] LockpickByHand,
// vr/LockpickLogic.h): while the LockPickMenu is on top in Full VR, the
// cursor the game is given is made from the tumbler the beam chose or a
// stick stepped to, and a stick pushed up sends the push. Runs after the
// hand mode's update and before the cursor is handed to the game
// (game::SetMenuCursorWanted) and the mouse steps go out
// (camera/hands/ControlsOut.h): it rewrites the result's cursor and steps.
// The game's own tutorial box over the first lock is another menu on top:
// the beam clicks it away as on any menu.

#include "camera/hands/HandStep.h"
#include "vr/LockpickLogic.h"

namespace obvr::camera::hands {

struct LockpickHandState {
	vr::LockpickState logic;
	bool up = false;        // the minigame was ours last frame
	UInt32 linesLeft = 40;  // the choices, steps and pushes said
	// The pick's lowest and highest y since the last mark of a hand script
	// (a push throws it up and lets it fall): for the mark's line.
	bool pickSeen = false;
	float pickLowY = 0.0f;
	float pickHighY = 0.0f;
};

void StepLockpickByHand(LockpickHandState& state, const HandStep& step, const HandShared& shared,
                        const vr::HandModeFrame& frame);

}  // namespace obvr::camera::hands
