#include "game/BodyStep.h"

#include "core/Log.h"

namespace obvr::game {

const FeetPlantVerdict& SayFootSteps(const FeetPlantVerdict& v, const FeetPlantState& s, const FeetPlantInput& in) {
	static UInt32 s_stepsSaid = 0;
	for (int side = 0; side < 2; ++side) {
		if (!v.began[side] || s_stepsSaid >= 60) {
			continue;
		}
		++s_stepsSaid;
		const float turned = math::WrapAngle(in.bodyYaw - s.foot[side].yaw) * math::kRadiansToDegrees;
		OBVR_LOG("Body IK: the %s foot steps after the body - %.1f units off where it stood, the body turned %.0f "
		         "degrees since",
		         side == 0 ? "right" : "left", static_cast<double>(FootPlantAcross(s.foot[side].at, in.wanted[side])),
		         static_cast<double>(turned));
	}
	return v;
}

}  // namespace obvr::game
