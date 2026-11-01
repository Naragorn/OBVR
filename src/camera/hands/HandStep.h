#pragma once

// What every hand feature's step reads (docs/god-functions-plan.md, phase 1).
//
// UpdateHandMode (camera/CameraHook.cpp) fills one HandStep at its top and
// hands it down by const reference, instead of each feature re-deriving it.
// Only what is constant for the whole frame belongs here - the frame's clock,
// whether a menu is up, whether the mode runs, the hand script's mark (set by
// test::StepHandScriptFrame before anything else and not changed after).
// The engine's state that a step can change within the frame (the player's
// action, the view, the weapon) is NOT read once here: a read after a step
// that equips or toggles must see the change, as it always did.
//
// HandShared is the other kind: OBVR's own objects several features read and
// one of them changes during the frame (the hand mode's result, the camera's
// world transform). They go by reference, so a step moved out of
// UpdateHandMode sees the same value at the same moment it saw before.

#include "core/Config.h"
#include "game/GameTypes.h"
#include "vr/HandMode.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {
class VignetteLayer;
}

namespace obvr::camera::hands {

struct HandStep {
	const Config& config;
	vr::OpenVRBackend& backend;
	float dt;               // the hand clock's seconds since the last frame, at most 0.25
	bool menuIsUp;          // a menu is up this frame
	bool headset;           // the headset is connected
	bool active;            // Full VR runs: the setting and a headset
	bool menusOnly;         // the controllers on the menus alone (the mode off)
	bool scriptMarked;      // the hand script wrote a mark this frame
};

struct HandShared {
	vr::HandModeResult& hand;               // the hand mode's result (g_hand)
	const NiTransform& cyclopeanCamera;     // the camera's world transform between the eyes
	const bool& cyclopeanCameraValid;
	render::VignetteLayer& vignette;        // the comfort vignette a move triggers
};

}  // namespace obvr::camera::hands
