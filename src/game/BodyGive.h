#pragma once

// The living give way where a hand or the blade presses or strikes them,
// the engine's side (game/BodyGiveLogic.h for what it is and is not).

#include "game/BodyGiveLogic.h"

namespace obvr::game {

struct GiveFrame {
	// [Hands] BodiesGive, and the moment allows it: Full VR in the world, no
	// menu.
	bool enabled = false;
	NiPoint3 around{0.0f, 0.0f, 0.0f};  // the eyes: the living within an arm's reach of them are looked at
	GivePusher pusher[kGivePushers];
	float dtSeconds = 0.0f;
	bool mark = false;  // a hand script's mark: the joints and the step's cost are logged
};

// Each world frame, after the actors' animation and before the render: the
// joints of everyone a pusher touches, or who is still swinging back, are
// stepped and their bones turned on top of the animation. Someone lying or
// getting up, dead, or gone from the high process is let go.
void StepBodyGive(const GiveFrame& frame);

// After the world is drawn: every bone written is given back, so the next
// animation blends from the game's own pose and not from the turn
// (game/BoneWrite.h).
void GiveBackBodyGive();

}  // namespace obvr::game
