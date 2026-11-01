#pragma once

// The bow by hand, each step (vr::StepArchery, vr/Archery.h): an arrow taken
// from the quiver, nocked, drawn, loosed or dropped - the engine's attack
// control followed, the string and the shot's power from the draw, the loose
// snapped to its hold at once, the bow's sounds.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.11): the code as it was, its
// four file globals and six statics now ArcheryState's members. What is
// seen of the bow (g_bowVisual) stays CameraHook's: the camera pass draws it.
// Harness: bow-by-hand.txt, bow-hand.txt, bow-draw-arm.txt.

#include "camera/hands/HandStep.h"
#include "game/BowVisual.h"
#include "vr/Archery.h"

namespace obvr::camera::hands {

struct ArcheryState {
	vr::BowStringState bowString;
	// The loose at once: the draw put at its Hold after the control goes up.
	vr::ReleaseSnapState releaseSnap;
	// The shot's power from the draw (vr::StepBowPower).
	vr::BowPowerState bowPower;
	bool releaseSnapped = false;
	// The block's function statics, as they were.
	float sinceLoose = -1.0f;
	UInt32 leftLines = 24;
	SInt32 action = -1;
	UInt32 actionLines = 200;
	bool watchEase = false;
	UInt32 soundLines = 6;
};

// `bowVisual`: what is seen of the bow, which this step sets for the camera
// pass. `deltaSeconds`: the frame's (g_deltaSeconds).
void StepArchery(ArcheryState& state, const HandStep& step, const HandShared& shared,
                 const vr::HandModeFrame& frame, game::BowVisualInput& bowVisual, float deltaSeconds);

}  // namespace obvr::camera::hands
