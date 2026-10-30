#pragma once

// The messages between OBVR and Oblivion Walkers' grab (vr/WalkersShake.h for
// what they say and why). Walkers' arrive through the listener the skill
// exceptions put on every plugin (game/SkillCredit.cpp), on the sender's
// thread; the frame reads a copy.

#include "core/Types.h"
#include "obse/NativeMenuApi.h"
#include "vr/WalkersShake.h"

namespace obvr::game {

// At xOBSE's PostLoad, once every plugin is loaded: the messaging to send
// through.
void SetWalkersMessaging(obse::MessagingApi* messaging, UInt32 pluginHandle);

// 'WGVR' 1 to Walkers: at PostPostLoad (Walkers listens to OBVR from its
// PostLoad - at its own load OBVR may not be loaded yet, and xOBSE's
// RegisterListener refuses a sender not loaded), and again at the first world
// frames while no Walkers took it. True when it was taken.
bool SayVrToWalkers();

// A message from another plugin that is not the skill exceptions; Walkers'
// 'WGRB' and 'WGBT' are kept, anything else is left alone.
void OnWalkersMessage(const char* sender, UInt32 type, const void* data, UInt32 bytes);

struct WalkersNow {
	vr::WalkersHolds holds;
	UInt32 holdsSerial = 0;  // grows at every 'WGRB'
	vr::WalkersBite bite;
	UInt32 biteSerial = 0;   // grows at every 'WGBT'
	bool linked = false;     // 'WGVR' reached Walkers
};

WalkersNow ReadWalkersNow();

// 'WGDM': whether OBVR cuts heads (game/DismemberLogic.h). True when taken.
bool SayDismemberToWalkers(bool enabled);

// Each world frame: 'WGDM' said until Walkers takes it - three world frames
// at most per setting, as 'WGVR' - and again when [Hands] Dismemberment changes.
void StepDismemberToWalkers(bool enabled);

// 'WGHD': a head off (kHeadCut) or crushed, the reference's form id. True when
// taken.
bool SendWalkersHead(UInt32 refId, UInt32 kind);

// 'WGPS': the player's power strike is about to hit this reference (form id).
bool SendWalkersPowerStrike(UInt32 refId);

// 'WGSK': one stroke of the held arm (1 left, 2 right).
bool SendWalkersStroke(UInt32 arm);

// [Debug] TestWalkersHold: 0, or the arm (1 left, 2 right) a pretend holder
// holds - the harness's way to the prompt, the strokes and the locked stick
// without a walker. Its reference is 0xFFFFFFFF.
void SetWalkersTestHold(UInt32 arm);

}  // namespace obvr::game
