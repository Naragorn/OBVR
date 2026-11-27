#pragma once

// Swimming with the arms, the hands' side of it (vr/SwimStroke.h,
// game/SwimStep.h, docs/swimming-spec.md): reads the hands, the head and the
// game each frame, steps the stroke logic and hands the swimmer's velocity to
// the player's move. Called from the analog walk's step (Locomotion.cpp), which
// owns the controls and the stick's request: while the arms swim and the stick
// is idle the game is given a tiny push in the stroke's direction, so it plays
// its swimming and counts the player as moving; the step itself is the arms'.

#include "camera/hands/HandStep.h"
#include "game/AnalogWalk.h"
#include "vr/SwimStroke.h"

namespace obvr::camera::hands {

struct SwimArmsState {
	vr::SwimState stroke;
	NiPoint3 headLast{0.0f, 0.0f, 0.0f};  // the head in tracking space last frame, metres
	bool headLastValid = false;
	bool wasSwimming = false;
	bool wasHolding = false;
	float sinceHolding = 1.0e9f;  // seconds since a hand last held the water
	float buzzClock = 0.0f;       // seconds until the next buzz
	float swum = 0.0f;            // game units carried by the strokes since the water was entered, for the log
	UInt32 lines = 24;
};

// The push that shows the game a swimmer: a stick's amount past nothing, so its
// own step is a few per cent of the swim speed and is cut away by the arms' step.
inline constexpr float kSwimNudgeAmount = 0.05f;

// This frame's strokes. `request` is the analog walk's, taken over (active, a
// tiny push) when the arms swim and the stick is idle; `controls` gets the
// push's keys and loses the ready click a closed hand in the water would make.
void StepSwimArms(SwimArmsState& state, const HandStep& step, const HandShared& shared, bool flying,
                  bool rolesSwapped, game::AnalogWalkRequest& request, vr::HandControlsWanted& controls);

}  // namespace obvr::camera::hands
