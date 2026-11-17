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

// What is drawn this frame: the quad's corners in the holding controller's
// frame (OpenVR's axes - x right, y up, z back - in metres; top left, top
// right, bottom left, bottom right) with their texture coordinates, and the
// layer's texture (IDirect3DTexture9).
struct TabletQuadFrame {
	bool valid = false;
	NiPoint3 corners[4] = {};
	float u[4] = {};
	float v[4] = {};
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
	TabletQuadFrame m_frame;
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
