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

// One self-initiated world render, for the menu-world probe: calls the
// engine's render function - through the trampoline, so no second pass and
// no callbacks run - on the renderer instance remembered from the last real
// render, with the frame clock zeroed, and reports how many draw calls it
// produced. False when there is nothing to call yet (no hook, no render seen
// so far) or a render is already running; drawsOut is 0 then.
//
// The caller owns the moment and the scene bracket: this is meant to run on
// a held menu frame, after the frame's own EndScene, wrapped in a
// BeginScene/EndScene pair of the caller's making. What it answers is
// whether a render Oblivion did not schedule draws at all - the question
// behind keeping the world live behind pause menus.
// vertexSetupOut separates the two ways a render can produce no draws, and
// they point at opposite fixes. A render that returns early does no vertex
// setup either: something upstream refuses, and the work is finding that
// gate. A render that sets up the pipeline and still draws nothing walked its
// scene and found it empty: the work is then filling the list it walks, which
// the engine's update step builds and which does not run while a menu is up.
// Counted from the same always-on totals the dual pass compares its passes
// with - transforms, declarations, FVFs, vertex shaders and constant uploads,
// summed, because here only "did any of it happen" is being asked.
bool RunMenuWorldProbe(UInt32& drawsOut, UInt32& vertexSetupOut);

// Why the probe would refuse right now, as a phrase for the log. "Refused"
// on its own reads as a fact about self-initiated renders when it is usually
// a fact about the run: a probe that fires in the main menu has never seen a
// world render, so there is no renderer instance to call and nothing has been
// learnt. Naming the reason keeps the two apart. Valid only alongside a
// RunMenuWorldProbe that returned false; on the successful path it says so.
const char* MenuWorldProbeRefusal();

}  // namespace obvr::render
