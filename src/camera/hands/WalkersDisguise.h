#pragma once

// Oblivion Walkers' disguise in the hands (vr/WalkersDisguise.h; Nadi,
// 2026-11-09): the gesture from a dead body to the chest sends 'WGBL'; while
// Walkers says 'WGDS' 1 the view gets a black-red vignette and darker and the
// hands bloody. Stepped from StepWalkersShake (camera/hands/WalkersShake.cpp).

#include "camera/hands/HandStep.h"
#include "render/VignetteLayer.h"
#include "vr/WalkersDisguise.h"

namespace obvr::camera::hands {

struct WalkersDisguiseState {
	vr::DisguiseGestureState gesture;
	vr::DisguiseLook look;
	render::VignetteLayer ring;    // black-red at the edge
	render::VignetteLayer shade;   // the whole view a little darker
	bool identitySet = false;
	float sinceScan = 0.0f;        // the bodies near the hands looked for at 10 Hz
	UInt32 underHand[2] = {0, 0};  // the last scan's bodies
	float sinceBlood = -1.0f;      // the hands' stains laid again before the engine fades them
	bool disguisedSaid = false;
	UInt32 lines = 12;
};

void StepWalkersDisguise(WalkersDisguiseState& state, const HandStep& step, const HandShared& shared,
                         const vr::HandModeFrame& frame, bool disguised);

// Out of the world: both rings hidden, the gesture forgotten.
void HideWalkersDisguise(WalkersDisguiseState& state, vr::OpenVRBackend& backend);

}  // namespace obvr::camera::hands
