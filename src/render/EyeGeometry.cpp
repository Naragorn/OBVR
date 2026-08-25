#include "render/EyeGeometry.h"

#include "core/MathFns.h"

namespace obvr::render {
namespace {

float Abs(float value) { return value < 0.0f ? -value : value; }

// The angle a frustum of the given half-extents covers, in degrees.
//
// atan of each edge separately and added, rather than atan of the sum: the
// tangent is not linear, so adding the tangents and taking one atan gives a
// smaller answer than the truth, and the error grows with the angle. At the
// field of view a headset uses it is not a rounding difference.
float SpanDegrees(float lowEdge, float highEdge) {
	const float low = math::Atan(Abs(lowEdge));
	const float high = math::Atan(Abs(highEdge));
	return (low + high) * math::kRadiansToDegrees;
}

}  // namespace

float HorizontalFovDegrees(const EyeProjection& projection) {
	return SpanDegrees(projection.left, projection.right);
}

float VerticalFovDegrees(const EyeProjection& projection) {
	return SpanDegrees(projection.top, projection.bottom);
}

float HorizontalAsymmetry(const EyeProjection& projection) {
	return projection.right + projection.left;
}

float VerticalAsymmetry(const EyeProjection& projection) {
	return projection.bottom + projection.top;
}

float OpticalCentreU(const EyeProjection& projection) {
	const float width = projection.right - projection.left;
	if (width == 0.0f) {
		return 0.5f;
	}

	// Where zero - the view axis - falls between the two edges.
	return -projection.left / width;
}

float OpticalCentreV(const EyeProjection& projection, bool topIsNegative) {
	const float height = projection.bottom - projection.top;
	if (height == 0.0f) {
		return 0.5f;
	}

	const float fromTopEdge = -projection.top / height;

	// A texture's v runs downwards from the top edge. If the value named
	// "top" is in fact the negative, lower edge - which is what Valve's wiki
	// claims - then the fraction has to be measured from the other end.
	//
	// This is a flag rather than a decision because the convention is not
	// documented, and a centring mark placed on a guess is worse than one
	// placed on nothing: it looks authoritative and is off by however much
	// the frustum is asymmetric.
	return topIsNegative ? 1.0f - fromTopEdge : fromTopEdge;
}

float InterpupillaryDistance(const NiPoint3& leftEye, const NiPoint3& rightEye) {
	const NiPoint3 between = rightEye - leftEye;
	return math::Sqrt(between.LengthSquared());
}

bool IsPlausibleIpd(float metres) {
	// Human interpupillary distance runs roughly 52 to 78 mm across adults,
	// and headsets allow a little beyond that. The bounds are deliberately
	// generous: this exists to catch a misread matrix, which would be out by
	// a factor rather than by a few millimetres.
	return metres > 0.045f && metres < 0.085f;
}

}  // namespace obvr::render
