#pragma once

#include "core/MathFns.h"
#include "vr/HandInput.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// Where the teleport's arc and ring hang in the room, as overlay transforms
// in tracking space (OpenVR axes: x right, y up, z back, metres). Pure, so
// the axes are tested without a headset (TeleportTest).
//
// An overlay is a flat quad drawn over the world. The arc is a chain of
// thin ones, each laid along its piece of the curve and turned about that
// line to face the eyes, so it never shows its edge. The ring lies on the
// ground, its face along the ground's normal.

inline NiPoint3 NormalizedOr(const NiPoint3& v, const NiPoint3& fallback) {
	const float lengthSquared = v.LengthSquared();
	if (!(lengthSquared > 1.0e-12f)) {
		return fallback;
	}
	return v * (1.0f / math::Sqrt(lengthSquared));
}

// A quad whose columns are right, up and front at `centre`.
inline openvr::HmdMatrix34 FrameAt(const NiPoint3& right, const NiPoint3& up,
                                   const NiPoint3& front, const NiPoint3& centre) {
	openvr::HmdMatrix34 m{};
	m.m[0][0] = right.x;
	m.m[1][0] = right.y;
	m.m[2][0] = right.z;
	m.m[0][1] = up.x;
	m.m[1][1] = up.y;
	m.m[2][1] = up.z;
	m.m[0][2] = front.x;
	m.m[1][2] = front.y;
	m.m[2][2] = front.z;
	m.m[0][3] = centre.x;
	m.m[1][3] = centre.y;
	m.m[2][3] = centre.z;
	return m;
}

// One piece of the arc from a to b: up along the piece, front towards the
// eyes as far as the piece allows (the eyes' direction with its part along
// the piece taken out), right completing the frame. Looked at straight
// along the piece, any front square to it will do.
inline openvr::HmdMatrix34 ArcSegmentMatrix(const NiPoint3& a, const NiPoint3& b,
                                            const NiPoint3& eyes) {
	const NiPoint3 up = NormalizedOr(b - a, NiPoint3{0.0f, 1.0f, 0.0f});
	const NiPoint3 centre = (a + b) * 0.5f;
	const NiPoint3 toEyes = eyes - centre;
	NiPoint3 front = toEyes - up * Dot(toEyes, up);
	const NiPoint3 fallback =
		NormalizedOr(Cross(up, NiPoint3{0.0f, 1.0f, 0.0f}), NiPoint3{0.0f, 0.0f, 1.0f});
	front = NormalizedOr(front, fallback);
	const NiPoint3 right = Cross(up, front);
	return FrameAt(right, up, front, centre);
}

// The ring on the ground at `at`: front along the ground's normal, up the
// horizontal way from the eyes to the ring (so it lies the same way round
// wherever it is), a hair above the ground so it is not lost in it.
constexpr float kTeleportRingLiftMetres = 0.02f;

inline openvr::HmdMatrix34 GroundRingMatrix(const NiPoint3& at, const NiPoint3& groundNormal,
                                            const NiPoint3& eyes) {
	const NiPoint3 front = NormalizedOr(groundNormal, NiPoint3{0.0f, 1.0f, 0.0f});
	const NiPoint3 away = at - eyes;
	NiPoint3 up = away - front * Dot(away, front);
	up = NormalizedOr(up, NormalizedOr(Cross(front, NiPoint3{1.0f, 0.0f, 0.0f}),
	                                   NiPoint3{0.0f, 0.0f, -1.0f}));
	const NiPoint3 right = Cross(up, front);
	return FrameAt(right, up, front, at + front * kTeleportRingLiftMetres);
}

}  // namespace obvr::vr
