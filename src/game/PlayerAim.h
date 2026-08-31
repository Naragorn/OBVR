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

// Turns the player to a heading, in radians, on Oblivion's own reckoning -
// zero at north, growing clockwise.
//
// Written for one frame at a time and put back at the end of it, which is what
// keeps it from feeding into the camera. The camera is built on the player's
// heading and the head's turn is added on top, so a heading left standing
// would be read back next frame with the turn already in it and the view would
// creep round for as long as the head stayed turned. Restoring the engine's
// own value before the next camera pass removes that possibility rather than
// correcting for it - see camera::AimYawWanted and the restore in OnFrameEnd.
//
// False on the same terms as the write above.
bool WritePlayerYaw(float radians);

// Whether the player has a weapon or spell readied - combat stance rather than
// walking around.
//
// Three answers, not two, and the third is the point: unknown is a real
// outcome. Before the player exists, or when the byte holds something that is
// not a boolean, this says so instead of guessing, and the caller then behaves
// as though a weapon were out - because a crosshair that is wrongly present is
// a far smaller fault than one that is wrongly missing while somebody is
// trying to aim.
//
// Read as a byte rather than through the virtual that returns it. See
// addr::kProcessWeaponOutOffset for why, and for the two readings of xOBSE's
// GameProcess.h that put the field there.
//
// A SPELL COUNTS AS A WEAPON HERE, and that is assumed rather than measured.
// Oblivion's own name for the field is "weapon out", and readying a spell puts
// the player in the same combat stance, so the expectation is that it covers
// both - but nothing has confirmed it. If casting turns out not to raise this,
// the queued magic item is the next place to look (GetQueuedMagicItem, vtable
// 0xAA, MiddleHighProcess+0x144).
enum class WeaponState { Unknown, Sheathed, Drawn };

WeaponState ReadPlayerWeaponState();

// Whether the player is sneaking.
//
// Needed for one narrow thing: Oblivion draws no crosshair in third person,
// but it does draw the sneak eye there, so OBVR must not paste its borrowed
// crosshair over one that the game is drawing after all. See
// camera::BorrowedCrosshairWanted.
//
// False when it cannot be read, which is the answer that does the least harm:
// the borrowed crosshair is then used, and the worst case is a crosshair where
// an eye should be.
bool IsPlayerSneaking();

// Whether the player is still in the middle of an attack - the swing, or the
// bow's release with the arrow not yet gone.
//
// This is the difference between "the control was let go" and "the shot has
// happened". Releasing the attack control starts the release animation; the
// arrow spawns several frames later. A heading changed in between is the
// heading the arrow leaves along, so straightening the body on the release
// frame would send the shot forwards instead of where it was aimed.
//
// False when it cannot be read. The caller has a time limit behind this, so a
// false that is wrong costs an early straightening rather than a body that
// never comes round - and being wrong in that direction at least keeps the
// feature working.
bool IsPlayerAttacking();

}  // namespace obvr::game
