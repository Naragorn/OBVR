#pragma once

// The shield bash's contact and approach in the game (game/BashContactLogic.h,
// game/BashApproachLogic.h): called by CameraHook's StepShieldBash once a frame
// it runs. It makes the shield's disc in the world (from its shape, else the
// hand's point), finds the living one near it, tests the disc against their
// column - or takes the contact step's word that the shield is on their body -
// and keeps the shield's last frames (the controller's velocity, the disc, the
// time) for the bash's speed: the fastest into their body over the last
// kBashApproachSeconds. One clock read and one ring write a frame; the
// candidate's look-up, the disc test and the walk only with someone near.

#include "game/BashContactLogic.h"
#include "game/HandContactLogic.h"
#include "game/NiMath.h"

namespace obvr::camera::hands {

struct BashTouch {
	game::BashDisc disc;        // the shield in the world
	void* actor = nullptr;      // the one it meets; null with none
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	game::BashColumn column;
	float approach = 0.0f;      // m/s into them, the gate's measure; 0 with none
	float gap = 0.0f;           // the disc's nearest point from their column, units (0 touching)
	bool byContact = false;     // met by the contact step's word, the disc short of the column
};

// Adds this frame. `hand`/`rot`: the off hand where it is drawn; `shape`: the
// shield's disc in the hand's frame, null when not read; `restingOn`: the one
// the contact step holds the off hand's shield on (0: none).
BashTouch ShieldBashTouch(const NiPoint3& hand, const NiMatrix33& rot, const game::ShieldDisc* shape,
                          const NiPoint3& velocity, UInt32 restingOn);

// The approach of the frames kept so far into `column`, for the attempt's line
// (camera/hands/ShieldBashTrace.cpp): someone near, not met.
float ShieldBashApproachInto(const game::BashColumn& column);

// Where the shove's push of a bash comes from: 30 units back along the travel
// across the ground, the shield's middle with no travel.
NiPoint3 ShieldBashPushFrom(const NiPoint3& at, const NiPoint3& velocity);

}  // namespace obvr::camera::hands
