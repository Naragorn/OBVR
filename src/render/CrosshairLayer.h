#pragma once

#include "core/Types.h"
#include "render/D3D9Types.h"
#include "render/GameDevice.h"
#include "render/InteropBracket.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::render {

// The crosshair, as a quad of its own at the depth it is aiming at.
//
// This exists because of a fact about eyes rather than about rendering. The
// whole 2D interface reaches the headset as one flat overlay hung at a fixed
// distance (HudLayer), and the crosshair rides along in it. Anything that is
// not in the plane the eyes are converged on is seen twice - so a crosshair
// two metres away, laid over a target ten metres away, is physiologically
// doubled. Nothing is drawn wrong; there is simply one crosshair at the wrong
// depth, and the fix is to put it at the right one rather than to draw it
// differently.
//
// So the crosshair gets its own overlay, placed straight ahead at a distance
// this class is told, and the vanilla one is taken out of the flat layer
// (Oblivion's own bCrossHair setting under [GamePlay] does that).
//
// The texture is drawn once, procedurally, out of coloured rectangles: four
// bars around a gap in the middle, each with a dark outline so the thing stays
// visible against a bright sky as well as against a dungeon wall. No asset, no
// file to ship, and nothing to go missing.
class CrosshairLayer {
public:
	~CrosshairLayer() { Destroy(); }

	CrosshairLayer() = default;
	CrosshairLayer(const CrosshairLayer&) = delete;
	CrosshairLayer& operator=(const CrosshairLayer&) = delete;

	// Once per frame, after the eyes have been submitted.
	//
	// visible false hides the overlay - a menu is up, the world is not being
	// drawn, or the feature is off - so a crosshair does not hang in front of
	// an inventory the player is reading.
	//
	// distanceMetres and widthMetres arrive already decided - see
	// PlaceCrosshair, which clamps them and works the width out from the
	// distance so the crosshair keeps its apparent size. Nothing here
	// recomputes either: one place decides, and it is the one with tests.
	void Submit(vr::OpenVRBackend& backend, void* gameDevice, bool visible,
	            float distanceMetres, float widthMetres);

	void Destroy();

private:
	bool EnsureTexture(void* gameDevice);
	bool DrawCrosshair(void* gameDevice);
	bool EnsureOverlay(vr::OpenVRBackend& backend);
	void Place(vr::OpenVRBackend& backend, float distanceMetres, float widthMetres);

	void* m_texture = nullptr;  // IDirect3DTexture9
	void* m_surface = nullptr;  // IDirect3DSurface9, level 0
	void* m_interop = nullptr;  // ID3D9VkInteropTexture
	BackBufferImage m_image;

	bool m_textureTried = false;
	bool m_drawn = false;

	vr::openvr::VROverlayHandle m_overlay = vr::openvr::kOverlayHandleInvalid;
	bool m_overlayTried = false;
	bool m_overlayVisible = false;

	// What the quad was last placed at, so a frame that changes nothing does
	// not talk to the compositor. Once the distance follows what the aim ray
	// hits, this will change most frames - but it will also change by
	// millimetres, and the comparison is what keeps those from becoming calls.
	float m_placedDistance = 0.0f;
	float m_placedWidth = 0.0f;
	bool m_placed = false;

	VulkanContext m_vulkan;
	bool m_vulkanChecked = false;
	bool m_vulkanUsable = false;

	bool m_liveReported = false;
	bool m_failureReported = false;

	InteropBracket m_bracket;
};

}  // namespace obvr::render
