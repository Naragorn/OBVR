#pragma once

#include "core/Types.h"

namespace obvr::game {

// A held object brought right up to the player - to the mouth, to the body -
// instead of being held off about a quarter metre away (2026-09-26: "werden
// 25 davor geblockt"; eating by bringing food to the mouth and stowing by
// bringing an item to the body are planned gestures).
//
// The grab update (0x0066D930) keeps the spring's target outside a cylinder
// round the player: it measures the target's offset from the player
// (0x0043F350, the length), takes the character controller's radius
// (0x008913C0) times 6.999 plus 5 units, and when the target lies inside
// that, pushes it out to the edge. In Oblivion.exe 1.2.0.416:
//
//   0066DF3D  D8 D1       fcom   st(1)         ; the radius against the length
//   0066DF3F  DF E0       fnstsw ax
//   0066DF41  F6 C4 41    test   ah, 41h
//   0066DF44  75 67       jne    0066DFAD      ; outside: the target as it is
//   0066DF46  DE E1       fsubrp st(1), st     ; inside: pushed out ...
//
// Turning the jne into a jmp (EB) always takes the "outside" path, with the
// same x87 stack. Done only while the hand mode runs; vanilla's is put back
// otherwise. The capsule's collision no longer stops the object either
// (game::GrabPhysics gives it the player's group).
inline constexpr UInt32 kGrabNearBodyBranch = 0x0066DF44;

// Brings the branch to the wanted form; safe every frame. Answers whether
// the wanted form is in place.
bool AllowGrabNearBody(bool allow);

}  // namespace obvr::game
