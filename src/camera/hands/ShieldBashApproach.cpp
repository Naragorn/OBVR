#include "camera/hands/ShieldBashApproach.h"

#include "game/BashApproachLogic.h"
#include "platform/Win32Min.h"

namespace obvr::camera::hands {
namespace {

struct Kept {
	game::BashApproachTrail trail;
	double now = 0.0;  // the time of the last frame added
};

Kept& TheKept() {
	static Kept s_kept;
	return s_kept;
}

// Seconds on the performance counter: a gap while StepShieldBash does not run
// (no shield, the hand lost) ages the kept frames out by itself.
double NowSeconds() {
	static const double s_frequency = static_cast<double>(ReadPerformanceFrequency());
	return s_frequency > 0.0 ? static_cast<double>(ReadPerformanceCounter()) / s_frequency : 0.0;
}

}  // namespace

float ShieldBashApproach(const NiPoint3& velocity, const NiPoint3& at, const void* actor, const NiPoint3& centre) {
	Kept& k = TheKept();
	k.now = NowSeconds();
	game::AddBashSample(k.trail, velocity, at, k.now);
	return actor != nullptr ? game::BashApproachSpeed(k.trail, centre, k.now, game::kBashApproachSeconds) : 0.0f;
}

float ShieldBashApproachTowards(const NiPoint3& centre) {
	const Kept& k = TheKept();
	return game::BashApproachSpeed(k.trail, centre, k.now, game::kBashApproachSeconds);
}

}  // namespace obvr::camera::hands
