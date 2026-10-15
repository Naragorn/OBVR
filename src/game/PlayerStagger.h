#pragma once

#include "core/Types.h"
#include "game/HitKnockbackLogic.h"

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
// The push itself: at 0x006000AF `call 008907A0` - thiscall(proxy, const
// NiPoint3* distance in game units, float seconds), ret 8 (Shove.cpp uses
// it for the shove). Read 2026-10-15 for the player's hits on others
// ([Hands] EnemyKnockback; game/HitKnockbackLogic.h): the hit handler keeps
// its `this`, the attacker, in edi from 0x005FEC3B (`mov edi, ecx`) to the
// epilogue's `pop edi` at 0x006005CF; its first argument, the power attack
// flag, lies at [esp+0x1E4] of a frame esp never leaves (read as a byte at
// 0x005FF434 and 0x005FF4C5; the locals at [esp+0x20..0xA0] right before
// 0x0060008A, and the push's vector built at [esp+0x98] is handed over as
// [esp+0x9C] after one push). So a stub at the proxy call hands the attacker
// (edi) and the flag ([esp+0x1E8] once the call pushed its return) to
// OnHitKnockbackProxy with the target in ecx, and the call at 0x006000AF is
// rerouted to scale the vector by the share that proxy call decided.
inline constexpr UInt32 kCallKnockbackPush = 0x006000AF;
inline constexpr UInt32 kProxyKnockbackPush = 0x008907A0;

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

// Exhaustion instead of the collapse and the paralysis fall ([Look]
// Exhaustion; game/ExhaustionLogic.h). Read in Oblivion.exe 1.2.0.416,
// 2026-10-13: the knock state's step, 0x006545E0 - thiscall(process,
// actor), ret 4, the process vtable's slot +0x2F4 right after the knockdown's
// (0x00A71B08 high, 0x00A72978 middle-high; no direct calls) - runs every
// frame and, with the state 0 and the actor not dead, begins the fatigue
// collapse when the fatigue (GetActorValue_F 0xA, 0x005E0A60) is below 0 -
// state 4, the controller pushed, no ragdoll - or the paralysis fall when
// actor value 0x30 is set (0x005E17E0): state 3, the ragdoll on. Both lie
// (state 3) until the fatigue is 0 or more and the paralysis gone, then the
// get-up (5, 6). Both slots are rerouted to OBVR's own: for the player, with
// the setting on, the step is SKIPPED on the frames it would begin either
// (CollapseSkipped: state 0 and the condition met) and run otherwise - a
// knockdown under way, the lying and the get-up keep their course; everyone
// else goes to the original. The frames skipped are counted for the camera
// pass (TakeCollapseSkips: the gasp, the log).
inline constexpr UInt32 kKnockStateStep = 0x006545E0;
inline constexpr UInt32 kKnockStepSlotHighProcess = 0x00A71B08;
inline constexpr UInt32 kKnockStepSlotMiddleHighProcess = 0x00A72978;

void InstallPlayerStagger();
void SetNoPlayerStagger(bool enabled);
void SetNoPlayerKnockdown(bool enabled);
void SetExhaustion(bool enabled);
// The player's hits on others: how much of the game's knockback they keep
// ([Hands] EnemyKnockback, EnemyKnockbackPowerOnly).
void SetEnemyKnockback(const HitKnockbackSettings& settings);
// A strike by motion is handing its hit to the engine (MeleeHits.cpp): the
// knockback takes the power flag from here while it is, and checks the
// handler's own reading against it once.
void SetMotionHit(bool active, bool powerAttack);
// How many frames the player's collapse or fall was skipped since the last
// call.
UInt32 TakeCollapseSkips();

}  // namespace obvr::game
