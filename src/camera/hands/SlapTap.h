#pragma once

// The mod's slap (Put it in its Place - Enhanced Grabbing, game/Shove.h): a
// grab tap with the pick on the slapped one - the pick there first, the
// grab down after (ShoveLogic.h, SlapGrabTapPressed), the body turned to
// face them, the mod's state logged over the tap's watch; and at Present
// the slapped one held facing the player and within the mod's reach
// (HoldSlapFacing).
//
// Moved out of camera/CameraHook.cpp on 2026-11-01 (docs/god-functions-plan.md,
// phase 2 step 2.16): the code as it was, its seven file globals now
// SlapTapState's members (the grab reach reads the tap's key as
// g_slapTap.pressed, the overlay pass its target). Harness: slap*.txt.

#include "camera/hands/HandStep.h"

namespace obvr::camera::hands {

struct SlapTapState {
	UInt32 frames = 0;
	bool pressed = false;  // the grab down this frame for the tap
	UInt32 actor = 0;
	UInt32 lines = 120;
	UInt32 watch = 0;  // the frames the log watches the mod's state after a tap
	NiPoint3 target{0.0f, 0.0f, 0.0f};
	UInt32 facingLines = 12;
};

// Turns the player's body to a heading (Oblivion's: zero at north,
// clockwise) the way the camera pass's own turns do, so the view holds
// still (camera/BodyYawHandOver.h); false when it could not be written.
using TurnBodyFn = bool (*)(float worldYaw);

// The tap itself, in the hand mode's step.
void StepSlapTap(SlapTapState& state, const HandStep& step, TurnBodyFn turnBody);

// The slapped one faced for the mod's slap, at Present: the player's heading
// to them and theirs to the player, written each frame of the tap's watch -
// after the gaze's own heading write, which took the one written in the
// camera pass straight back ("der text must face usw kommt", the tester,
// 2026-10-07).
void HoldSlapFacing(SlapTapState& state, TurnBodyFn turnBody);

}  // namespace obvr::camera::hands
