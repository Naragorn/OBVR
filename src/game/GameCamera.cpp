#include "game/GameCamera.h"

namespace obvr::game {
namespace {

float Abs(float value) { return value < 0.0f ? -value : value; }

// Whether two figures agree to within a fraction of the larger.
//
// A ratio rather than a difference, because the quantities compared below are
// tangents: the same absolute slack means something different at 40 degrees
// than at 90.
bool CloseEnoughImpl(float a, float b, float tolerance) {
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

	// l, r, t and b are tangents of the half-angles already, NOT distances on
	// the near plane.
	//
	// This was got wrong once, and the first reading is what settled it. The
	// game reported l=-1.1188 r=1.1188 t=0.6293 b=-0.6293 n=10. Dividing by
	// the near plane gives 12.8 degrees across, which no game renders.
	// Reading them as tangents gives 96.4, which is at least an angle. And
	// 1.1188/0.6293 is 1.7778 to four figures - exactly 16:9, the shape of the
	// frame - which no misreading produces by accident.
	const float halfWidth = (Abs(frustum.l) + Abs(frustum.r)) * 0.5f;
	const float halfHeight = (Abs(frustum.t) + Abs(frustum.b)) * 0.5f;

	// 10 per cent. Wide enough that the two paths rounding differently cannot
	// fail it, narrow enough that reading the wrong object cannot pass it.
	return CloseEnoughImpl(halfWidth, tanHalfWidth, 0.10f) &&
	       CloseEnoughImpl(halfHeight, tanHalfHeight, 0.10f);
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

bool FrustumWatcher::Observe(const NiFrustum& frustum) {
	if (m_reported >= kMaxReports) {
		return false;
	}

	if (!m_seen) {
		m_seen = true;
		m_last = frustum;
		++m_reported;
		return true;
	}

	// One per cent of the wider edge. Loose enough that float noise is not a
	// change, tight enough that a genuinely different pass is.
	const bool moved = !CloseEnoughImpl(frustum.l, m_last.l, 0.01f) ||
	                   !CloseEnoughImpl(frustum.r, m_last.r, 0.01f) ||
	                   !CloseEnoughImpl(frustum.t, m_last.t, 0.01f) ||
	                   !CloseEnoughImpl(frustum.b, m_last.b, 0.01f) ||
	                   !CloseEnoughImpl(frustum.n, m_last.n, 0.01f);

	if (!moved) {
		return false;
	}

	m_last = frustum;
	++m_reported;
	return true;
}

}  // namespace obvr::game
