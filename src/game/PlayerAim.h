#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

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

// A NOTE ABOUT SPELLS, because this is where the reader for them used to be.
//
// The action enum has no cast in it, so extending the aim to magic first went
// looking for a casting flag of its own - MiddleHighProcess+0x14C, which
// xOBSE's header hedges as "looks like true if casting". It worked, and then
// the trace measured what it was worth: the flag stood for 79 frames, and the
// action field over the same casts read 54 frames of Attack followed by 24 to
// 26 of AttackFollowThrough. 54 plus 25 is 79.
//
// So the flag says nothing the action field does not, and says it with less
// resolution: it cannot tell Attack from FollowThrough, and FollowThrough is
// exactly the line that matters, because it means the thing has GONE. A cast is
// simply reported as an attack, and IsShotUnreleased above already draws that
// line correctly for it.
//
// The reader was deleted rather than left standing unused. What it found is
// here because the finding is worth keeping and the code was not.

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

// Whether the shot is still in hand - the arrow on the string, the swing not
// yet landed.
//
// This is the difference between "the control was let go" and "the arrow has
// gone". Releasing starts the release animation; the arrow leaves several
// frames later, along whatever the heading is at that moment. So the heading
// has to be held from the release until this turns false, and not one frame
// longer.
//
// MEASURED, not reasoned. The shot trace across real bow shots gives the
// sequence plainly:
//
//   frame 0-6   action=5  kAction_AttackBowArrowAttached - arrow on the string
//   frame 7     action=3  kAction_AttackFollowThrough    - the arrow has gone
//   frame 7-39+ action=3  still following through
//
// FollowThrough is therefore excluded, and that is the whole point of the
// measurement. Counting it as "attacking" held the body turned for the entire
// follow-through, which the trace shows running past forty frames - over half a
// second of walking the way the shot went, which is exactly what was reported:
// "fuer den zeitraum des schiessen laeuft er in die richtung wo ich ziele".
// Ending at the transition instead makes the window seven frames.
//
// The same reading also confirms the offset and the action values themselves,
// which had only one source: 5 and 3 appear exactly where a bow shot should put
// them, and -1 and 0 appear when nothing is being done.
//
// False when it cannot be read. The caller has a time limit behind this, so a
// wrong false costs an early straightening rather than a body that never comes
// round.
bool IsShotUnreleased();

// The raw action value, for the shot trace and for nothing else.
//
// IsPlayerAttacking answers the question the code acts on; this answers the one
// a person reading a log needs, which is what the sequence of actions during a
// shot actually looks like. kActionNone when it cannot be read.
//
// It exists because the window the body is turned for is currently the whole
// attack, and that turned out to be visible: the character walks the aimed way
// for its length, and the bow swings and springs back. Shortening it to the
// frame the arrow is actually made needs knowing which action change that is,
// and guessing at it from an enum read once is exactly what the project's own
// notes say not to do.
SInt32 ReadPlayerAction();

// Where the player stands, in world units. The origin at the feet, as every
// TESObjectREFR's position is - see CrosshairTarget, which learned that the
// hard way when a distance came out at twice what the eye said.
//
// Wanted as a rotation CENTRE. The body's turn swings the first person camera
// through an arc, and an arc is only defined once the point it turns about is
// known. The offset is the same 0x2C the crosshair already reads references at,
// so this adds no new address to keep correct.
//
// False when the player cannot be reached, and the caller must then leave the
// camera alone. Said with a return value rather than with a zero vector,
// because a caller that mistook "unknown" for the world origin would turn the
// camera about a point half a map away.
bool PlayerWorldPosition(NiPoint3& out);

// Whether the player stands in a cell - a game is loaded. False at the main
// menu and through the intro, where the player object exists and belongs
// nowhere. Read from the reference's parent cell pointer
// (kRefParentCellOffset); decides only where a menu hangs, so a wrong
// answer is a menu in the wrong place and nothing worse.
bool PlayerInWorld();

}  // namespace obvr::game
