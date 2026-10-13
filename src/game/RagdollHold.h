#pragma once

#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// A fighter's ragdoll under the off hand (GrappleLogic.h, the ragdoll hold;
// docs/physical-combat-spec.md, section 8).
//
// The skeleton's bones carry bhkBlendCollisionObjects with a bhkRigidBody
// each (meshes/characters/_male/skeleton.nif, read 2026-10-13: the spine
// 60, 50 and 40 kg, the head 30, an upper arm 40, a forearm 30, a hand 15,
// a thigh 45, a calf 30, a foot 15 - keyframed while they stand, the
// engine's knockdown turns the ragdoll on and them dynamic). The hold takes
// the body on the part's bone nearest the hand - the collision object
// getter 0x0047FAC0 walks the RTTI parents, so the blend object passes as
// HandBodies' HavokBodyOfNode reads it - and drives it to the hand each
// frame the way a held object is driven (GrabPhysics.h, DriveLinearVelocity:
// activated, its linear velocity set), the rest of the body hanging from it
// through the ragdoll's joints. Let go fast, the same body is sent off with
// the hand's speed (the throw). Nothing here decides; GrappleLogic.h does.

struct RagdollPart {
	UInt32 actor = 0;
	UInt32 bone = 0;  // BladeBone
	UInt32 node = 0;  // the bone's NiNode
	UInt32 body = 0;  // its hkRigidBody
};

// The part's bone nearest the hand that carries a Havok body. `why` names
// the refusal. The bones are the ones CollectBladeBodies kept for the actor
// (BladeBodies.h), so a collect must have walked them.
bool FindRagdollPart(UInt32 actor, BodyPart part, const NiPoint3& hand, RagdollPart& out, const char** why);

// The held body read anew: its motion type (GrabPhysics.h's values; 6 is
// keyframed - the ragdoll not on) and its centre, game units. False when
// the bone's body is no longer the one taken, or nothing reads.
bool ReadRagdollPart(const RagdollPart& p, NiPoint3& centre, UInt32& motionType);

// The body driven to `target` (game units) this frame: activated and given
// the velocity that closes the gap (DriveLinearVelocity, capped at 15 m/s).
// False, and `why`, when it cannot be driven (keyframed, fixed, unreadable).
// `speedUnitsOut`: the velocity given, game units a second.
bool DriveRagdollPart(const RagdollPart& p, const NiPoint3& target, float dtSeconds, float* speedUnitsOut,
                      const char** why);

// The body sent off at `velocityUnits` (game units a second): activated, its
// linear velocity set. False when nothing reads.
bool ThrowRagdollPart(const RagdollPart& p, const NiPoint3& velocityUnits);

}  // namespace obvr::game
