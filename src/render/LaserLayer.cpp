#include "render/LaserLayer.h"

#include "core/Log.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {
namespace {

// Bright and warm, the gold of the menus, fading towards the tip so the
// beam reads as light rather than as a rod. Row 0 is the texture's top,
// which the transform below puts at the far end.
void PaintBeam(UInt8* rgba) {
	for (UInt32 row = 0; row < kLaserTextureHeight; ++row) {
		const float towardsHand = static_cast<float>(row) / static_cast<float>(kLaserTextureHeight - 1);
		const float alpha = 0.25f + 0.75f * towardsHand;
		for (UInt32 col = 0; col < kLaserTextureWidth; ++col) {
			const bool core = col == 1 || col == 2;
			UInt8* const pixel = rgba + (row * kLaserTextureWidth + col) * 4;
			pixel[0] = 255;
			pixel[1] = core ? 236 : 200;
			pixel[2] = core ? 190 : 110;
			pixel[3] = static_cast<UInt8>((core ? 255.0f : 140.0f) * alpha);
		}
	}
}

}  // namespace

vr::openvr::HmdMatrix34 LaserBeamTransform(float lengthMetres) {
	// Columns are the quad's axes in the controller's frame: its right stays
	// the controller's right; its up is the controller's forward (-z); its
	// front is then the controller's up, which keeps the frame right-handed
	// (right x up = front: x cross -z = +y). The centre sits half way out.
	vr::openvr::HmdMatrix34 m{};
	m.m[0][0] = 1.0f;
	m.m[2][1] = -1.0f;
	m.m[1][2] = 1.0f;
	m.m[2][3] = -0.5f * lengthMetres;
	return m;
}

float LaserBeamWidth(float lengthMetres) {
	return lengthMetres * static_cast<float>(kLaserTextureWidth) /
	       static_cast<float>(kLaserTextureHeight);
}

bool LaserLayer::EnsureOverlay(vr::OpenVRBackend& backend) {
	if (m_overlay != vr::openvr::kOverlayHandleInvalid) {
		return true;
	}
	if (m_overlayTried) {
		return false;
	}
	m_overlayTried = true;
	if (!backend.CreateOverlay("obvr.laser", "OBVR laser", m_overlay)) {
		return false;
	}
	static UInt8 pixels[kLaserTextureWidth * kLaserTextureHeight * 4];
	PaintBeam(pixels);
	const int error = backend.SetOverlayRaw(m_overlay, pixels, kLaserTextureWidth,
	                                        kLaserTextureHeight);
	if (error != vr::openvr::kOverlayErrorNone) {
		OBVR_LOG("Laser: SetOverlayRaw failed (%d) - no beam is drawn", error);
		backend.DestroyOverlay(m_overlay);
		m_overlay = vr::openvr::kOverlayHandleInvalid;
		return false;
	}
	backend.SetOverlayAlpha(m_overlay, 0.9f);
	return true;
}

void LaserLayer::Submit(vr::OpenVRBackend& backend, bool visible, UInt32 deviceIndex,
                        float lengthMetres) {
	if (!visible || deviceIndex == vr::openvr::kTrackedDeviceIndexInvalid ||
	    !(lengthMetres > 0.01f)) {
		if (m_overlayVisible && m_overlay != vr::openvr::kOverlayHandleInvalid) {
			backend.HideOverlay(m_overlay);
			m_overlayVisible = false;
		}
		return;
	}
	if (!EnsureOverlay(backend)) {
		return;
	}
	if (lengthMetres > 5.0f) {
		lengthMetres = 5.0f;
	}
	// The transform and width only when they change by more than a
	// centimetre: a beam that follows a hand changes every frame anyway,
	// but one resting on a wrist panel does not have to talk to the
	// compositor twice a frame for nothing.
	const float delta = lengthMetres - m_placedLength;
	if (!m_placed || m_placedDevice != deviceIndex || delta > 0.01f || delta < -0.01f) {
		backend.SetOverlayTransformDeviceRelative(m_overlay, deviceIndex,
		                                          LaserBeamTransform(lengthMetres));
		backend.SetOverlayWidthInMetres(m_overlay, LaserBeamWidth(lengthMetres));
		m_placed = true;
		m_placedDevice = deviceIndex;
		m_placedLength = lengthMetres;
	}
	if (!m_overlayVisible) {
		backend.ShowOverlay(m_overlay);
		m_overlayVisible = true;
		if (!m_reported) {
			m_reported = true;
			OBVR_LOG("Laser: the beam is shown on device %u, %.2f m long", deviceIndex,
			         lengthMetres);
		}
	}
}

void LaserLayer::Destroy() {
	m_overlay = vr::openvr::kOverlayHandleInvalid;
	m_overlayVisible = false;
	m_placed = false;
}

}  // namespace obvr::render
