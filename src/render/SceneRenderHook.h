#pragma once

#include "core/Types.h"

namespace obvr::render {

// Makes Oblivion draw the world twice per frame, once per eye, by detouring
// the engine's own render function - addr::kRenderScene, the function that
// draws culling, both scene graph passes, water and the image space shaders,
// and returns with the finished picture in the back buffer and the 2D layer
// not yet on it.
//
// This is the pattern the two nearest relatives use. bo1-vr describes itself
// as "two scene renders per frame"; FEAR2VR hooks "the one call that decides
// what the next DrawScene sees". The function that sets up a view is the
// function you call twice - and here the view setup and the drawing share one
// function, so calling it twice with the camera moved in between is the whole
// mechanism.
//
// What makes this safe to do at all: rendering does not advance the
// simulation. The game advanced physics, animation and particles in its
// update step, before this function runs; drawing reads that state. The
// second call re-renders water reflections and re-runs the image space
// shaders, which costs GPU time and nothing else. Whether any per-frame
// render state objects to being read twice is what the in-game run answers.
//
// The decisions - whether this frame gets a second pass, and what happens
// between and after the passes - belong to the caller. This module owns only
// the detour and the sequence, so the callbacks arrive as three functions.

struct ScenePassCallbacks {
	// Asked once per world render, before anything is drawn. True runs the
	// render twice; false passes the call through untouched.
	bool (*wantsSecondPass)() = nullptr;

	// After the first pass: the finished first-eye picture is in the back
	// buffer, and the camera can be moved for the second.
	void (*betweenPasses)() = nullptr;

	// After the second pass: the second eye's picture is in the back buffer,
	// and the camera should go back where the game left it.
	void (*afterSecondPass)() = nullptr;

	// Which rung of the dual pass this frame is running - see
	// camera::SweepProbeStage. Optional, and used for nothing but the trace:
	// the rung has to appear beside what the 2D pass drew in that frame, or
	// the sweep says nothing.
	UInt32 (*probeStage)() = nullptr;
};

// Verifies the entry bytes, builds the way back in, and patches the entry.
// False when the bytes differ - a different game version, or another mod got
// there first - or when memory cannot be written. Install once.
bool InstallSceneRenderHook(const ScenePassCallbacks& callbacks);

bool IsSceneRenderHooked();

// World renders so far. The probe sweep needs a clock that ticks once per
// frame whatever the frame does, and this is the only one there is: the
// interface pass is skipped on some frames, and Present is hooked later
// than this.
UInt32 CurrentSceneCall();

}  // namespace obvr::render
