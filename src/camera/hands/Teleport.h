#pragma once

// The Full VR teleport (docs/next-up.md 6, vr::Teleport for the logic,
// game::PlayerTeleport for the engine): the right stick pushed forward aims
// an arc from the right hand, the ring shows where it lands, letting go
// moves the player there.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.1): the code as it was, its
// file globals now the members of TeleportState. Harness: teleport.txt,
// teleport-noise.txt, teleport-fatigue.txt, teleport-sound-landing.txt.

#include "camera/hands/HandStep.h"
#include "render/TeleportArcLayer.h"
#include "vr/Teleport.h"

namespace obvr::camera::hands {

struct TeleportState {
	render::TeleportArcLayer arc;  // the arc while it aims
	vr::TeleportMove move;         // the move once it goes
	// How long a teleport is still heard, with TeleportMakesNoise.
	float noiseLeft = 0.0f;
	// Where the last teleport landed, and the second after it: then the log
	// says how far the player has moved since, with the sticks untouched it
	// should be nothing (the noise once walked it a step on, 2026-09-28).
	NiPoint3 landedFeet{0.0f, 0.0f, 0.0f};
	float driftCheckLeft = 0.0f;
	UInt32 driftLines = 4;
	UInt32 linesLeft = 16;
	UInt32 soundLines = 4;
	UInt32 arrivedLines = 4;
};

// Whether a teleport may be aimed now: the setting, the world, first person,
// alive, no move running, not riding, and in combat only when allowed.
// `thirdPerson` is the view read at the call.
bool TeleportAllowedNow(const TeleportState& state, const Config& config, bool menuIsUp, bool inWorld,
                        bool thirdPerson);

// A move is running with its fade to black (the screen fader steps aside).
bool TeleportFading(const TeleportState& state);

// The frame's teleport: a move a menu interrupted ended, the arc aimed and
// shown, a commit started, the running move stepped, the noise and the drift
// check. `active`: the mode runs (false ends a running move).
void StepTeleport(TeleportState& state, const HandStep& step, const HandShared& shared, bool active);

}  // namespace obvr::camera::hands
