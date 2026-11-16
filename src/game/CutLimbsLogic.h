#pragma once

// Two small decisions about the limbs that come off (camera/hands/Dismember.cpp,
// game/Dismember.cpp). Pure, covered by dismember_test (TestFeet).

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

// A part of which nothing would lie on the ground without a skin of its own
// ([Hands] DismemberLimbSkin, CutSideLogic.h PartCopyWanted). A foot is 2 of
// a foot mesh's 8 bones (measured 2026-11-15): the mesh is the body's, the
// foot collapses into the stump at the ankle, and only the copy drawn for
// the part shows it. Every other part has a shape that is its own - the
// face, a hand's mesh, a boot or a bare foot at the knee.
inline bool CutNeedsLimbSkin(CutPart part) { return part == CutPart::LeftFoot || part == CutPart::RightFoot; }

// Whether a cut of `part` would show its part: a foot only with the limbs'
// own skin on - with it off the cut is refused, not made invisibly.
inline bool CutShows(CutPart part, bool limbSkin) { return limbSkin || !CutNeedsLimbSkin(part); }

}  // namespace obvr::game
