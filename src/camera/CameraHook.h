#pragma once

#include "camera/FrameLogic.h"
#include "game/GameTypes.h"
#include "vr/HeadTracker.h"

namespace obvr::camera {

// Attaches to the end of Oblivion's camera calculation.
//
// Per frame:
//
//   Oblivion computes the vanilla camera
//        -> writes localTransform.pos and .rot of the CameraNode
//        -> OBVR lays the head rotation on top
//        -> Oblivion carries on and updates the scene graph
//
// Which implements the VR formula:
//
//   final camera = vanilla camera * relative head rotation
//
// Returns false when the expected bytes are not at the target address. In
// that case nothing is patched - a wrong game version must not end in a
// shredded code segment.
bool Install();

// State of the last hook pass. Declared in FrameLogic.h, together with the
// transitions it can report.
const State& GetState();

// The tracker whose rotation is laid onto the camera.
vr::HeadTracker& GetHeadTracker();

}  // namespace obvr::camera
