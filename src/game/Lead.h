#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// Taking someone by the hand, the engine's side (game/LeadLogic.h for the
// decisions). Read in Oblivion.exe 1.2.0.416, 2026-09-29:
//
// - Their hands: the actor's 3D root (vtable +0x154, GetNiNode), then
//   NiAVObject::GetObject (vtable +0x58) for "Bip01 R Hand" and "Bip01 L
//   Hand", the world position at node +0x88.
// - A follower: the current package (process vtable +0x184, 0x0064B040:
//   [+0xC0] when set, else [+8]) of type Follow (TESPackage +0x20 == 1) whose
//   target (+0x28: a type byte, 0 a reference, and the reference at +4) is
//   the player. Read before anything of OBVR's is added.
// - Leading someone else: Oblivion.esm's own "FollowPlayer" package
//   (0x0009828A: Follow, target the player, no conditions), given the way
//   the console's AddScriptPackage does it (its handler 0x005123A0, the
//   high-process part replayed): refused while an interrupt package runs
//   (0x005E6B40); the end event of the current package and the start event
//   of the new one (0x004FBF90 cdecl(package, actor+0x44, 0x800 / 0x200));
//   0x005672A0 for a package without its data yet; the package's start
//   marked where the actor stands (0x004D7A20); flag 0x4000 on the package
//   (0x005660C0 thiscall(package, 1)); and 0x005F1590 thiscall(actor,
//   package, 0, 0) sets it. They walk after the player with their own
//   animation and paths.
// - Letting go: RemoveScriptPackage's handler (0x0050B6A0) - the end event
//   (0x400), the process's package cleared (+8), process vtable +0x18
//   thiscall(process, actor, 1) - and EvaluatePackage (0x00601B80) so their
//   own AI takes over again. Only while the package is still the one given.

// The living person whose hand is nearest `point` within `searchUnits`:
// their hand's world position and which one it is. Null when none.
void* ActorHandNear(const NiPoint3& point, float searchUnits, NiPoint3* handOut, bool* rightOut);

// Their hand's world position now; false when it cannot be read.
bool ActorHandPosition(void* actor, bool right, NiPoint3* out);

// Their position (TESObjectREFR +0x2C).
bool ActorPosition(void* actor, NiPoint3* out);

bool ActorIsPlayersFollower(void* actor);
bool ActorFineToLead(void* actor);  // alive, a high process, not fighting
bool ActorInCombat(void* actor);

// Gives them the FollowPlayer package; false (and a log line) when refused.
bool StartFollowing(void* actor, UInt32 followUnits = 0);
// Takes it away again if it is still theirs, and lets their AI choose.
void StopFollowing(void* actor);

void ChangeDisposition(void* actor, float delta);

}  // namespace obvr::game
