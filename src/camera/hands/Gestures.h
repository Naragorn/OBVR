#pragma once

// Two gestures at people:
// - the yield (vr/Yield.h): both hands up and open, palms to the foe, rocked
//   - then vanilla's yield carried out, block and activate together with
//   the pick on them (YieldDrive below);
// - the middle finger at an NPC (game/Insult.h): their disposition drops.
//
// Moved out of camera/CameraHook.cpp on 2026-11-01 (docs/god-functions-plan.md,
// phase 2 step 2.3): the code as it was, its file globals now GesturesState's
// members. The hand script's head turn to a fighter (FaceFighterForHandScript)
// stays in CameraHook and runs just before UpdateYield, as it did as the
// first statement inside it. Harness: yield*.txt, insult*.txt.

#include "camera/hands/HandStep.h"
#include "game/Insult.h"
#include "vr/Yield.h"

namespace obvr::camera::hands {

// The yield carried out once the gesture is complete: the engine's own yield
// is block and activate together, and its block needs a weapon or the fists
// in hand - with nothing readied, activate on the attacker opened their
// pockets instead (the harness, 2026-10-12: "Container (0x3F0)" on the bandit
// right after "activate goes down ... the player's action -1"). So with the
// weapons away the fists are raised first (the ready-weapon click, followed
// until the game shows them up), then block is held 30 frames with activate
// down from the 10th to the 16th of them, once the block has stood (8 frames
// with activate down right away did not yield in the headset, 2026-10-07),
// and the fists raised for it are lowered again after.
enum class YieldPhase : UInt8 { None, Readying, Blocking };
struct YieldDrive {
	YieldPhase phase = YieldPhase::None;
	UInt32 framesLeft = 0;       // Blocking: frames of block left
	float readySeconds = 0.0f;   // Readying: waited so far
	bool raisedFists = false;    // the fists were raised for this yield
	bool clickWanted = false;    // the ready-weapon click for the next hand mode step
};

struct GesturesState {
	vr::YieldState yield;
	vr::YieldState yieldShadow;  // the rocking alone, for the log's "why not"
	YieldDrive yieldDrive;
	NiPoint3 yieldTarget{0.0f, 0.0f, 0.0f};
	// The attacker yielded to, whose bound the pick's target follows while the
	// block is held - one walking on in the 20 frames before activate put the
	// crosshair on nobody (the harness, 2026-10-12, "the crosshair on 00000000").
	UInt32 yieldActor = 0;
	UInt32 yieldLines = 40;          // the yields and their block and activate
	UInt32 yieldRockLines = 40;      // "rocked, but no yield" with the player fighting
	UInt32 yieldRockQuietLines = 4;  // ... and with nobody fighting the player

	// The middle finger: [0] the right hand, [1] the left.
	game::InsultState insult[2];
	UInt32 insultLines = 20;
};

// The yield's gesture this frame; a complete one starts the drive.
// `active`: the mode runs, in the world, no menu, no settings panel.
void UpdateYield(GesturesState& state, const HandStep& step, const HandShared& shared,
                 const vr::HandModeFrame& frame, bool active);

// The yield carried out: the fists raised and followed, then block held and
// activate pressed through the hand mode's controls. `deltaSeconds` is the
// frame's (g_deltaSeconds), as the drive always counted.
void StepYieldDrive(GesturesState& state, const HandShared& shared, float deltaSeconds);

// The block of a yield is being held: the camera pass puts the pick on the
// one yielded to (YieldTarget).
bool YieldPicking(const GesturesState& state);

// The middle finger this frame. `active` as for UpdateYield.
void UpdateInsult(GesturesState& state, const HandStep& step, const HandShared& shared,
                  const vr::HandModeFrame& frame, bool active);

}  // namespace obvr::camera::hands
