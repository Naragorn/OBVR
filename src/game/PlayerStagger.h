#pragma once

#include "core/Types.h"

namespace obvr::game {

// The player never staggered by a hit, and never knocked back ([Look]
// NoPlayerStagger). In a headset the stagger's lurch - the view jolting a
// step back - is motion nobody asked for (2026-09-26). NPCs keep both.
//
// Read in Oblivion.exe 1.2.0.416:
//
// - The stagger has one start, 0x005F4FD0 (thiscall, the actor in ecx, no
//   stack arguments, return unused): anim group 0x1F, then `push 8; call
//   005EFFD0` - the process's action kAction_Stagger (8, xOBSE
//   GameProcess.h). It is called at two sites, both `mov ecx, <target>;
//   call 005F4FD0`: 0x0060051E in the hit handler 0x005FEBF0 (the target
//   in esi) and 0x005FCCC8 (the target in edi).
//
// - Separately, every qualifying hit pushes the target through its Havok
//   character proxy: in the hit handler `mov ecx, esi; call 0065A2C0` at
//   0x0060008A fetches the target's proxy (thiscall, no arguments, plain
//   ret), and a non-null proxy gets the knockback force (fKnockback*,
//   capped by fKnockbackForceMax, for fKnockbackTime) through 0x008907A0 at
//   0x006000AF. A null proxy is already handled: `test ebx, ebx; je
//   006000B4` skips the push. This moves the actor for real.
//
// Both sites are rerouted to OBVR's own functions, which answer "not the
// player" by calling the original and "the player, with the setting on" by
// doing nothing (the stagger) or answering no proxy (the knockback).
inline constexpr UInt32 kStaggerStart = 0x005F4FD0;
inline constexpr UInt32 kCallStaggerFromHit = 0x0060051E;
inline constexpr UInt32 kCallStaggerFromReach = 0x005FCCC8;
inline constexpr UInt32 kCharacterProxyOf = 0x0065A2C0;
inline constexpr UInt32 kCallKnockbackProxy = 0x0060008A;

// Whether this actor's stagger or knockback is skipped.
inline bool SkipForPlayer(bool enabled, UInt32 actor, UInt32 player) {
	return enabled && player != 0 && actor == player;
}

void InstallPlayerStagger();
void SetNoPlayerStagger(bool enabled);

}  // namespace obvr::game
