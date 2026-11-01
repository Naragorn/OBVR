#pragma once

// The thing a hand holds by the engine's grab (game::GrabPhysics):
// - its physics: through the player's body while held, thrown with the
//   hand's speed (v + w x r at the held point) when let go;
// - the stow (vr::StepStow): let go at the chest or the belly it goes into
//   the inventory, taken as activating it would take it; the stow spot's
//   ring while it is held, and its placing (vr/StowPlace.h);
// - eating at the mouth: let go there, food is eaten and a potion drunk as
//   the inventory does it; a poison is refused.
//
// Moved out of camera/CameraHook.cpp on 2026-11-01 (docs/god-functions-plan.md,
// phase 2 step 2.7): the code as it was, its file globals and function
// static now HeldState's members. The holster sign that stood in the middle
// of the block runs just before it now: it reads only the hand mode's
// result, the frame and its own state, none of which this step changes.
// Harness: stow*.txt, eat-at-mouth.txt, throw*.txt, grab*.txt.

#include "camera/hands/HandStep.h"
#include "ui/CanvasOverlay.h"
#include "ui/StowSpotPainter.h"
#include "vr/HandInput.h"
#include "vr/Stow.h"
#include "vr/StowPlace.h"

namespace obvr::camera::hands {

struct HeldState {
	// The hand that held it, kept past the grip opening: the engine lets go
	// a frame or two after the key, and the throw is that hand's speed.
	bool holdLeft = false;
	vr::StowState stow;
	bool stowWasAtBody = false;
	UInt32 stowLinesLeft = 40;
	// The spot at the chest an item is let go in (ui::PaintStowSpot).
	ui::CanvasOverlay stowSpotLayer{"obvr.stowspot", "OBVR Stow Spot", ui::kStowSpotCanvas, ui::kStowSpotCanvas};
	ui::StowSpotView stowSpotView;
	UInt32 stowSpotRevision = 1;
	// Placing the stow spot: stepped every frame of the hand mode, the
	// window's commands taken as they come.
	vr::StowPlaceState stowPlace;
	bool eatWasAtMouth = false;
	UInt32 eatPoisonRef = 0;
	UInt32 eatLinesLeft = 40;
	float eatAfterSeconds = -1.0f;
};

// What the held object's step shares with the rest of the frame.
struct HeldLinks {
	bool grabKey;              // the grab key down this frame (the grab reach's verdict)
	bool haveHoldPoint;        // where the hand holds it, when known
	NiPoint3 holdPoint;
	bool weaponPullTaking;     // a pulled weapon is being taken: the grab physics stands back
	bool handBackTaking;       // a handed-over thing is taken back: likewise
	bool rolesSwapped;         // the hands' roles swapped (left-handed)
	float deltaSeconds;        // the frame's (g_deltaSeconds)
	void (*forgetTaken)(UInt32 ref);
	// The thrown thing's hits (CameraHook StepThrowHits).
	void (*throwHits)(const Config& config, float dt, bool inWorld);
};

void StepHeldObject(HeldState& state, const HandStep& step, const HandShared& shared,
                    const vr::HandModeFrame& frame, const HeldLinks& links);

// A second after a meal: what the player's health and fatigue became.
void StepMealAfter(HeldState& state, float deltaSeconds);

// Placing the stow spot (vr/StowPlace.h): while its window is open the ring
// shows at the spot, lit while a grip drags it. `deltaSeconds`: the frame's
// (g_deltaSeconds).
void UpdateStowPlacing(HeldState& state, Config& config, vr::OpenVRBackend& backend, const vr::HandModeFrame& frame,
                       float deltaSeconds);

}  // namespace obvr::camera::hands
