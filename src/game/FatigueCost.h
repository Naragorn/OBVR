#pragma once

// Every fatigue cost the player pays, dearer while a two-hander is held in
// one hand (vr/OneHandTwoHander.h, Nadi 2026-10-26: "40% (default) mehr
// Ausdauer kosten auf alles").
//
// The engine pays each cost through one function, 0x005E07D0 (thiscall:
// the actor, the change - a cost negative - and ret 4; it never takes the
// fatigue under 0). Its seven calls, read 2026-10-26 (objdump of the Steam
// build; Ghidra's callers of 0x005E07D0 agree on the five functions):
//
//   0x005E4075  an attack       0x005E4010, the cost from 0x00547560
//                               (fFatigueAttackWeaponBase/Mult, the power
//                               attack's penalty)
//   0x005F5C5D  a block         0x005F5B70, from 0x00547590
//                               (fFatigueBlockSkillBase/Mult)
//   0x005FACBF  running         0x005FAAE0, from 0x00547F40 (fFatigueRunBase)
//                               times the Athletics perk's share
//   0x005FAD16  a bow held drawn  the same function, fMarksmanFatigueBurnPerSecond
//   0x005FCA6F  a spell         0x005FCAB0, from 0x00548DA0 (fFatigueCastBase)
//   0x005FD4D1  a bow's shot    the same function, fMarksmanFatigueBurnPerShot
//   0x00672B7C  a jump          0x00671620, from 0x00547F60 (fFatigueJumpBase)
//                               times the Acrobatics perk's
//
// OBVR's own costs - a swing, a parry, a shove, a grab, a teleport - call it
// too (game::ChargeSwingFatigue, game::SpendPlayerFatigue). Fatigue taken by
// others - a fist's blow (fHandFatigueDamageMult, 0x00547353), a spell that
// damages it - is not paid through it, and is not made dearer.
//
// Each call is pointed at OBVR when its five bytes are that call; the change
// is scaled for the player alone, and only while the scale is over 1.

#include "core/Types.h"

namespace obvr::game {

// Checks the function and each call and redirects those that match; logs
// what it did. Without a match a cost is paid as vanilla pays it.
void InstallFatigueCost();

// The scale for the player's costs from this frame on: 1 leaves them as
// they are.
void SetFatigueCostScale(float scale);
float FatigueCostScale();

// The cost OBVR itself makes the player pay: times the scale (the swing, the
// parry, the shove, the grab, the teleport), counted with the others.
float PlayerFatigueCost(float cost);

// What the scale has added since the last call, by what was paid for: the
// engine's seven calls, then OBVR's own. Zeroed by the call.
inline constexpr UInt32 kFatigueCostKinds = 8;
const char* FatigueCostKindName(UInt32 kind);
void TakeFatigueCostExtras(float out[kFatigueCostKinds]);

}  // namespace obvr::game
