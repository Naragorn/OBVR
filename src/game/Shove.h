#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

// The shove's engine side (game/ShoveLogic.h for the decisions). Read in
// Oblivion.exe 1.2.0.416, 2026-09-29:
// - light: the stagger, 0x005F4FD0 thiscall(actor) - the one a heavy hit
//   starts; it guards itself (no animation data or process: nothing) - and
//   the hit's knockback through the character proxy, 0x008907A0, which
//   moves the actor: the stagger alone was not seen in the headset
//   (2026-09-29, logged "staggered" three times, the tester saw nothing);
// - hard: the knockback PushActorAway's handler (0x0050EAB0) ends in -
//   the actor's process ([actor+0x58]) vtable +0x2F0, 0x00654420
//   thiscall(process, actor, float fromX, fromY, fromZ, float force), ret
//   14h. A Character is knocked down (process +0x11C = 2) and its ragdoll
//   pushed away from the point; nothing when it is already down (+0x11C
//   not 0) or dead. Only for a high process (vtable +8 answers 0), as the
//   handler checks;
// - the disposition: ModDisposition's handler (0x00503A50) calls the
//   actor's vtable +0x374, 0x005E2070 thiscall(actor, Actor* toward, float
//   delta), ret 8;
// - the fatigue: SpendPlayerFatigue (game/PlayerTeleport.h).

// Checks the three functions' first bytes once; false (and every shove
// refused) when one differs.
bool VerifyShoveAddresses();

// Shoves `actor` (from LivingActorAt): the stagger or the knockback away
// from `fromWorld`, the fatigue, the disposition. False when refused (not
// verified, no process, already down).
bool ShoveActor(void* actor, ShoveKind kind, const NiPoint3& fromWorld, const NiPoint3& centre,
                const ShoveSettings& settings, bool byHand = true);

}  // namespace obvr::game
