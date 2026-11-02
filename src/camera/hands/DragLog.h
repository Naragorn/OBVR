#pragma once

// A drag on a menu, the first several times, to the log: where the engine's
// own cursor - the one the drag follows - was as the drag began and as it
// ended, and on the map its pan as well (game::MapMenuPan).
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-02
// (docs/god-functions-plan.md, phase 2 step 2.18g): the code as it was, its
// seven statics now DragLogState's members. Diagnostics only. Harness:
// laser-drag.txt (the map's drag), slider-knob.txt.

#include "camera/hands/HandStep.h"

namespace obvr::camera::hands {

struct DragLogState {
	bool dragWas = false;
	UInt32 dragLines = 12;
	float dragFromX = 0.0f;
	float dragFromY = 0.0f;
	// On the map, its pan as well: what the drag did.
	bool dragOnMap = false;
	float panFromX = 0.0f;
	float panFromY = 0.0f;
};

void LogMenuDrags(DragLogState& state, const HandShared& shared, const vr::HandModeFrame& frame);

}  // namespace obvr::camera::hands
