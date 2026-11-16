#pragma once

// Which limbs a killing strike on a body part can take (camera/hands/Dismember.cpp).
// Pure, covered by dismember_test (TestFeet).

#include "core/Types.h"
#include "game/BodyPart.h"
#include "game/DismemberLogic.h"

namespace obvr::game {

// The limbs a killing strike on a body part can take, in the order tried: an
// arm's capsule its forearm and then the whole arm, a leg's its lower leg and
// then its foot. `out` holds kLimbsOfBodyPartMost; the number written.
inline constexpr UInt32 kLimbsOfBodyPartMost = 2;

inline UInt32 LimbsOfBodyPart(BodyPart part, CutPart* out) {
	switch (part) {
	case BodyPart::LeftArm:
		out[0] = CutPart::LeftForearm;
		out[1] = CutPart::LeftArm;
		return 2;
	case BodyPart::RightArm:
		out[0] = CutPart::RightForearm;
		out[1] = CutPart::RightArm;
		return 2;
	case BodyPart::LeftLeg:
		out[0] = CutPart::LeftLowerLeg;
		out[1] = CutPart::LeftFoot;
		return 2;
	case BodyPart::RightLeg:
		out[0] = CutPart::RightLowerLeg;
		out[1] = CutPart::RightFoot;
		return 2;
	default: return 0;
	}
}

}  // namespace obvr::game
