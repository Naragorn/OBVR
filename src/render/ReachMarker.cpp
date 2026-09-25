#include "render/ReachMarker.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {
namespace {

// A ring in the parchment's light brown, the colour Oblivion's menus are
// made of - soft-edged, open in the middle so the object shows through.
void PaintRing(UInt8* rgba) {
	const float half = 0.5f * static_cast<float>(kReachMarkerTexture);
	for (UInt32 row = 0; row < kReachMarkerTexture; ++row) {
		for (UInt32 col = 0; col < kReachMarkerTexture; ++col) {
			const float dx = (static_cast<float>(col) + 0.5f - half) / half;
			const float dy = (static_cast<float>(row) + 0.5f - half) / half;
			const float r = math::Sqrt(dx * dx + dy * dy);
			// Full between 0.72 and 0.9 of the radius, fading over 0.1 either side.
			float alpha = 0.0f;
			if (r >= 0.72f && r <= 0.9f) {
				alpha = 1.0f;
			} else if (r > 0.62f && r < 0.72f) {
				alpha = (r - 0.62f) / 0.1f;
			} else if (r > 0.9f && r < 1.0f) {
				alpha = (1.0f - r) / 0.1f;
			}
			UInt8* const pixel = rgba + (row * kReachMarkerTexture + col) * 4;
			pixel[0] = 222;
			pixel[1] = 190;
			pixel[2] = 140;
			pixel[3] = static_cast<UInt8>(alpha * 220.0f);
		}
	}
}

}  // namespace

void ReachMarker::Submit(vr::OpenVRBackend& backend, bool visible,
                         const vr::openvr::HmdMatrix34& trackingToMarker) {
	if (!visible) {
		if (m_visible && m_overlay != vr::openvr::kOverlayHandleInvalid) {
			backend.HideOverlay(m_overlay);
			m_visible = false;
		}
		return;
	}
	if (m_overlay == vr::openvr::kOverlayHandleInvalid) {
		if (m_tried) {
			return;
		}
		m_tried = true;
		if (!backend.CreateOverlay("obvr.reach", "OBVR reach marker", m_overlay)) {
			return;
		}
		static UInt8 pixels[kReachMarkerTexture * kReachMarkerTexture * 4];
		PaintRing(pixels);
		if (backend.SetOverlayRaw(m_overlay, pixels, kReachMarkerTexture, kReachMarkerTexture) !=
		    vr::openvr::kOverlayErrorNone) {
			OBVR_LOG("Hands: the reach marker could not be painted - none is shown");
			backend.DestroyOverlay(m_overlay);
			m_overlay = vr::openvr::kOverlayHandleInvalid;
			return;
		}
		backend.SetOverlayWidthInMetres(m_overlay, kReachMarkerWidthMetres);
	}
	backend.SetOverlayTransformAbsolute(m_overlay, trackingToMarker);
	if (!m_visible) {
		backend.ShowOverlay(m_overlay);
		m_visible = true;
		if (!m_reported) {
			m_reported = true;
			OBVR_LOG("Hands: the reach marker is shown on an object within the hand's reach");
		}
	}
}

}  // namespace obvr::render
