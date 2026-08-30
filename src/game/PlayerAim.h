#pragma once

#include "core/Types.h"

namespace obvr::game {

// The player's own rotation - which is what an arrow leaves along, and is not
// the camera.
//
// This is the gap behind "the arrow does not go where I am looking". OBVR
// takes the vertical look away from the mouse and hands the camera to the
// head, but it does that by replacing the camera matrix after the engine has
// computed it (see LookControl). The player's rotation is never touched, so
// the mouse still aims it, invisibly, while the view goes somewhere else.
// Projectiles are TESObjectREFRs and leave along a rotation, so they follow
// the mouse and not the head.
//
// Read-only for now, deliberately. Which way rotX grows - and whether it is
// radians at all - decides whether writing it aims up or straight down, and
// guessing that wrong on a bow is worse than not aiming at all. AimProbe logs
// the three angles side by side so one short session settles it.
struct PlayerRotation {
	float pitch;  // rotX
	float roll;   // rotY
	float yaw;    // rotZ
};

// False before the player exists - the main menu, and the first frames of a
// load. Nothing is written to out then.
bool ReadPlayerRotation(PlayerRotation& out);

}  // namespace obvr::game
