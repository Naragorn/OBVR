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
#include "vr/AnalogWalkLogic.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {

struct LocomotionState {
	bool swingWas = false;
	UInt32 swingLines = 40;
	vr::SprintState sprint;
	bool spentWas = false;
	UInt32 spentLines = 8;
};

// "Arm swing: running / walking again" when the swung arms start or stop
// the run.
void LogArmSwing(LocomotionState& state, const HandStep& step, const HandShared& shared);

// The analog walk and the sprint this frame, with the hand controls running.
// `flying`: the console's fly camera is on (its sticks are the camera's).
// `deltaSeconds`: the frame's (g_deltaSeconds), as the sprint's cost always
// counted. Turns `controls.run` over for the gait the push chose.
void StepAnalogWalk(LocomotionState& state, const HandStep& step, const HandShared& shared, bool flying,
                    float deltaSeconds, vr::HandControlsWanted& controls);

}  // namespace obvr::camera::hands
