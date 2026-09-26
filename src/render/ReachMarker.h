#pragma once

#include "core/Types.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::render {

// A light-brown ring where an object within the hand's reach stands - the
// object a closed grip would take. For [Hands] ReachMarker: the grab has to
// be learned, and "is it close enough" was the question (2026-09-25).
// Drawn as an overlay, so over the world rather than in it, facing the
// wearer, at the object's reference point.
class ReachMarker {
public:
	ReachMarker() = default;
	ReachMarker(const ReachMarker&) = delete;
	ReachMarker& operator=(const ReachMarker&) = delete;

	// Once per frame: shown at trackingToMarker when visible, hidden otherwise,
	// as opaque as [Hands] ReachMarkerOpacity says (ReachMarkerAlpha).
	void Submit(vr::OpenVRBackend& backend, bool visible,
	            const vr::openvr::HmdMatrix34& trackingToMarker, float opacity);

private:
	vr::openvr::VROverlayHandle m_overlay = vr::openvr::kOverlayHandleInvalid;
	bool m_tried = false;
	bool m_visible = false;
	bool m_reported = false;
	float m_alpha = -1.0f;  // the overlay alpha last set; none yet
};

// The overlay alpha for an opacity setting: 0 to 1, a setting out of range
// (or not a number) clamped into it - a hand-edited INI must not hide the
// ring for good or make the call fail.
inline float ReachMarkerAlpha(float opacity) {
	if (!(opacity > 0.0f)) {
		return 0.0f;
	}
	return opacity < 1.0f ? opacity : 1.0f;
}

// The ring's size in the room, and its texture's side.
constexpr float kReachMarkerWidthMetres = 0.07f;
constexpr UInt32 kReachMarkerTexture = 64;

// The crosshair's icon in the ring's middle: this wide, a few millimetres
// towards the wearer so it is drawn over the ring rather than fighting it.
constexpr float kReachIconWidthMetres = 0.05f;
constexpr float kReachIconLiftMetres = 0.005f;

// The icon's pose from the ring's: moved along the ring's own +z, the side
// that faces the wearer (the ring is turned like the head, whose +z points
// back at the eyes).
inline vr::openvr::HmdMatrix34 ReachIconPose(const vr::openvr::HmdMatrix34& ring) {
	vr::openvr::HmdMatrix34 icon = ring;
	for (int row = 0; row < 3; ++row) {
		icon.m[row][3] += ring.m[row][2] * kReachIconLiftMetres;
	}
	return icon;
}

}  // namespace obvr::render
