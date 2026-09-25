#include "render/LaserLayer.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "vr/LaserGeometry.h"
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

// The dot at the beam's end: the beam's gold, bright in the middle and
// soft at the rim, round within its square.
void PaintDot(UInt8* rgba) {
	const float half = 0.5f * static_cast<float>(kLaserDotTexture);
	for (UInt32 row = 0; row < kLaserDotTexture; ++row) {
		for (UInt32 col = 0; col < kLaserDotTexture; ++col) {
			const float dx = (static_cast<float>(col) + 0.5f - half) / half;
			const float dy = (static_cast<float>(row) + 0.5f - half) / half;
			const float r = math::Sqrt(dx * dx + dy * dy);
			float alpha = r < 0.6f ? 1.0f : (r < 1.0f ? (1.0f - r) / 0.4f : 0.0f);
			UInt8* const pixel = rgba + (row * kLaserDotTexture + col) * 4;
			pixel[0] = 255;
			pixel[1] = static_cast<UInt8>(r < 0.35f ? 250 : 220);
			pixel[2] = static_cast<UInt8>(r < 0.35f ? 225 : 140);
			pixel[3] = static_cast<UInt8>(alpha * 255.0f);
		}
	}
}

}  // namespace

vr::openvr::HmdMatrix34 LaserBeamTransform(float lengthMetres, float pitchDegrees,
                                           float yawDegrees, float originMetres) {
	return vr::LaserBeamMatrix(lengthMetres, pitchDegrees, yawDegrees, originMetres);
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
	// Over the menus and the HUD: at the same order an overlay at the same
	// distance as the menu quad is drawn before or after it by a hair, and
	// the dot at the beam's end was seen under the text (2026-09-25).
	backend.SetOverlaySortOrder(m_overlay, kLaserSortOrder);
	backend.SetOverlayAlpha(m_overlay, 0.9f);
	return true;
}

void LaserLayer::Submit(vr::OpenVRBackend& backend, bool visible, UInt32 deviceIndex,
                        float pitchDegrees, float yawDegrees, float originMetres,
                        float lengthMetres, bool withDot) {
	if (!visible || deviceIndex == vr::openvr::kTrackedDeviceIndexInvalid ||
	    !(lengthMetres > 0.01f)) {
		if (m_overlayVisible && m_overlay != vr::openvr::kOverlayHandleInvalid) {
			backend.HideOverlay(m_overlay);
			m_overlayVisible = false;
		}
		SubmitDot(backend, false, deviceIndex, pitchDegrees, yawDegrees, originMetres,
		          lengthMetres);
		return;
	}
	SubmitDot(backend, withDot, deviceIndex, pitchDegrees, yawDegrees, originMetres,
	          lengthMetres > 5.0f ? 5.0f : lengthMetres);
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
	if (!m_placed || m_placedDevice != deviceIndex || delta > 0.01f || delta < -0.01f ||
	    pitchDegrees != m_placedPitch || yawDegrees != m_placedYaw ||
	    originMetres != m_placedOrigin) {
		backend.SetOverlayTransformDeviceRelative(m_overlay, deviceIndex,
		                                          LaserBeamTransform(lengthMetres, pitchDegrees, yawDegrees,
		                                                             originMetres));
		backend.SetOverlayWidthInMetres(m_overlay, LaserBeamWidth(lengthMetres));
		m_placed = true;
		m_placedDevice = deviceIndex;
		m_placedLength = lengthMetres;
		m_placedPitch = pitchDegrees;
		m_placedYaw = yawDegrees;
		m_placedOrigin = originMetres;
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

void LaserLayer::SubmitDot(vr::OpenVRBackend& backend, bool visible, UInt32 deviceIndex,
                           float pitchDegrees, float yawDegrees, float originMetres,
                           float lengthMetres) {
	if (!visible || deviceIndex == vr::openvr::kTrackedDeviceIndexInvalid ||
	    !(lengthMetres > 0.01f)) {
		if (m_dotVisible && m_dot != vr::openvr::kOverlayHandleInvalid) {
			backend.HideOverlay(m_dot);
			m_dotVisible = false;
		}
		return;
	}
	if (m_dot == vr::openvr::kOverlayHandleInvalid) {
		if (m_dotTried) {
			return;
		}
		m_dotTried = true;
		if (!backend.CreateOverlay("obvr.laser.dot", "OBVR laser dot", m_dot)) {
			OBVR_LOG("Laser: the dot's overlay could not be created - the beam goes without it");
			return;
		}
		static UInt8 pixels[kLaserDotTexture * kLaserDotTexture * 4];
		PaintDot(pixels);
		backend.SetOverlaySortOrder(m_dot, kLaserSortOrder + 1);
		const int error = backend.SetOverlayRaw(m_dot, pixels, kLaserDotTexture, kLaserDotTexture);
		if (error != vr::openvr::kOverlayErrorNone) {
			OBVR_LOG("Laser: SetOverlayRaw failed for the dot (%d) - the beam goes without it",
			         error);
			backend.DestroyOverlay(m_dot);
			m_dot = vr::openvr::kOverlayHandleInvalid;
			return;
		}
	}
	// Every frame: the end of a beam that follows a hand moves every frame.
	backend.SetOverlayTransformDeviceRelative(
		m_dot, deviceIndex,
		vr::LaserPointMatrix(lengthMetres, pitchDegrees, yawDegrees, originMetres));
	backend.SetOverlayWidthInMetres(m_dot, vr::LaserDotWidth(lengthMetres));
	if (!m_dotVisible) {
		backend.ShowOverlay(m_dot);
		m_dotVisible = true;
	}
}

void LaserLayer::Destroy() {
	m_overlay = vr::openvr::kOverlayHandleInvalid;
	m_overlayVisible = false;
	m_placed = false;
	m_dot = vr::openvr::kOverlayHandleInvalid;
	m_dotVisible = false;
}

}  // namespace obvr::render
