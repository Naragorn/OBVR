#pragma once

// The fatigue fighting costs the player, dearer while a two-hander is held
// in one hand (vr/OneHandTwoHander.h; Nadi, 2026-10-26: "nur auf angriff
// block, swung, parade, schubsen, packen").
//
// The engine pays every fatigue cost through one function, 0x005E07D0
// (thiscall: the actor, the change - a cost negative - and ret 4; it never
// takes the fatigue under 0). Its seven calls, read 2026-10-26 (objdump of
// the Steam build; Ghidra's callers of 0x005E07D0 agree on the five
// functions):
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
// The attack's and the block's are pointed at OBVR, which scales the
// player's change; the other five are left as the game has them. OBVR's own
// costs - the swing, the parry, the shove, the grab, the teleport, the
// hold's burn - call 0x005E07D0 themselves (game::ChargeSwingFatigue,
// game::SpendPlayerFatigue) and are scaled by kind (vr::OneHandScales).
// Fatigue taken by others - a fist's blow (fHandFatigueDamageMult,
// 0x00547353), a spell that damages it - is not paid through it.
//
// A call is pointed at OBVR only when its five bytes are that call.

#include "core/Types.h"
#include "vr/OneHandTwoHander.h"

namespace obvr::game {

// Checks the function and the two calls and redirects those that match;
// logs what it did. Without a match that cost is paid as vanilla pays it.
void InstallFatigueCost();

// The scale for the player's costs of fighting from this frame on: 1 leaves
// them as they are.
void SetFatigueCostScale(float scale);
float FatigueCostScale();

// A cost OBVR itself makes the player pay: times the scale when the kind
// is one a two-hander in one hand makes dearer (vr::OneHandScales), counted.
float PlayerFatigueCost(float cost, vr::FatigueCostKind kind);

// What the scale has added since the last call, by kind. Zeroed by the call.
inline constexpr UInt32 kFatigueCostKinds = static_cast<UInt32>(vr::FatigueCostKind::Count);
void TakeFatigueCostExtras(float out[kFatigueCostKinds]);

}  // namespace obvr::game
