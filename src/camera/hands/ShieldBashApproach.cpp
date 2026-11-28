#include "camera/hands/ShieldBashApproach.h"

#include "core/MathFns.h"
#include "game/BashApproachLogic.h"
#include "game/MeleeHits.h"
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

game::BashDisc DiscInWorld(const NiPoint3& hand, const NiMatrix33& rot, const game::ShieldDisc* shape) {
	game::BashDisc d;
	d.centre = hand;
	if (shape != nullptr && shape->valid) {
		d.centre = hand + rot * shape->centre;
		d.axisA = rot * shape->axis[0];
		d.axisB = rot * shape->axis[1];
		d.halfA = shape->half[0];
		d.halfB = shape->half[1];
	}
	return d;
}

}  // namespace

BashTouch ShieldBashTouch(const NiPoint3& hand, const NiMatrix33& rot, const game::ShieldDisc* shape,
                          const NiPoint3& velocity, UInt32 restingOn) {
	Kept& k = TheKept();
	k.now = NowSeconds();
	BashTouch t;
	t.disc = DiscInWorld(hand, rot, shape);
	game::AddBashSample(k.trail, velocity, t.disc, k.now);
	// Anyone whose column the disc can reach: its middle within the column grown by its longer half spoke.
	const float reach = t.disc.halfA > t.disc.halfB ? t.disc.halfA : t.disc.halfB;
	float radius = 0.0f;
	void* const candidate = game::LivingActorAt(t.disc.centre, game::kBashColumnSide, game::kBashColumnPad + reach,
		                                            &t.centre, 1.0f, &radius);
	if (candidate == nullptr) {
		return t;
	}
	t.column = game::BashColumnOf(t.centre, radius);
	const game::BashGap g = game::DiscColumnGap(t.disc, t.column);
	t.gap = g.gap;
	const bool onThem = restingOn != 0 && restingOn == reinterpret_cast<UInt32>(candidate);
	if (!game::BashTouches(g, onThem)) {
		return t;
	}
	t.actor = candidate;
	t.byContact = !g.touching;
	t.approach = game::BashApproachSpeed(k.trail, t.column, k.now, game::kBashApproachSeconds);
	return t;
}

float ShieldBashApproachInto(const game::BashColumn& column) {
	const Kept& k = TheKept();
	return game::BashApproachSpeed(k.trail, column, k.now, game::kBashApproachSeconds);
}

NiPoint3 ShieldBashPushFrom(const NiPoint3& at, const NiPoint3& velocity) {
	const float across = math::Sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
	if (!(across > 0.001f)) {
		return at;
	}
	return at - NiPoint3{velocity.x / across, velocity.y / across, 0.0f} * 30.0f;
}

}  // namespace obvr::camera::hands
