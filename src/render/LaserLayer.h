#pragma once

#include "core/Types.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::render {

// The laser beam: a thin quad hung on a controller, from its tip along its
// pointing axis for as long as the way to whatever it points at.
//
// The one picture in OBVR that does not come from the game: a few thousand
// pixels of a bright line fading towards the tip, drawn once on the CPU and
// handed to the compositor as raw pixels - no Vulkan interop, no game
// texture, nothing the DXVK route has to carry. The quad's width is
// re-derived from the length each frame, so the beam stays the same
// thickness to the eye at every length: the texture is 1 by 256, and the
// compositor sizes the quad by width and the texture's aspect.
class LaserLayer {
public:
	~LaserLayer() { Destroy(); }

	LaserLayer() = default;
	LaserLayer(const LaserLayer&) = delete;
	LaserLayer& operator=(const LaserLayer&) = delete;

	// Once per frame, after the eyes. pointing false hides the laser; true
	// hangs the beam (withBeam) and the dot at its end (withDot) on the given
	// tracked device for the given length - each on its own switch
	// (vr::LaserPartsShown).
	void Submit(vr::OpenVRBackend& backend, bool pointing, UInt32 deviceIndex, float pitchDegrees,
	            float yawDegrees, float originMetres, float lengthMetres, bool withBeam,
	            bool withDot);

	void Destroy();

private:
	bool EnsureOverlay(vr::OpenVRBackend& backend);
	// The dot at the beam's end, its own small overlay facing back along the beam.
	void SubmitDot(vr::OpenVRBackend& backend, bool visible, UInt32 deviceIndex, float pitchDegrees,
	               float yawDegrees, float originMetres, float lengthMetres);
	vr::openvr::VROverlayHandle m_dot = vr::openvr::kOverlayHandleInvalid;
	bool m_dotTried = false;
	bool m_dotVisible = false;

	vr::openvr::VROverlayHandle m_overlay = vr::openvr::kOverlayHandleInvalid;
	bool m_overlayTried = false;
	bool m_overlayVisible = false;
	bool m_placed = false;
	UInt32 m_placedDevice = 0;
	float m_placedLength = 0.0f;
	float m_placedPitch = 0.0f;
	float m_placedYaw = 0.0f;
	float m_placedOrigin = 0.0f;
	bool m_reported = false;
};

// The beam's device-to-overlay transform: the quad's up axis along the
// controller's pointing direction (-z), its centre half the length out, so
// the quad runs from the controller's origin to the length. Pure, so the
// axes can be checked without a headset.
vr::openvr::HmdMatrix34 LaserBeamTransform(float lengthMetres, float pitchDegrees,
                                           float yawDegrees, float originMetres);

// The quad's width for a beam of this length: the texture is kLaserTextureWidth
// by kLaserTextureHeight, so the compositor draws it length / aspect wide.
constexpr UInt32 kLaserTextureWidth = 4;
constexpr UInt32 kLaserTextureHeight = 1024;
constexpr UInt32 kLaserDotTexture = 32;
// The beam over the menus and the HUD (order 0), the dot over the beam.
constexpr UInt32 kLaserSortOrder = 10;
float LaserBeamWidth(float lengthMetres);

}  // namespace obvr::render
