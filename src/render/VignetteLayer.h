#pragma once

#include "core/Types.h"
#include "render/D3D9Types.h"
#include "render/GameDevice.h"
#include "render/InteropBracket.h"
#include "render/OverlayWarmup.h"
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

	// The texture's side in pixels, square.
	static constexpr UInt32 kTextureSide = 512;

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
	void Update(vr::OpenVRBackend& backend, void* gameDevice, bool visible, float deltaSeconds,
	            float clearDegrees, float strength);

	// A second use of the layer, set once before anything else: its own overlay
	// key and name, and a white ring to be tinted by the overlay's colour
	// instead of a black one.
	void SetIdentity(const char* key, const char* name, bool white) {
		m_key = key;
		m_name = name;
		m_white = white;
	}

	// Held instead of pulsed (the sneak indicator, vr/SneakIndicator.h): shown
	// at `alpha` (0..1) in the colour red, green, blue while `show`, hidden
	// otherwise. Every frame, as Update.
	void UpdateHeld(vr::OpenVRBackend& backend, void* gameDevice, bool show, float alpha, float clearDegrees,
	                float red, float green, float blue);

	// Makes the texture and the overlay and hands the texture to the
	// compositor once with the overlay hidden, so SteamVR's import of it
	// (render/OverlayWarmup.h) is not paid at the first snap turn.
	OverlayWarmResult Warm(vr::OpenVRBackend& backend, void* gameDevice);

	// Throws away the texture and overlay handle. For configuration changes or shutdown.
	void Destroy();

private:
	bool EnsureTexture(void* gameDevice);
	bool FillTexture(void* gameDevice);
	void DestroyTexture();
	// Once per layer: which step kept the vignette from being shown.
	void ReportFailure(const char* why);
	bool m_failureReported = false;
	bool m_shownReported = false;
	float m_holdSeconds = 0.0f;
	bool EnsureOverlay(vr::OpenVRBackend& backend);
	bool VulkanUsable(void* gameDevice);
	bool HandOver(vr::OpenVRBackend& backend, void* gameDevice);
	// The texture made and handed, the overlay placed, shown at this alpha.
	void ShowAt(vr::OpenVRBackend& backend, void* gameDevice, float overlayAlpha);
	const char* m_key = "obvr.vignette";
	const char* m_name = "OBVR Snap Turn Vignette";
	bool m_white = false;
	bool m_handed = false;  // the texture went to SetOverlayTexture once

	// Generate a radial gradient into pixel rows pitch bytes apart: clear at
	// centre, darkening towards edges.
	static void GenerateVignettePixels(UInt32 width, UInt32 height, UInt8* rows, UInt32 pitch,
	                                   float clearDegrees, UInt32 rgb);
	float m_clearDegrees = 25.0f;   // wanted, from the settings
	float m_builtDegrees = -1.0f;   // what the texture holds
	float m_strength = 0.85f;       // the overlay's alpha at full, from the settings

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
	static constexpr float kHoldSeconds = 0.20f;  // at full after it is up
	static constexpr float kFadeOutRate = 4.0f;   // alpha units per second going down: 0.25 s

	VulkanContext m_vulkan;
	bool m_vulkanChecked = false;
	bool m_vulkanUsable = false;

	InteropBracket m_bracket;
};

}  // namespace obvr::render
