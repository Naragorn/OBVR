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

}  // namespace obvr::vr
