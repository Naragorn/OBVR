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
// The units and the direction are settled, from two sources that were found
// independently of each other:
//
//   * xOBSE's GameObjects.h puts the triple at TESObjectREFR+0x20 and stores
//     it in RADIANS, with rotX as the player's pitch and rotZ as the yaw; its
//     script commands convert with kRadToDegree = 57.29577951f.
//   * the Construction Set wiki's SetAngle page says which way it runs, and
//     says it is worth saying: "Note that the values are counterintuitive:
//     negative angles force the player look up, positive angles, down." Its
//     GetAngle page gives the true in-game range as -89 to 89 degrees.
//
// So POSITIVE rotX LOOKS DOWN. Everywhere else in OBVR a positive pitch looks
// up, which is the one thing about this that has to be got right: the sign
// wrong on a bow aims at the floor when the wearer looks at the sky.
struct PlayerRotation {
	float pitch;  // rotX
	float roll;   // rotY
	float yaw;    // rotZ
};

// False before the player exists - the main menu, and the first frames of a
// load. Nothing is written to out then.
bool ReadPlayerRotation(PlayerRotation& out);

// Points the player up or down, in radians, positive looking DOWN - the
// engine's own convention, kept rather than translated so that this reads the
// same way as the field it writes. camera::PlayerPitchForGaze is where the
// sign is turned around, once.
//
// Only the pitch. The yaw is the player's to steer and stays theirs: turning
// the body from the head was considered and refused - "character bleibt" -
// and roll on an actor does nothing at all.
//
// False when the player could not be reached, on the same terms as the read.
bool WritePlayerPitch(float radians);

}  // namespace obvr::game
