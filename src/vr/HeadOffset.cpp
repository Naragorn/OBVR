#include "vr/HeadOffset.h"

#include "core/MathFns.h"

namespace obvr::vr {

NiPoint3 OffsetFromPose(const Quaternion& reference, const NiPoint3& rawPosition,
                        const NiPoint3& referencePosition, float unitsPerMetre) {
	const NiPoint3 delta = rawPosition - referencePosition;
	const NiPoint3 recentered = Rotate(reference.Conjugate(), delta);
	return PositionFromOpenXR(recentered) * unitsPerMetre;
}

NiPoint3 ClampOffset(const NiPoint3& offset, float maxUnits) {
	if (maxUnits <= 0.0f) {
		return offset;
	}

	const float lengthSquared = offset.LengthSquared();
	if (lengthSquared <= maxUnits * maxUnits) {
		return offset;
	}

	return offset * (maxUnits / math::Sqrt(lengthSquared));
}

NiPoint3 Approach(const NiPoint3& current, const NiPoint3& target, float speedPerSecond,
                  float deltaSeconds) {
	if (!(deltaSeconds > 0.0f) || !(speedPerSecond > 0.0f)) {
		return target;
	}

	float factor = speedPerSecond * deltaSeconds;

	// Above 1 the step would overshoot the target and oscillate around it.
	// That is reachable with an ordinary setting and a single long frame, not
	// only with a broken one, which is why it is capped rather than rejected.
	if (factor > 1.0f) {
		factor = 1.0f;
	}

	return current + (target - current) * factor;
}

}  // namespace obvr::vr
