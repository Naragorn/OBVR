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

// A radial vignette overlay for snap turn feedback.
//
// Shows a dark ring around the edges of the view when triggered, fading in and
// out over a fraction of a second. The texture is static (dark at edges, clear
// at centre) - only the alpha changes per frame to control visibility.
//
// Placed flat ahead of the head like the HUD layer, so it covers both eyes'
// view uniformly regardless of where they are looking.
class VignetteLayer {
public:
	~VignetteLayer() { Destroy(); }

	VignetteLayer() = default;
	VignetteLayer(const VignetteLayer&) = delete;
	VignetteLayer& operator=(const VignetteLayer&) = delete;

	// Trigger a vignette pulse. The effect fades in quickly then out more slowly,
	// giving visual feedback that a snap turn just happened without lingering.
	void Trigger();

	// Once per frame, after the eyes have been submitted. Advances the fade state
	// and updates overlay alpha if needed. Call even when not triggered - it keeps
	// the overlay hidden when nothing is active.
	//
	// visible controls whether vignettes are allowed at all (user setting). When
	// false, any in-progress fade is cancelled immediately.
	void Update(vr::OpenVRBackend& backend, void* gameDevice, bool visible, float deltaSeconds);

	// Throws away the texture and overlay handle. For configuration changes or shutdown.
	void Destroy();

private:
	bool EnsureTexture(void* gameDevice);
	bool EnsureOverlay(vr::OpenVRBackend& backend);

	// Generate a radial gradient into pixel data: clear at centre, darkening towards edges.
	static void GenerateVignettePixels(UInt32 width, UInt32 height, UInt32* pixels);

	void* m_texture = nullptr;  // IDirect3DTexture9
	void* m_surface = nullptr;  // IDirect3DSurface9, level 0
	void* m_interop = nullptr;  // ID3D9VkInteropTexture
	BackBufferImage m_image;

	bool m_textureTried = false;

	vr::openvr::VROverlayHandle m_overlay = vr::openvr::kOverlayHandleInvalid;
	bool m_overlayTried = false;
	bool m_overlayVisible = false;

	// Fade state: alpha ramps up when triggered, then down. 0 means invisible.
	float m_alpha = 0.0f;
	bool m_fadingIn = false;

	// Timing constants for the fade curve. Fast in so it feels responsive, slower out
	// so it does not cut off while the turn is still settling visually.
	static constexpr float kFadeInRate = 25.0f;   // alpha units per second going up
	static constexpr float kFadeOutRate = 12.0f;  // alpha units per second going down

	VulkanContext m_vulkan;
	bool m_vulkanChecked = false;
	bool m_vulkanUsable = false;

	InteropBracket m_bracket;
};

}  // namespace obvr::render
