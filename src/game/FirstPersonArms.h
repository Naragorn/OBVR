#pragma once

#include "core/Types.h"
#include "game/GameTypes.h"

namespace obvr::game {

// The first person arms, turned to follow the gaze while the body is not.
//
// WHY THIS EXISTS. Aiming with the head works by turning the player's heading,
// because that is what an arrow leaves along. But walking follows the same
// heading, so a turned body walks sideways - one field, two jobs, and no way
// to separate them in space. The way out was to separate them in TIME: turn
// the heading only for the frames in which the arrow is made, and leave the
// draw alone.
//
// That works, and it leaves the bow pointing forwards while the wearer aims
// somewhere else, which was reported at once: "der bogen zeigt egal wo ich
// hinschaue nach vorne."
//
// The bow is only DRAWN, though. Its direction is a picture, not a fact about
// the world - nothing is fired along it and nobody walks along it. So it can
// be turned freely, and turning it costs nothing and breaks nothing.
//
// THE ANGLE IS THE ONE ALREADY BEING COMPUTED. camera::AimYawRemaining is how
// far the head is turned beyond what the body has already taken. While drawing
// the body has taken nothing, so the arms turn the whole way; during the shot
// the body takes it, and the remainder falls to zero as the arms give it up.
// The sum is constant, so the bow does not jump when one hands over to the
// other - which is the second half of what was reported: "then it jumps back
// to the middle after the shot".

// The node, or null when it cannot be reached or does not prove to be one.
//
// The offset it uses has a single source. So the object is asked to identify
// itself before a byte is written to it: a scene graph node carries a name, and
// a pointer that leads to a readable one is a node, while one that does not is
// refused. What it was instead goes in the log, once.
NiAVObject* FirstPersonArmsNode();

// Turns the arms by radians about the vertical, on top of whatever the engine
// and the animation put there this frame.
//
// SELF-CORRECTING ABOUT THE ENGINE'S HABITS, which is the part worth reading.
// Whether Oblivion rewrites this node's local rotation every frame is not
// known, and the answer decides whether a rotation applied on top of it
// accumulates. So the value written is remembered: if it is still there next
// frame, the engine did NOT rewrite it and the previous frame's base is put
// back before the new turn is applied; if it is gone, the engine did rewrite it
// and the fresh value is the base. Both ways the arms end up turned by exactly
// the angle asked for, and the animation underneath survives.
//
// False when there is no node, which is the caller's cue to leave the setting
// alone rather than to try again differently.
bool TurnFirstPersonArms(float radians);

// The hand-tracked mode's placement: a whole rotation and a translation on
// top of what the engine and the animation put there this frame, with the
// same self-correcting base as the turn. The rotation is applied in the
// node's parent space, the translation added in it, both in game units.
// Which space the parent is - the camera's, most likely, for a node the
// first-person arms hang from - is not measured yet; the log names the
// parent once so the first headset run can settle it.
bool PlaceFirstPersonArms(const NiMatrix33& rotation, const NiPoint3& offset);

// Puts back whatever the engine had, and forgets the base. For switching the
// feature off, and for leaving first person, so the arms are not left holding
// a turn nothing is going to update.
void ReleaseFirstPersonArms();

// Which way the arms actually point in the world, in radians, or 0 when the
// node cannot be reached.
//
// THE ONLY NUMBER THAT IS NOT AN INTENTION. Everything else logged about the
// aim - the head's angle, the body's share, the arms' share - is what OBVR
// means to happen. This is read back out of the world transform after the fact,
// so it is what the wearer is looking at. When a jump is visible and every
// intention checks out, this is the column that has to move.
float FirstPersonArmsWorldYaw();

}  // namespace obvr::game
