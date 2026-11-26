#pragma once

// The engine side of a cut's joint (moved out of game/Dismember.cpp,
// 2026-11-26; the reading of 2026-10-30 in docs/dismemberment-spec.md section
// 6): a bone's collision object (0x0047FAC0) -> +0x10 its bhkRigidBody; its
// constraints in the list at +0x10 (inline first node: the item, then the
// next node). A joint is a bhkConstraint (vtable +0x60 = 0x008A0760,
// RemoveFromWorld) whose hkConstraintInstance (+0x08) links two entities
// (+0x10, +0x14, each's +0x0C its bhk wrapper). It is taken out of the world
// (vt+0x60) and out of the part's list (0x0067A850, the list's remove of a
// node) - else the engine's joint walker 0x008A53E0 would put it back -
// inside the Havok lock, one reference kept on it so a removal the world
// still has queued never meets a freed constraint.
//
// Which joints: those the part's own body holds to a body outside the part
// (game/CutSkeletonLogic.h ReleasesPartJoint) - whatever that body's bone is
// called: Spine2 for a human head, Neck for a goblin's (the tester's goblins
// of 2026-11-26 kept their heads, "0 neck joint(s) released").

#include "core/Types.h"

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

// The engine's collision getter, the joint's removal and the list's remove,
// checked once by their first bytes (logs "Dismember: the engine's joint and
// list functions as read" or "NOT as read - no cut").
bool CutJointsVerified();

// The bhkRigidBody wrapper of a bone's collision object, or 0.
UInt32 CutBodyOf(const NiAVObject* node);

// The joints `part`'s own body holds to bodies outside the part, out of the
// Havok world and out of its list: how many.
UInt32 ReleasePartJoints(const NiAVObject* part);

}  // namespace obvr::game
