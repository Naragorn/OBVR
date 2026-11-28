#pragma once

// The mirror's picture laid onto the glass as it is when drawn (docs/
// mirror-spec.md 13). The picture was rendered a frame earlier through a
// steadied glass with a margin (vr/MirrorSteadyLogic.h); the glass in the hand
// stands a little elsewhere now. Each of its four corners is projected through
// the camera the picture was rendered with, and that is where the corner reads
// the picture: the reflected world stays where it was rendered, whatever the
// glass in the hand does around it, and the frame of lateness the glass had
// is gone with the same step.
//
// The glass and the picture's image plane are no longer parallel, so the
// mapping is projective: plain (u, v) per corner would bend along the
// diagonal. Each corner gets (s, t, q) = (u q, v q, q), q its depth in the
// mirror camera (over the corners' mean, so q stays near 1); the device
// interpolates s, t and q over the glass and divides per pixel
// (D3DTTFF_PROJECTED, honoured by the fixed-function pixel pipeline for
// pre-transformed vertices too - Microsoft's D3DTEXTURETRANSFORMFLAGS page;
// DXVK passes the third coordinate as the divisor, its fixed-function shader
// via DeepWiki, 2026-11-28). Exact, because u q, v q and q are affine over the
// glass's plane [D].
//
// Pure; covered by mirror_glass_map_test. Drawn by render/TabletQuad.cpp.

#include <cstdint>

#include "game/NiMath.h"
#include "vr/MirrorGlassLogic.h"

namespace obvr::vr {

// The camera a picture was rendered with and the part of the texture it
// fills: what the glass needs to read it.
struct MirrorPictureView {
	bool valid = false;
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	NiPoint3 forward{0.0f, 0.0f, 0.0f};
	NiPoint3 up{0.0f, 0.0f, 0.0f};
	NiPoint3 right{0.0f, 0.0f, 0.0f};
	float left = 0.0f;  // the frustum's tangents along right and up
	float rightEdge = 0.0f;
	float top = 0.0f;
	float bottom = 0.0f;
	float rect[4] = {0.0f, 0.0f, 1.0f, 1.0f};  // left, top, right, bottom in 0..1 of the texture
};

// The view of a rendered camera (MirrorCamera's columns: forward, up, right).
inline MirrorPictureView PictureViewOf(const MirrorCamera& c, const float (&rect)[4]) {
	MirrorPictureView v;
	if (c.view != MirrorView::Ok || !(c.right > c.left) || !(c.top > c.bottom)) {
		return v;
	}
	v.valid = true;
	v.position = c.position;
	v.forward = NiPoint3{c.rotation.data[0][0], c.rotation.data[1][0], c.rotation.data[2][0]};
	v.up = NiPoint3{c.rotation.data[0][1], c.rotation.data[1][1], c.rotation.data[2][1]};
	v.right = NiPoint3{c.rotation.data[0][2], c.rotation.data[1][2], c.rotation.data[2][2]};
	v.left = c.left;
	v.rightEdge = c.right;
	v.top = c.top;
	v.bottom = c.bottom;
	for (int i = 0; i < 4; ++i) {
		v.rect[i] = rect[i];
	}
	return v;
}

// How a glass was mapped: inside the picture; with a corner past the
// picture's edge (drawn, and dark there: the texture outside the picture is
// black); or not at all - a corner at or behind the camera (a projective
// coordinate through zero would wrap the picture around), a corner that is
// no number, or no view - then the glass is drawn plain dark.
enum class GlassMap : std::uint8_t { Inside, Outside, Behind, NoView };

// A corner nearer the camera than this is behind it for the mapping.
inline constexpr float kMirrorMapMinDepthUnits = 0.5f;

// The glass's four corners in the world (top left, top right, bottom left,
// bottom right) to their (s, t, q).
inline GlassMap MapGlassCorners(const MirrorPictureView& v, const NiPoint3 (&corners)[4], float (&s)[4], float (&t)[4],
                                float (&q)[4]) {
	if (!v.valid) {
		return GlassMap::NoView;
	}
	float u[4];
	float w[4];
	float depth[4];
	float depthSum = 0.0f;
	bool outside = false;
	const float width = v.rightEdge - v.left;
	const float height = v.top - v.bottom;
	for (int i = 0; i < 4; ++i) {
		if (!MirrorFinite(corners[i])) {
			return GlassMap::Behind;
		}
		const NiPoint3 d = corners[i] - v.position;
		const float z = MirrorDot(d, v.forward);
		if (!(z >= kMirrorMapMinDepthUnits)) {
			return GlassMap::Behind;
		}
		const float across = (MirrorDot(d, v.right) / z - v.left) / width;
		const float down = (v.top - MirrorDot(d, v.up) / z) / height;
		outside = outside || !(across >= 0.0f && across <= 1.0f && down >= 0.0f && down <= 1.0f);
		u[i] = v.rect[0] + across * (v.rect[2] - v.rect[0]);
		w[i] = v.rect[1] + down * (v.rect[3] - v.rect[1]);
		depth[i] = z;
		depthSum += z;
	}
	const float mean = depthSum * 0.25f;
	for (int i = 0; i < 4; ++i) {
		q[i] = depth[i] / mean;
		s[i] = u[i] * q[i];
		t[i] = w[i] * q[i];
	}
	return outside ? GlassMap::Outside : GlassMap::Inside;
}

// Which of the glass's two pictures an eye draws (render/TabletQuad.cpp): its
// own when two were given (each laid through the camera it was rendered with),
// the one picture for both eyes otherwise.
inline int GlassPictureFor(bool leftEye, bool rightGiven) { return leftEye || !rightGiven ? 0 : 1; }

}  // namespace obvr::vr
