#include "game/GameCamera.h"

namespace obvr::game {
namespace {

float Abs(float value) { return value < 0.0f ? -value : value; }

// Whether two figures agree to within a fraction of the larger.
//
// A ratio rather than a difference, because the quantities compared below are
// tangents: the same absolute slack means something different at 40 degrees
// than at 90.
bool CloseEnough(float a, float b, float tolerance) {
	const float larger = Abs(a) > Abs(b) ? Abs(a) : Abs(b);
	if (!(larger > 0.0f)) {
		return false;
	}
	return Abs(a - b) / larger <= tolerance;
}

}  // namespace

bool FrustumLooksRight(const NiFrustum& frustum, float tanHalfWidth, float tanHalfHeight) {
	// A near plane of zero or less is not a frustum, and it is also what a
	// wrong address most often produces: whatever those bytes held, read as a
	// float.
	if (!(frustum.n > 0.0f)) {
		return false;
	}

	if (!(frustum.f > frustum.n)) {
		return false;
	}

	// l, r, t and b are distances on the near plane, so dividing by n turns
	// them into the tangents of the half-angles - the same quantity the
	// projection matrix gave. Absolute values, because the sign convention of
	// which edge is which is not settled here and does not need to be: a
	// frustum two units wide spans the same angle whichever way its edges are
	// signed.
	const float halfWidth = (Abs(frustum.l) + Abs(frustum.r)) * 0.5f;
	const float halfHeight = (Abs(frustum.t) + Abs(frustum.b)) * 0.5f;

	// 10 per cent. Wide enough that the two paths rounding differently cannot
	// fail it, narrow enough that reading the wrong object cannot pass it -
	// stray bytes read as floats land nowhere near a plausible tangent, and
	// when they do it is by chance rather than in both axes at once.
	return CloseEnough(halfWidth / frustum.n, tanHalfWidth, 0.10f) &&
	       CloseEnough(halfHeight / frustum.n, tanHalfHeight, 0.10f);
}

bool ReadGameCameraFrustum(NiFrustum& out) {
	// Three dereferences and a null check at each. The scene graph does not
	// exist before the game has a world, and reading through a null one would
	// fault inside a plugin - which is reported as Oblivion crashing.
	auto* sceneGraph = *reinterpret_cast<UInt8**>(kWorldSceneGraph);
	if (sceneGraph == nullptr) {
		return false;
	}

	auto* camera = *reinterpret_cast<UInt8**>(sceneGraph + kSceneGraphCameraOffset);
	if (camera == nullptr) {
		return false;
	}

	out = *reinterpret_cast<NiFrustum*>(camera + kNiCameraFrustumOffset);
	return true;
}

}  // namespace obvr::game
