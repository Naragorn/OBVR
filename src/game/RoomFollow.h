#pragma once

#include "core/Types.h"

namespace obvr::game {

// The room's step on the player's own move (vr/RoomFollowLogic.h). Read in
// Oblivion.exe 1.2.416 (2026-10-31):
//
// - the walk: 0x0063C730 (the process's UpdateMovement, its vtable 0x00A71814
//   slot 0x2CC) turns the animation's root motion into an actor-local step
//   and calls the actor's Move, vtable slot 0x1B4 - for the player the entry
//   0x00A73BC0, holding 0x005EFAE0 (Actor::Move: `sub esp,28h ... ret 0Ch`,
//   thiscall with dt, the local step NiPoint3*, the movement flags), which
//   ends in MobileObject::Move 0x0065AF30 and the character controller's
//   step 0x00896000: the step over dt is its wanted velocity (x 0.14287673,
//   Havok's units), swept with collision.
// - so a step added to that local step before the call is walked like the
//   walk itself: walls stop it, slopes and stairs carry it.
//
// OBVR wraps the vtable entry; only the player's call from 0x0063C730 takes
// the step, once per request.

// Wraps the player's Move; refused (and logged) when the entry is not the
// game's own.
void InstallRoomFollow();

// The step for the player's next own move, in world units (x, y); 0, 0 asks
// nothing. Replaces a request not yet taken.
void RequestRoomStep(float worldX, float worldY);

// The share (0..1) of the last request the move achieved, once per taken
// request: false when no request was taken since the last call.
bool TakeRoomStepShare(float& share);

}  // namespace obvr::game
