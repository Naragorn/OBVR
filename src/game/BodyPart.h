#pragma once

#include "core/Types.h"

namespace obvr::game {

// Which part of a person a capsule is: the off hand's grab holds a part
// (GrappleLogic.h). The column is a body read without its bones.
enum class BodyPart : UInt8 { Column, Head, Neck, Torso, LeftArm, RightArm, LeftLeg, RightLeg };

inline const char* BodyPartName(BodyPart p) {
	switch (p) {
	case BodyPart::Head: return "head";
	case BodyPart::Neck: return "neck";
	case BodyPart::Torso: return "torso";
	case BodyPart::LeftArm: return "left arm";
	case BodyPart::RightArm: return "right arm";
	case BodyPart::LeftLeg: return "left leg";
	case BodyPart::RightLeg: return "right leg";
	default: return "body";
	}
}

}  // namespace obvr::game
