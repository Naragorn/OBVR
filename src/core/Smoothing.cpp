#include "core/Smoothing.h"

#include "core/MathFns.h"

namespace obvr {
namespace {

// The share of the remaining distance to cover this frame, or a negative value
// when there is nothing to smooth with.
float StepFactor(float speedPerSecond, float deltaSeconds) {
	if (!(deltaSeconds > 0.0f) || !(speedPerSecond > 0.0f)) {
		return -1.0f;
	}

	const float factor = speedPerSecond * deltaSeconds;

	// Above 1 the step would overshoot the target and oscillate around it.
	// That is reachable with an ordinary setting and a single long frame, not
	// only with a broken one, which is why it is capped rather than rejected.
	return factor > 1.0f ? 1.0f : factor;
}

}  // namespace

float Approach(float current, float target, float speedPerSecond, float deltaSeconds) {
	const float factor = StepFactor(speedPerSecond, deltaSeconds);
	if (factor < 0.0f) {
		return target;
	}
	return current + (target - current) * factor;
}

Heading Approach(const Heading& current, const Heading& target, float speedPerSecond,
                 float deltaSeconds) {
	const float factor = StepFactor(speedPerSecond, deltaSeconds);
	if (factor < 0.0f) {
		return target;
	}

	// Nearly opposite headings, tested before the easing rather than after it.
	//
	// Afterwards is too late and gives a wrong answer rather than an awkward
	// one. The straight line between two opposite headings runs through the
	// origin, so a step shorter than halfway lands on the near side and
	// renormalises straight back onto the heading it started from: the camera
	// would sit still while the game turned, then flip once the step grew past
	// half. Taking the target as it stands is the only answer that is not
	// arbitrary, and a camera that turned right round in a single frame was
	// cut rather than panned.
	//
	// The threshold is about two and a half degrees short of a full
	// about-face, which is where the easing stops being well conditioned.
	const float alignment = current.cosine * target.cosine + current.sine * target.sine;
	if (alignment <= -0.999f) {
		return target;
	}

	const float cosine = current.cosine + (target.cosine - current.cosine) * factor;
	const float sine = current.sine + (target.sine - current.sine) * factor;

	const float lengthSquared = cosine * cosine + sine * sine;

	// Unreachable given the check above, but a heading that arrived with a
	// length of zero would otherwise divide by it.
	if (lengthSquared < 1.0e-6f) {
		return target;
	}

	const float inverseLength = 1.0f / math::Sqrt(lengthSquared);
	return Heading{cosine * inverseLength, sine * inverseLength};
}

}  // namespace obvr
