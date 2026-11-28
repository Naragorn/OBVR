#include "game/BodyUpright.h"

#include "core/Log.h"
#include "game/BodyIkLogic.h"
#include "game/GameCamera.h"
#include "game/SoftPushLogic.h"

namespace obvr::game {

namespace {
// The first eight takes are said: a take is once a skeleton, a load or so.
constexpr UInt32 kTakesSaidMost = 8;
}  // namespace

TrunkUpright StandTrunkUpright(NiAVObject* const* chain, BoneWrite* writes, UInt32 count, NiAVObject* skeleton,
                               float bodyYaw) {
	static UInt32 s_takesSaid = 0;
	// The joints are the chain's after NonAccum: the pelvis up to the head.
	if (chain == nullptr || writes == nullptr || skeleton == nullptr || count < 3 || count > kUprightMostJoints + 1) {
		return TrunkUpright{};
	}
	NiPoint3 joints[kUprightMostJoints];
	for (UInt32 i = 1; i < count; ++i) {
		if (chain[i] == nullptr || chain[i]->parent == nullptr) {
			return TrunkUpright{};
		}
		joints[i - 1] = chain[i]->worldTransform.pos;
	}
	const TrunkUpright up = UprightTrunk(joints, count - 1, skeleton->worldTransform.pos, bodyYaw);
	if (!up.valid) {
		if (s_takesSaid < kTakesSaidMost) {
			++s_takesSaid;
			OBVR_LOG("Body IK: the trunk's pose not stood upright - it reads as no standing pose (BodyUprightLogic.h)");
		}
		return up;
	}
	NiAVObject* const nonAccum = chain[0];
	if (up.moveHips && nonAccum != nullptr && nonAccum->parent != nullptr) {
		const NiPoint3 across = UprightRight(bodyYaw) * -up.hipsAsideUnits;
		NiTransform local = nonAccum->localTransform;
		local.pos = local.pos + WorldOffsetToLocal(nonAccum->parent->worldTransform.rot,
		                                           nonAccum->parent->worldTransform.scale, across);
		WriteLocal(writes[0], local);
		UpdateNodeTransforms(nonAccum);
	}
	const NiPoint3 forward = UprightForward(bodyYaw);
	for (UInt32 k = 0; k < up.turns; ++k) {
		NiAVObject* const bone = chain[k + 1];
		if (up.turn[k] == 0.0f) {
			continue;
		}
		NiTransform local = bone->localTransform;
		local.rot = InverseMatrix(bone->parent->worldTransform.rot) *
		            (ik::TurnAbout(forward, up.turn[k]) * bone->worldTransform.rot);
		WriteLocal(writes[k + 1], local);
		UpdateNodeTransforms(bone);
	}
	if (s_takesSaid < kTakesSaidMost) {
		++s_takesSaid;
		OBVR_LOG("Body IK: the trunk's pose stood upright - it leaned %.1f degrees aside, the hips %.1f units across "
		         "from the root%s (BodyUprightLogic.h)",
		         static_cast<double>(up.leanRadians * math::kRadiansToDegrees),
		         static_cast<double>(up.hipsAsideUnits), up.moveHips ? ", moved back over it" : ", left there");
	}
	return up;
}

}  // namespace obvr::game
