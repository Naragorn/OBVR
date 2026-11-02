#pragma once

// What the controllers report, to the log: the button mask the first few
// times it changes (which bits an Index puts its buttons on under the legacy
// path is what the gamepad layout rests on), the grips and which actions
// SteamVR has bound whenever either changes, the grip's signals for a
// measurement ([Debug] GripTrace, vr/GripTrace.h), how long a hand's actions
// stay dead (vr/ActionsWatch.h), and which controllers are tracked, on
// change. Diagnostics only: nothing of the frame is changed here.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-02
// (docs/god-functions-plan.md, phase 2 step 2.18h): the code as it was, its
// five file globals and five statics now ControllerLogState's members.
// Harness: every scenario reads "Hands: right controller tracked" and the
// grip lines (holster.txt, grip-trace.txt if present).

#include "camera/hands/HandStep.h"
#include "vr/ActionsWatch.h"
#include "vr/GripTrace.h"

namespace obvr::camera::hands {

struct ControllerLogState {
	// The mask lines, the first few times (was g_handMaskLinesLeft and the last masks).
	UInt32 maskLinesLeft = 24;
	UInt64 lastRightMask = 0;
	UInt64 lastLeftMask = 0;
	// Which controllers are tracked, said on change (was g_rightHandTracked, g_leftHandTracked).
	bool rightTracked = false;
	bool leftTracked = false;
	// The blocks' function statics, as they were.
	bool grip[2] = {false, false};
	UInt32 active[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
	vr::GripTraceState gripTrace;
	vr::ActionsInactiveWatch actionsWatch[2];
	UInt32 actionsLinesLeft = 40;
};

void LogControllers(ControllerLogState& state, const vr::HandModeFrame& frame, float deltaSeconds);

}  // namespace obvr::camera::hands
