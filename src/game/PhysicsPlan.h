#pragma once

// The physics steps of a frame (game/PhysicsPlanLogic.h).
//
// [Hands] PhysicsEvenSteps: the engine's call of its step planner - `call
// 0x00889810` at 0x0040D8C2 in OSGlobals::UpdateStep, cdecl(float frame
// seconds, the cap's flag), read in OBNG re/functions/0x00889810.md and
// 0x0040D800.md - goes to OBVR's planner, which splits every frame into equal
// steps no longer than the maximum step [0x00B2E2E8] and writes the engine's
// own outputs: the step count 0x00BA7914, the step length 0x00BA790C, the carry
// 0x00BA7910, and the frame's seconds and simulated time 0x00B2E2E4 and
// 0x00B2E2E0. Off, or with the INI's iUpdateType not 0 (the engine's other
// modes, 0x00BA7918), the engine's planner runs as it always did. The world
// update (bhkWorld::Update 0x0088C440) reads the two outputs and runs that many
// steps of that length; OBVR's hand bodies read them for their drive
// (game/HandBodyLogic.h HandBodyDriveSeconds).
//
// [Debug] PhysicsStepTimes: the engine's one step (hkWorld::stepDeltaTime,
// 0x00898B70, thiscall(world, float seconds), ret 4, its entry `sub esp,8;
// push esi; push edi` read in the EXE) timed on the performance counter;
// at every hand-script mark the steps since the last one, their time and the
// longest are logged. For measuring what a step costs; off in play.

namespace obvr::game {

// Each frame, on the main thread: whether the even steps are wanted. The
// reroute is put in the first time they are and stays; with them unwanted
// the engine's planner is called through it.
void StepEvenPhysicsPlan(bool wanted);

// [Debug] PhysicsStepTimes: the timing detour put in once when on, and at a
// hand-script mark (`mark` not null) the steps since the last mark logged.
void StepPhysicsStepTimes(bool on, const char* mark);

}  // namespace obvr::game
