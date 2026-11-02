#pragma once

// What the laser can point at, read into the frame: the game's cursor pixel
// (the engine's own cursor where the laser's pixel was written for the tile
// search, game/MenuCursor.h), whether the tile search found a tile under it
// (the sticky button), the scroll knob and the knob snap under a pull, the
// map as a drag surface, the quad the game's menus hang on (render/HudLayer.h),
// the cinema screen of a flat frame (render/HeadsetRenderer.h), and OBVR's
// own panel (ui/SettingsMenuLayer.h) with its canvas; the pull's tile and
// the flat picture's arrival logged the first several times.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-02
// (docs/god-functions-plan.md, phase 2 step 2.18f): the code as it was, its
// one file global and two statics now LaserTargetsState's members; the
// layers, the renderer and the onboarding's state come in as links.
// Harness: main-menu-laser.txt, slider-knob.txt, laser-drag.txt,
// settings-sections.txt, load-menu-map.txt.

#include "camera/hands/HandStep.h"
#include "render/HeadsetRenderer.h"
#include "render/HudLayer.h"
#include "ui/SettingsMenuLayer.h"

namespace obvr::camera::hands {

struct LaserTargetsState {
	bool flatReported = false;  // the flat picture's arrival said (was g_flatLaserTargetReported)
	// The block's function statics, as they were.
	UInt32 pullLinesLeft = 10;
	bool wasPulled = false;
};

// What the fill reads of the rest of the frame.
struct LaserTargetsLinks {
	render::HudLayer& hudLayer;                    // the quad the game's menus hang on
	render::HeadsetRenderer& headsetRenderer;      // the flat picture of a flat frame
	bool onboardingOpen;                           // the onboarding's panel is the one open (g_onboarding.IsOpen())
	const ui::SettingsMenuLayer& onboardingLayer;
	const ui::SettingsMenuLayer& settingsMenuLayer;
};

void FillLaserTargets(LaserTargetsState& state, const HandStep& step, vr::HandModeFrame& frame,
                      const LaserTargetsLinks& links);

}  // namespace obvr::camera::hands
