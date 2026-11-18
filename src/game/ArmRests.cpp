#include "game/ArmRests.h"

#include "core/AddressSpace.h"
#include "game/ArmTwistLogic.h"

namespace obvr::game {

bool TurnBetween(const NiAVObject* limb, const NiAVObject* node, NiMatrix33& out) {
	out = NiMatrix33::Identity();
	if (limb == nullptr || node == nullptr) {
		return false;
	}
	const NiAVObject* p = node->parent;
	for (UInt32 depth = 0; depth < 3; ++depth) {
		if (p == limb) {
			return true;
		}
		if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p))) {
			return false;
		}
		out = p->localTransform.rot * out;
		p = p->parent;
	}
	return false;
}

bool TakeArmRests(const NiAVObject* upper, const NiAVObject* upperTwist, const NiAVObject* fore,
                  const NiAVObject* foreTwist, const NiAVObject* hand, ArmRests& out) {
	if (upper == nullptr || upperTwist == nullptr || fore == nullptr || foreTwist == nullptr || hand == nullptr) {
		return false;
	}
	NiMatrix33 upperBetween;
	NiMatrix33 foreBetween;
	if (!TurnBetween(upper, upperTwist, upperBetween) || !TurnBetween(fore, foreTwist, foreBetween)) {
		return false;
	}
	out.upper = upper->localTransform.rot;
	out.hand = hand->localTransform.rot;
	out.upperTwist = TwistRestOnLimb(upperBetween, upperTwist->localTransform.rot);
	out.foreTwist = TwistRestOnLimb(foreBetween, foreTwist->localTransform.rot);
	return true;
}

}  // namespace obvr::game
