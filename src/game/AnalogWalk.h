#pragma once

#include "core/Types.h"
#include "vr/AnalogWalkLogic.h"

namespace obvr::game {

// The left stick's analog walk in the game (vr/AnalogWalkLogic.h). Read in
// Oblivion.exe 1.2.416 (2026-10-31):
//
// - 0x00B14E58, a float in .data (1.0 in the file): the player's pace
//   factor. The player's handler 0x00671620 writes it only from a gamepad's
//   stick (|stick| x 0.02, at most 1), and hands it to the movement
//   animation's update 0x005FCAB0, which plays the walk or the run at
//   GetSpeed / the animation's own speed times it; the walk's step is that
//   animation's root motion (UpdateMovement 0x0063C730 via 0x004723A0).
// - 0x005E3590 and 0x005E3750, the actor's walk and run speeds: thiscall,
//   no arguments, the float in ST0, plain `ret` - getters, nothing written.
//   Run = walk x (fMoveRunMult 3 + Athletics x 0.01 x fMoveRunAthleticsMult).
// - PlayerCharacter +0x58B: Always Run, a byte the Always Run control turns.
//
// Each frame OBVR writes the factor for the push (1.0 again once it is not
// walking analog, only if OBVR wrote it), and keeps the stick's direction for
// the wrap of the player's Move (game/RoomFollow.cpp), which turns the step.

// Checks the speed getters' first bytes; logs whether the analog walk can run.
void InstallAnalogWalk();

struct AnalogWalkRequest {
	bool active = false;  // walking analog this frame (the gate open, the stick pushed)
	vr::AnalogStick stick;
	bool running = false;
	bool forcedRun = false;
	float runFrom = 0.65f;
	// Sprinting (vr::StepSprint): the run's legs at SprintPace(sprintFactor).
	bool sprinting = false;
	float sprintFactor = 1.4f;
};

// Once a frame: the pace factor written and the direction kept for the move.
// The factor written comes back in `paceOut` (1 when not walking analog).
void StepAnalogWalk(const AnalogWalkRequest& request, float& paceOut);

// Always Run as the player has it (false without a player).
bool PlayerAlwaysRuns();

// From the wrap of the player's Move: the step turned to the stick's
// direction while walking analog (vr::AnalogTurnStep), else left.
void TurnAnalogStep(float* step);

}  // namespace obvr::game
