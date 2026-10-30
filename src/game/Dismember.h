#pragma once

#include "core/Types.h"
#include "game/DismemberLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The engine side of the cut (game/DismemberLogic.h for the rule; read
// 2026-10-30 in OBNG's decompile, docs/dismemberment-spec.md section 6):
//
// - the joint: `Bip01 Head`'s collision object (0x0047FAC0) -> +0x10 its
//   bhkRigidBody; its constraints in the list at +0x10 (inline first node:
//   the item, then the next node). The neck's is the bhkConstraint (vtable
//   +0x60 = 0x008A0760, RemoveFromWorld) whose hkConstraintInstance (+0x08)
//   links Head and Spine2 (entities +0x10, +0x14, each's +0x0C its bhk
//   wrapper). It is taken out of the world (vt+0x60) and out of the head's
//   list (0x0067A850, the list's remove of a node) - else the engine's
//   joint walker 0x008A53E0 would put it back - inside the Havok lock. One
//   reference is kept on it, so a removal the world still has queued never
//   meets a freed constraint.
// - the skins: each actor has its own NiSkinInstance (cloned per actor,
//   0x0072BFA0); in the shapes outside the head node the head bone (and
//   what hangs under it) is pointed at an OBVR node at the neck with almost
//   no scale, in the shapes under the head node every other bone at one
//   under the head - the arm stump's technique (game/ArmStump.h).

struct NeckOf {
	UInt32 actor = 0;
	bool dead = false;
	NiPoint3 low{0.0f, 0.0f, 0.0f};   // Spine2's origin
	NiPoint3 high{0.0f, 0.0f, 0.0f};  // the head's
};

// The loaded actors (not the player) within `within` units of `around`,
// with their necks; up to `most`.
UInt32 CollectNecks(const NiPoint3& around, float within, NeckOf* out, UInt32 most);

// Whether `actor` is among the loaded actors and dead; false when it is
// not loaded any more (the killing blow's wait never cuts a vanished one).
bool LoadedActorIsDead(UInt32 actor);

// Cuts `actor`'s head off and sends it off at `pushUnits` (game units a
// second: the blade's way); false (and `why` said) when refused.
bool CutHead(UInt32 actor, const NiPoint3& pushUnits, char* why, UInt32 whySize);

bool HeadIsCut(UInt32 actor);

// Once a frame: OBVR's nodes of every cut follow the neck and the head; a
// cut whose actor has a new 3D (unloaded, loaded again) is forgotten.
void StepCuts();

// For the harness's marks: each cut's head and the point it was cut at, and
// how far apart ("Dismember: at the mark").
void LogCuts(const char* mark);

// The equipped weapon's model path ([weapon+0x34], TESModel's path), or
// null.
const char* WeaponModelPath(const UInt8* weapon);

}  // namespace obvr::game
