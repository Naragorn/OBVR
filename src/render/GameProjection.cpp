#include "render/GameProjection.h"

#include "render/D3D11Types.h"
#include "render/D3D9Types.h"

namespace obvr::render {

bool IsPlausibleProjection(float m00, float m11) {
	// A perspective projection's first two diagonal elements are
	// 1/tan(half-angle), so they are positive and finite. Zero or negative is
	// not a narrow view, it is not a projection.
	if (!(m00 > 0.0f) || !(m11 > 0.0f)) {
		return false;
	}

	// 1/tan(10 degrees) is 5.67, and 1/tan(80 degrees) is 0.176. Anything
	// between those is a field of view somebody might have chosen; outside
	// them is either identity, a scratch matrix, or something that is not a
	// projection.
	//
	// Identity sits at exactly 1.0 in both, which is inside this range - so
	// the range alone does not reject it, and the check below does. A square
	// view is possible in principle and Oblivion is not rendering one: it
	// renders whatever iSize W and H say, and those are equal only if someone
	// deliberately made them so.
	if (m00 < 0.176f || m00 > 5.67f || m11 < 0.176f || m11 > 5.67f) {
		return false;
	}

	// Exactly equal, to the bit, in both elements is identity or a square
	// frustum. A real projection at a real aspect ratio differs in the two,
	// and a comparison this strict cannot reject a genuine square view by
	// accident - it can only reject one that is square to the last bit, which
	// a computed frustum is not.
	if (m00 == m11) {
		return false;
	}

	return true;
}

bool ReadGameProjection(void* gameDevice, GameProjection& out) {
	auto getTransform =
		d3d9::Method<d3d9::GetTransformFn>(gameDevice, d3d9::kDeviceGetTransform);
	if (getTransform == nullptr) {
		return false;
	}

	d3d9::Matrix4 matrix{};
	if (d3d11::Failed(getTransform(gameDevice, d3d9::kTransformProjection, &matrix))) {
		return false;
	}

	const float m00 = matrix.m[0][0];
	const float m11 = matrix.m[1][1];
	if (!IsPlausibleProjection(m00, m11)) {
		return false;
	}

	out.tanHalfWidth = 1.0f / m00;
	out.tanHalfHeight = 1.0f / m11;
	out.measured = true;
	return true;
}

}  // namespace obvr::render
