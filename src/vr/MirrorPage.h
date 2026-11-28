#pragma once

// The mirror page on the tablet (docs/mirror-spec.md 7, stage 1): its frame
// and the glass inside it, the small drawn glass on the Mirror tab, and the
// texture coordinates the reflection lands on once stage 2 renders one.
// Rectangles in the layer's pixels, as the tablet's other pieces. Pure;
// covered by mirror_page_test.

#include "vr/HudTileTree.h"

namespace obvr::vr {

// The frame's width on each side, of the page's width.
inline constexpr float kMirrorFrameShare = 0.05f;

struct MirrorPageRects {
	UiRect frame;  // the page's whole rectangle: the frame is what the glass leaves
	UiRect glass;  // the glass, inside the frame
};

inline MirrorPageRects MirrorPageOf(const UiRect& paper) {
	MirrorPageRects out;
	const float width = paper.right - paper.left;
	const float height = paper.bottom - paper.top;
	const float edge = kMirrorFrameShare * width;
	if (!paper.valid || !(width > 0.0f) || !(height > 2.0f * edge)) {
		return out;
	}
	out.frame = paper;
	out.glass.left = paper.left + edge;
	out.glass.right = paper.right - edge;
	out.glass.top = paper.top + edge;
	out.glass.bottom = paper.bottom - edge;
	out.glass.valid = true;
	return out;
}

// The Mirror tab's picture: a hand mirror, a rim and the glass in it, as tall
// as the other tabs' pictures (0.7 of the tab) and centred in the tab.
inline constexpr float kMirrorIconHeightShare = 0.7f;  // of the tab's height
inline constexpr float kMirrorIconAspect = 0.75f;      // width over height
inline constexpr float kMirrorIconRimShare = 0.16f;    // of the rim's width, on each side

struct MirrorIconRects {
	UiRect rim;
	UiRect glass;
};

inline MirrorIconRects MirrorIconIn(const UiRect& tab) {
	MirrorIconRects out;
	const float tabHeight = tab.bottom - tab.top;
	if (!tab.valid || !(tabHeight > 0.0f) || !(tab.right > tab.left)) {
		return out;
	}
	const float height = kMirrorIconHeightShare * tabHeight;
	const float width = kMirrorIconAspect * height;
	const float middleX = 0.5f * (tab.left + tab.right);
	const float middleY = 0.5f * (tab.top + tab.bottom);
	out.rim.left = middleX - 0.5f * width;
	out.rim.right = middleX + 0.5f * width;
	out.rim.top = middleY - 0.5f * height;
	out.rim.bottom = middleY + 0.5f * height;
	out.rim.valid = true;
	const float rim = kMirrorIconRimShare * width;
	out.glass.left = out.rim.left + rim;
	out.glass.right = out.rim.right - rim;
	out.glass.top = out.rim.top + rim;
	out.glass.bottom = out.rim.bottom - rim;
	out.glass.valid = true;
	return out;
}

// Where the reflection lands on the glass: `rect` is the part of the mirror's
// texture the glass shows (left, top, right, bottom, in 0..1 of the texture),
// the four corners in the strip's order - top left, top right, bottom left,
// bottom right. A mirror shows the picture flipped left to right, so the top
// left corner takes the rectangle's right edge. Stage 2 hands in the rectangle
// (render::TabletQuad::SetMirrorGlass); nothing else about the page changes.
inline void GlassTexCoords(const float (&rect)[4], float (&u)[4], float (&v)[4]) {
	u[0] = rect[2];
	u[1] = rect[0];
	u[2] = rect[2];
	u[3] = rect[0];
	v[0] = rect[1];
	v[1] = rect[1];
	v[2] = rect[3];
	v[3] = rect[3];
}

}  // namespace obvr::vr
