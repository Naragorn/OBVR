#include "game/RagdollHold.h"

#include "core/AddressSpace.h"
#include "core/MathFns.h"
#include "game/BladeBodies.h"
#include "game/GameTypes.h"
#include "game/GrabPhysics.h"
#include "game/GrappleLogic.h"
#include "game/HandBodies.h"

namespace obvr::game {
namespace {

using obvr::NiAVObject;

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

// The bones each part is held by (BladeContactLogic.h builds the part's
// capsules on them); the nearest to the hand with a body is taken.
constexpr UInt32 kPartBonesMax = 4;

struct PartBones {
	UInt32 bone[kPartBonesMax];
	UInt32 count;
};

PartBones BonesOf(BodyPart part) {
	switch (part) {
	case BodyPart::Torso: return {{kBoneSpine2, kBoneSpine1, kBoneSpine, kBonePelvis}, 4};
	case BodyPart::LeftArm: return {{kBoneLForearm, kBoneLUpperArm, kBoneLHand, 0}, 3};
	case BodyPart::RightArm: return {{kBoneRForearm, kBoneRUpperArm, kBoneRHand, 0}, 3};
	case BodyPart::LeftLeg: return {{kBoneLCalf, kBoneLThigh, kBoneLFoot, 0}, 3};
	case BodyPart::RightLeg: return {{kBoneRCalf, kBoneRThigh, kBoneRFoot, 0}, 3};
	default: return {{0, 0, 0, 0}, 0};
	}
}

// The body's motion (hkRigidBody +0x50) and its type (the motion vtable's
// +0x08); 0 when nothing reads.
UInt32 MotionOf(UInt32 body, UInt32* typeOut) {
	if (!LooksLikeObject(body)) {
		return 0;
	}
	const UInt32 motion = Read(body + kBodyMotionOffset);
	if (!LooksLikeObject(motion) || !LooksLikeObject(Read(motion))) {
		return 0;
	}
	const UInt32 typeSlot = Read(Read(motion) + kMotionTypeSlot);
	if (!LooksLikeObject(typeSlot)) {
		return 0;
	}
	using TypeFn = UInt32(__thiscall*)(void* motion);
	*typeOut = reinterpret_cast<TypeFn>(typeSlot)(reinterpret_cast<void*>(motion)) & 0xFF;
	return motion;
}

// The motion's mass inverse (+0xC0) when it is a number above 0; else 0.
float MassInvOf(UInt32 motion) {
	const float inv = *reinterpret_cast<const float*>(motion + kMotionMassInvOffset);
	return inv > 0.0f && inv < 1.0e6f ? inv : 0.0f;
}

// The world's gravity along z as a positive number (hkRigidBody +0x08 the
// hkWorld, its gravity at +0x20), 0 when nothing reads or it points up.
float GravityOf(UInt32 body) {
	const UInt32 world = Read(body + kBodyWorldOffset);
	if (!LooksLikeObject(world)) {
		return 0.0f;
	}
	const float gz = *reinterpret_cast<const float*>(world + kWorldGravityOffset + 8);
	return gz < 0.0f && gz > -10000.0f ? -gz : 0.0f;
}

// The body still the one taken: the bone's collision object still leads to
// it (a reload builds new ones).
bool PartStillThere(const RagdollPart& p) {
	if (p.actor == 0 || p.node == 0 || p.body == 0) {
		return false;
	}
	const NiAVObject* const node = BladeBoneNode(p.actor, p.bone);
	if (reinterpret_cast<UInt32>(node) != p.node) {
		return false;
	}
	return HavokBodyOfNode(p.node) == p.body;
}

bool SetBodyVelocity(UInt32 body, UInt32 motion, const NiPoint3& havokPerSecond) {
	const UInt32 slot = Read(Read(motion) + kMotionSetLinearVelocitySlot);
	if (!LooksLikeObject(slot)) {
		return false;
	}
	using ActivateFn = void(__thiscall*)(void* body);
	using SetVectorFn = void(__thiscall*)(void* motion, const float* v);
	reinterpret_cast<ActivateFn>(kActivateBody)(reinterpret_cast<void*>(body));
	alignas(16) float linear[4] = {havokPerSecond.x, havokPerSecond.y, havokPerSecond.z, 0.0f};
	reinterpret_cast<SetVectorFn>(slot)(reinterpret_cast<void*>(motion), linear);
	return true;
}

}  // namespace

bool FindRagdollPart(UInt32 actor, BodyPart part, const NiPoint3& hand, RagdollPart& out, const char** why) {
	out = RagdollPart{};
	const PartBones bones = BonesOf(part);
	if (bones.count == 0) {
		*why = "not a part the ragdoll hold takes";
		return false;
	}
	bool anyBone = false;
	float best = 0.0f;
	for (UInt32 i = 0; i < bones.count; ++i) {
		const NiAVObject* const node = BladeBoneNode(actor, bones.bone[i]);
		if (node == nullptr) {
			continue;
		}
		anyBone = true;
		const UInt32 body = HavokBodyOfNode(reinterpret_cast<UInt32>(node));
		if (body == 0) {
			continue;
		}
		const float d = (node->worldTransform.pos - hand).LengthSquared();
		if (out.body == 0 || d < best) {
			best = d;
			out.actor = actor;
			out.bone = bones.bone[i];
			out.node = reinterpret_cast<UInt32>(node);
			out.body = body;
		}
	}
	if (out.body == 0) {
		*why = anyBone ? "no Havok body on the part's bones" : "no bones kept for them";
		return false;
	}
	*why = "";
	return true;
}

bool ReadRagdollPart(const RagdollPart& p, NiPoint3& centre, UInt32& motionType) {
	if (!PartStillThere(p)) {
		return false;
	}
	UInt32 type = 0;
	const UInt32 motion = MotionOf(p.body, &type);
	if (motion == 0) {
		return false;
	}
	const float* const at = reinterpret_cast<const float*>(motion + kMotionTranslationOffset);
	centre = NiPoint3{at[0], at[1], at[2]} * (1.0f / kHavokPerUnit);
	motionType = type;
	return true;
}

bool ReadRagdollMass(const RagdollPart& p, float* mass, float* gravity) {
	*mass = 0.0f;
	*gravity = 0.0f;
	if (!PartStillThere(p)) {
		return false;
	}
	UInt32 type = 0;
	const UInt32 motion = MotionOf(p.body, &type);
	if (motion == 0) {
		return false;
	}
	const float inv = MassInvOf(motion);
	*mass = inv > 0.0f ? 1.0f / inv : 0.0f;
	*gravity = GravityOf(p.body);
	return true;
}

float RagdollMassOf(UInt32 actor, UInt32* bodies) {
	*bodies = 0;
	float sum = 0.0f;
	for (UInt32 bone = 0; bone < kBladeBoneCount; ++bone) {
		const NiAVObject* const node = BladeBoneNode(actor, bone);
		if (node == nullptr) {
			continue;
		}
		const UInt32 body = HavokBodyOfNode(reinterpret_cast<UInt32>(node));
		if (body == 0) {
			continue;
		}
		UInt32 type = 0;
		const UInt32 motion = MotionOf(body, &type);
		if (motion == 0) {
			continue;
		}
		const float inv = MassInvOf(motion);
		if (inv <= 0.0f) {
			continue;
		}
		sum += 1.0f / inv;
		++*bodies;
	}
	return sum;
}

bool DriveRagdollPart(const RagdollPart& p, const NiPoint3& target, float dtSeconds, float pullBudget,
                      RagdollDrive& drive, RagdollDriveReport& out, const char** why) {
	out = RagdollDriveReport{};
	if (!PartStillThere(p)) {
		*why = "the body no longer reads";
		return false;
	}
	UInt32 type = 0;
	const UInt32 motion = MotionOf(p.body, &type);
	if (motion == 0) {
		*why = "the body's motion does not read";
		return false;
	}
	if (!MotionTypeDrivable(type)) {
		*why = type == kMotionTypeKeyframed ? "the body is keyframed - the ragdoll is not on"
		       : type == kMotionTypeFixed   ? "the body is fixed"
		                                    : "the body's motion type is unknown";
		return false;
	}
	const float* const at = reinterpret_cast<const float*>(motion + kMotionTranslationOffset);
	const NiPoint3 current{at[0], at[1], at[2]};
	const float* const vel = reinterpret_cast<const float*>(motion + kMotionLinearVelocityOffset);
	const NiPoint3 now{vel[0], vel[1], vel[2]};
	const NiPoint3 wanted = DriveLinearVelocity(current, target * kHavokPerUnit, dtSeconds);
	drive.sinceInjected += dtSeconds > 0.0f ? dtSeconds : 0.0f;
	const bool stepped = !VelocityStillTheSet(drive.haveLast, now, drive.lastSet);
	out.massInv = MassInvOf(motion);
	const PullVerdict pull =
		PullLimitedVelocity(now, wanted, out.massInv, pullBudget, drive.sinceInjected, stepped, drive.lastSet);
	if (!SetBodyVelocity(p.body, motion, pull.velocity)) {
		*why = "the motion's velocity setter does not read";
		return false;
	}
	drive.lastSet = pull.velocity;
	drive.haveLast = true;
	if (pull.injected) {
		drive.sinceInjected = 0.0f;
		++drive.injected;
		if (pull.capped) {
			++drive.capped;
		}
	}
	out.speedUnits = math::Sqrt(pull.velocity.LengthSquared()) / kHavokPerUnit;
	out.accel = pull.accel;
	out.injected = pull.injected;
	out.capped = pull.capped;
	*why = "";
	return true;
}

bool ThrowRagdollPart(const RagdollPart& p, const NiPoint3& velocityUnits) {
	if (!PartStillThere(p)) {
		return false;
	}
	UInt32 type = 0;
	const UInt32 motion = MotionOf(p.body, &type);
	if (motion == 0) {
		return false;
	}
	return SetBodyVelocity(p.body, motion, velocityUnits * kHavokPerUnit);
}

}  // namespace obvr::game
