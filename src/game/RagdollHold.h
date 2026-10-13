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
// through the ragdoll's joints. Since the weight (GrappleLogic.h,
// PullLimitedVelocity) the change of the body's velocity is cut to the
// pull's budget over the body's mass inverse (the motion's +0xC0), once per
// Havok step, so the body lags and sags with its mass. Let go fast, the
// same body is sent off with the hand's speed (the throw). Nothing here
// decides; GrappleLogic.h does.

struct RagdollPart {
	UInt32 actor = 0;
	UInt32 bone = 0;  // BladeBone
	UInt32 node = 0;  // the bone's NiNode
	UInt32 body = 0;  // its hkRigidBody
};

// The drive's own memory across the frames of a hold.
struct RagdollDrive {
	NiPoint3 lastSet{0.0f, 0.0f, 0.0f};  // the velocity set last, Havok units a second
	bool haveLast = false;
	float sinceInjected = 0.0f;  // seconds since a change was last given (the budget's time)
	UInt32 injected = 0;         // the changes given, this hold
	UInt32 capped = 0;           // of them, cut to the budget
};

struct RagdollDriveReport {
	float speedUnits = 0.0f;  // the velocity set, game units a second
	float massInv = 0.0f;     // the body's, Havok (0: did not read)
	float accel = 0.0f;       // the change given, Havok units a second squared
	bool injected = false;
	bool capped = false;
};

// The part's bone nearest the hand that carries a Havok body. `why` names
// the refusal. The bones are the ones CollectBladeBodies kept for the actor
// (BladeBodies.h), so a collect must have walked them.
bool FindRagdollPart(UInt32 actor, BodyPart part, const NiPoint3& hand, RagdollPart& out, const char** why);

// The held body read anew: its motion type (GrabPhysics.h's values; 6 is
// keyframed - the ragdoll not on) and its centre, game units. False when
// the bone's body is no longer the one taken, or nothing reads.
bool ReadRagdollPart(const RagdollPart& p, NiPoint3& centre, UInt32& motionType);

// The held body's mass (1 over the motion's mass inverse; 0 when it does
// not read) and its world's gravity along z as a positive number (0 when
// nothing reads). False when the body does not read at all.
bool ReadRagdollMass(const RagdollPart& p, float* mass, float* gravity);

// The masses of the bodies on all the actor's kept bones, summed (the
// ragdoll's whole mass, as far as the bones go), and how many bodies read.
float RagdollMassOf(UInt32 actor, UInt32* bodies);

// The body driven to `target` (game units) this frame: activated and given
// the velocity that closes the gap (DriveLinearVelocity, capped at 15 m/s),
// the change from the velocity read back cut to `pullBudget` over the
// body's mass inverse once per Havok step (0: no limit). False, and `why`,
// when it cannot be driven (keyframed, fixed, unreadable).
bool DriveRagdollPart(const RagdollPart& p, const NiPoint3& target, float dtSeconds, float pullBudget,
                      RagdollDrive& drive, RagdollDriveReport& out, const char** why);

// The body sent off at `velocityUnits` (game units a second): activated, its
// linear velocity set. False when nothing reads.
bool ThrowRagdollPart(const RagdollPart& p, const NiPoint3& velocityUnits);

}  // namespace obvr::game
