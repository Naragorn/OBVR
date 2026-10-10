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
// verified, no process, already down). `punch`: a fist (the off hand's
// strike, GrappleLogic.h) - never a slap in the face.
bool ShoveActor(void* actor, ShoveKind kind, const NiPoint3& fromWorld, const NiPoint3& centre,
                const ShoveSettings& settings, bool byHand = true, bool punch = false);

// The stagger alone (the one a heavy hit starts; it guards itself). False
// when the shove's functions are not the ones read.
bool StaggerActor(void* actor);

// Moves `actor` by `distance` (game units) over `seconds` through its
// character proxy - the hit's knockback, as the light shove pushes - with
// its collision. False when not verified or no proxy.
bool PushActorBy(void* actor, const NiPoint3& distance, float seconds);

// A living Character or Creature in a high process, not knocked down:
// someone a hand can hold.
bool ActorStanding(void* actor);

// Put it in its Place - Enhanced Grabbing's load index when it is loaded
// (its slap noise and its slapped idle token found in the game's form table
// under that index), 0 when it is not. Read once; a slap in the face then
// takes turns with its noise and runs its slapped idle (ShoveLogic.h,
// SlapLines).
UInt8 PutItInItsPlaceIndex();

// A slap the mod is to finish (ShoveLogic.h, SlapByModsGrabTap): the slapped
// one and where they stand, once; the caller taps the grab with the pick on
// them. False with none pending.
bool TakeSlapGrabTap(UInt32& actor, NiPoint3& centre);

// Once a frame through a slap tap's watch (ShoveLogic.h, StepSlapNoiseWait):
// the mod's sequence started, or - `watchOver` with none - the mod's noise
// played by OBVR for the slap dealt.
void StepSlapNoise(bool watchOver);

}  // namespace obvr::game

namespace obvr::game {

// Put it in its Place's own state, read from its quest scripts' variables
// (the quests' low form ids and the variable ids from its ESP, v0.5): for
// the log of a slap tap - whether its grab quest runs, what it counted.
inline constexpr UInt32 kPiiiPGrabQuestLow = 0x0015AC;   // QUST zzPiiiPGrabQ
inline constexpr UInt32 kPiiiPVarsQuestLow = 0x00A548;   // QUST zzPiiiPVarsQ
inline constexpr UInt32 kPiiiPVarNpcGrab = 117;          // zzPiiiPGrabQS.sNPCGrab
inline constexpr UInt32 kPiiiPVarIsGrabbing = 67;        // zzPiiiPGrabQS.sIsGrabbing
inline constexpr UInt32 kPiiiPVarGrabbedItem = 60;       // zzPiiiPGrabQS.rGrabbedItem
inline constexpr UInt32 kPiiiPVarEnabled = 4;            // zzPiiiPVarsQS.sEnabled
inline constexpr UInt32 kPiiiPVarSlapper = 70;           // zzPiiiPVarsQS.sSlapper
// Whether the mod's quest is running (its flags' active bit).
bool ModQuestActive(UInt8 modIndex, UInt32 questLow);
// One of its quest script's variables; false when the quest, its event
// list or the variable is not there.
bool ReadModQuestVar(UInt8 modIndex, UInt32 questLow, UInt32 varId, double& out);

}  // namespace obvr::game
