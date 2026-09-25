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

	// Once per frame: shown at trackingToMarker when visible, hidden otherwise.
	void Submit(vr::OpenVRBackend& backend, bool visible,
	            const vr::openvr::HmdMatrix34& trackingToMarker);

private:
	vr::openvr::VROverlayHandle m_overlay = vr::openvr::kOverlayHandleInvalid;
	bool m_tried = false;
	bool m_visible = false;
	bool m_reported = false;
};

// The ring's size in the room, and its texture's side.
constexpr float kReachMarkerWidthMetres = 0.07f;
constexpr UInt32 kReachMarkerTexture = 64;

}  // namespace obvr::render
