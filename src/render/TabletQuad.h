#pragma once

// The tablet's picture drawn into the eyes' pictures (docs/inventory-tablet-
// spec.md section 3, B): the cut of the 2D layer the player's menus are on,
// as one textured quad on the hand that holds it - into the back buffer
// while an eye's picture is still there, against the game's own depth and
// through the projection the game drew with, the way the controllers are
// (render/ControllerModels.h, DrawIntoWorld). So the tapping hand is seen in
// front of the page instead of behind an overlay.
//
// The picture is the layer's texture as the interface pass last drew it:
// the first eye's capture comes before this frame's 2D pass (Tracker::
// hudBetweenPasses), the second after it - the content lags a frame in one
// eye, the place never does.

#include "core/Types.h"
#include "game/NiMath.h"
#include "render/ControllerModels.h"
#include "render/D3D9Types.h"

namespace obvr::render {

// What is drawn this frame (the rolls at the open part's edges last, while it
// unrolls): up to ten quads on the hand of the left role,
// in order - the page (the layer's picture); with the figure on a page of its
// own (vr::SetFigurePage) that page's edge and paper in plain colour and the
// figure; with the figure behind a tab (vr::SetFigureTab) the two tabs with a
// small picture of the page and of the figure, and the figure on paper over
// the page while its tab is chosen. Corners in the holding controller's frame (OpenVR's axes - x
// right, y up, z back - in metres; top left, top right, bottom left, bottom
// right) with their texture coordinates; the layer's texture
// (IDirect3DTexture9) for the textured ones.
struct TabletQuadPiece {
	bool valid = false;
	bool textured = true;
	UInt32 colour = 0xFFFFFFFFu;  // ARGB, for a plain one
	// The game's page paper (vr/TabletPaper.h) instead of the plain colour,
	// u and v then in the paper's texture, its colour times `tint`; the plain
	// colour while the paper is not loaded.
	bool paper = false;
	UInt32 tint = 0xFFFFFFFFu;
	NiPoint3 corners[4] = {};
	float u[4] = {};
	float v[4] = {};
};

inline constexpr UInt32 kTabletPieces = 10;

struct TabletQuadFrame {
	bool valid = false;
	TabletQuadPiece pieces[kTabletPieces];
	void* texture = nullptr;
};

class TabletQuad {
public:
	void Set(const TabletQuadFrame& frame) { m_frame = frame; }

	// Into the back buffer for one eye, on the hand of the left role
	// (view.hands[1]). False, drawing nothing, without a quad, a hand, or the
	// world camera's projection on the device.
	bool DrawIntoWorld(void* device, const ControllerModels::WorldView& view, bool leftEye);

	// Both eyes had the tablet drawn into them since the last asking - the
	// overlay need not show it - and a fresh count from here. Asked once a
	// frame, before the frame's captures: a frame with no capture at all
	// answers no, and the overlay shows the tablet.
	bool TakeDrawnBoth() {
		const bool both = m_eyes == 3;
		m_eyes = 0;
		++m_serial;
		return both;
	}

private:
	// The paper's texture on the game's device, made the first time a piece
	// asks for it; tried once.
	void* Paper(void* device);

	TabletQuadFrame m_frame;
	void* m_paper = nullptr;
	bool m_paperTried = false;
	UInt32 m_eyes = 0;  // bit 1 left, bit 2 right
	// The frame's count (TakeDrawnBoth), and the world projection of its
	// first eye, for the second when the 2D pass between them set another.
	UInt32 m_serial = 1;
	UInt32 m_projectionSerial = 0;
	d3d9::Matrix4 m_projection{};
	bool m_reported = false;
	bool m_refusedReported = false;
};

}  // namespace obvr::render
