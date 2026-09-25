#include "render/LaserLayer.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "vr/HandInput.h"
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

vr::openvr::HmdMatrix34 LaserBeamTransform(float lengthMetres, float pitchDegrees,
                                           float yawDegrees, float originMetres) {
	// Columns are the quad's axes in the controller's frame: its up is the
	// beam's direction (LaserDirectionLocal, the same the hit test uses), its
	// right the controller's x turned by the same yaw (LaserRightLocal), its
	// front right x up, which keeps the frame right-handed - with no angles
	// the controller's up (x cross -z = +y). The beam starts originMetres
	// along its direction and its centre sits half its length further out.
	const NiPoint3 up = vr::LaserDirectionLocal(pitchDegrees, yawDegrees);
	const NiPoint3 right = vr::LaserRightLocal(yawDegrees);
	const NiPoint3 front{right.y * up.z - right.z * up.y, right.z * up.x - right.x * up.z,
	                     right.x * up.y - right.y * up.x};
	const float along = originMetres + 0.5f * lengthMetres;
	vr::openvr::HmdMatrix34 m{};
	m.m[0][0] = right.x;
	m.m[1][0] = right.y;
	m.m[2][0] = right.z;
	m.m[0][1] = up.x;
	m.m[1][1] = up.y;
	m.m[2][1] = up.z;
	m.m[0][2] = front.x;
	m.m[1][2] = front.y;
	m.m[2][2] = front.z;
	m.m[0][3] = up.x * along;
	m.m[1][3] = up.y * along;
	m.m[2][3] = up.z * along;
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
                        float pitchDegrees, float yawDegrees, float originMetres,
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

void LaserLayer::Destroy() {
	m_overlay = vr::openvr::kOverlayHandleInvalid;
	m_overlayVisible = false;
	m_placed = false;
}

}  // namespace obvr::render
