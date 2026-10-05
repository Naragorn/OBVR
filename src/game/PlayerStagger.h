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

// The player never knocked down and thrown through the air ([Look]
// NoPlayerKnockdown; the tester, 2026-10-05, of a beta tester's report: "andere
// npcs können die player mit iwelchen attacken in die luft befördern und so
// weghauen ... das sollten wir anbieten als option wegzupatchen").
//
// Read in Oblivion.exe 1.2.0.416: every knockdown goes through one function,
// 0x00654420 - thiscall(process, actor, x, y, z, force), ret 14h - which
// puts the actor into the knock state "Explode Lead In" (process+0x11C = 2),
// turns its ragdoll on (0x0088D070) and pushes it with an impulse from the
// source point (0x005364B0 at 0x006545BA): the throw. It is the process
// vtable's slot +0x2F0 in both the high (0x00A71814, the slot at 0x00A71B04)
// and the middle-high process (0x00A72684, at 0x00A72974), and is reached
// from three places: a melee or arrow hit that rolled a knockdown (0x00600402,
// fKnockdownChance and the Marksman perk), a magic explosion (0x00699AA2),
// and the script command PushActorAway (0x0050EB8C). NoPlayerStagger covers
// none of them. Both slots are rerouted to OBVR's own: for the player, with
// the setting on, nothing happens (the damage is the hit's own and still
// lands); everyone else goes to the original. The fatigue knock-out (0x006545E0)
// is a collapse, not a throw, and is left alone.
inline constexpr UInt32 kKnockActorAway = 0x00654420;
inline constexpr UInt32 kKnockSlotHighProcess = 0x00A71B04;
inline constexpr UInt32 kKnockSlotMiddleHighProcess = 0x00A72974;

// Whether this actor's stagger or knockback is skipped.
inline bool SkipForPlayer(bool enabled, UInt32 actor, UInt32 player) {
	return enabled && player != 0 && actor == player;
}

void InstallPlayerStagger();
void SetNoPlayerStagger(bool enabled);
void SetNoPlayerKnockdown(bool enabled);

}  // namespace obvr::game
