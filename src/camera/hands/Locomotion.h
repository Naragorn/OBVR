#pragma once

// Walking in Full VR, the parts that run with the hand mode's controls:
// - running by swung arms ([Hands] ArmSwingRun, vr/ArmSwing.h): its log;
// - the left stick's analog walk and its sprint (vr/AnalogWalkLogic.h,
//   game/AnalogWalk.h): the engine's pace factor written for the push, the
//   heading kept for the player's move, the run key turned over under Always
//   Run so the legs show the gait the push chose, the sprint paid in fatigue.
//
// Moved out of camera/CameraHook.cpp on 2026-11-01 (docs/god-functions-plan.md,
// phase 2 step 2.4): the code as it was, its function statics now
// LocomotionState's members. Harness: analog-walk.txt, arm-swing-run.txt.

#include "camera/hands/HandStep.h"
#include "camera/hands/Swim.h"
#include "vr/AnalogWalkLogic.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {

struct LocomotionState {
	bool swingWas = false;
	UInt32 swingLines = 40;
	vr::SprintState sprint;
	bool spentWas = false;
	UInt32 spentLines = 8;
	SwimArmsState swim;  // swimming with the arms (camera/hands/Swim.h)
	bool blockingWas = false;  // the block the pose holds, last frame (LogBlockChange)
	bool shieldRaisedWas = false;  // the shield hand raised while it guards by contact
	UInt32 shieldRaisedLines = 12;
	bool weaponRaisedWas = false;  // the weapon's guard pose up while it guards by contact
	UInt32 weaponRaisedLines = 12;
	bool torchRaisedWas = false;  // the torch raised while it guards by contact
	UInt32 torchRaisedLines = 12;
};

// "Arm swing: running / walking again" when the swung arms start or stop
// the run.
void LogArmSwing(LocomotionState& state, const HandStep& step, const HandShared& shared);

// A block's start or end, from the hands' pose (vr/HandBlockLogic.h): at the
// start with the walk's state - the stick's push and its run, a run asked,
// the arms' run - and both controllers' speeds, since the engine walks a
// blocking player (free-weapons-spec, "Windows run of 2026-11-27": no run
// with a shield). Once a change, never a frame. And, capped, the shield hand
// raised or lowered while the shield guards by contact (vr/HandBlockFrame.h):
// no Block is held then, the line says so with the same walk's state.
void LogBlockChange(LocomotionState& state, const vr::HandModeResult& hand, const vr::HandModeFrame& frame);

// The analog walk and the sprint this frame, with the hand controls running.
// `flying`: the console's fly camera is on (its sticks are the camera's).
// `deltaSeconds`: the frame's (g_deltaSeconds), as the sprint's cost always
// counted. `rolesSwapped`: the hands' roles swapped (the swimming's buzz goes to
// the physical hand). Turns `controls.run` over for the gait the push chose;
// the arms' swimming (Swim.h) steps here too, as it owns the same request.
void StepAnalogWalk(LocomotionState& state, const HandStep& step, const HandShared& shared, bool flying,
                    float deltaSeconds, bool rolesSwapped, vr::HandControlsWanted& controls);

}  // namespace obvr::camera::hands
