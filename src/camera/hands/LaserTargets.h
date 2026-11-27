#pragma once

// What the laser can point at, read into the frame: the game's cursor pixel
// (the engine's own cursor where the laser's pixel was written for the tile
// search, game/MenuCursor.h), whether the tile search found a tile under it
// (the sticky button), the scroll knob and the knob snap under a pull, the
// map as a drag surface, the quad the game's menus hang on (render/HudLayer.h),
// the cinema screen of a flat frame (render/HeadsetRenderer.h), and OBVR's
// own panel (ui/SettingsMenuLayer.h) with its canvas, the tablet's menus
// (game/TabletMenu.h); the pull's tile and
// the flat picture's arrival logged the first several times.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-02
// (docs/god-functions-plan.md, phase 2 step 2.18f): the code as it was, its
// one file global and two statics now LaserTargetsState's members; the
// layers, the renderer and the onboarding's state come in as links.
// Harness: main-menu-laser.txt, slider-knob.txt, laser-drag.txt,
// settings-sections.txt, load-menu-map.txt.

#include "camera/hands/HandReachRead.h"
#include "camera/hands/HandStep.h"
#include "game/TabletMenu.h"
#include "render/HeadsetRenderer.h"
#include "render/HudLayer.h"
#include "ui/SettingsMenuLayer.h"
#include "vr/TabletClose.h"

namespace obvr::camera::hands {

struct LaserTargetsState {
	bool flatReported = false;  // the flat picture's arrival said (was g_flatLaserTargetReported)
	// The block's function statics, as they were.
	UInt32 pullLinesLeft = 10;
	bool wasPulled = false;
	// The tablet's menus (game/TabletMenu.h).
	game::TabletMenuState tabletMenu;
	// The drawn index fingers' tips in their controllers' frames (vr/FingerTip.h), and the lines left to say them.
	vr::IndexTip rightIndexTip;
	vr::IndexTip leftIndexTip;
	// The palms' middles, where the tablet is held (vr/FingerTip.h PalmBetween).
	vr::IndexTip rightPalm;
	vr::IndexTip leftPalm;
	UInt32 tipLinesLeft = 6;
	// How each hand holds a sword, from its held-item node (vr::HeldAxes):
	// the tablet is turned like the sword in the hand that holds it.
	vr::HeldAxes rightHeld;
	vr::HeldAxes leftHeld;
	UInt32 heldLinesLeft = 4;
	// How far each hand as drawn reaches out of the page's flat, read while
	// the tablet is up (camera/hands/HandReachRead.h): the page lies before it.
	HandReachState reach;
	bool tabletUp = false;  // the tablet's menus were up at the last fill
	vr::TabletCloseState tabletClose;  // its menus fading out after the close (vr/TabletClose.h): the walk goes on
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

// The drawn index fingers' tips, read right after the fingers are posed (BeforeFirstScenePass, game::StepHandFingers
// has updated their bones) against the hands' frames they were pinned with, while the drawn fingers follow the
// controller's (game::FingerPose::Tracked); the next FillLaserTargets hands them to the hand mode for the tablet's tap.
void StepIndexTips(LaserTargetsState& state, bool rightFollows, bool leftFollows, const NiMatrix33& cameraRot,
                   const NiPoint3& cameraPos, float unitsPerMetre, const vr::HandModeResult& hand);

}  // namespace obvr::camera::hands
