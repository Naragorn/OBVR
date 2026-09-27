#pragma once

#include <vector>

#include "render/GameDevice.h"
#include "render/InteropBracket.h"
#include "render/TestPattern.h"
#include "ui/MenuCanvas.h"
#include "vr/OpenVRBackend.h"

namespace obvr::ui {

// A picture OBVR paints itself, hung in the room as an OpenVR overlay: the
// pieces SettingsMenuLayer is made of (a dynamic Direct3D texture, the DXVK
// interop, the overlay), for a panel that is not the settings menu - the quick
// menu's ring. The picture is painted into memory first and copied into the
// texture from there, so the last picture can also be written out as a BMP
// for the test runner (SaveBmp).
//
// Painted with the colours as the texture keeps them (A8R8G8B8: blue in the
// Pixel's r), the way SettingsMenuLayer's theme is.
class CanvasOverlay {
public:
	using PaintFn = void (*)(Canvas& canvas, const void* context);

	CanvasOverlay(const char* overlayKey, const char* overlayName, UInt32 width, UInt32 height)
		: m_key(overlayKey), m_name(overlayName), m_width(width), m_height(height) {}
	~CanvasOverlay() { Destroy(); }
	CanvasOverlay(const CanvasOverlay&) = delete;
	CanvasOverlay& operator=(const CanvasOverlay&) = delete;

	// Shows the picture at an absolute pose in tracking space, widthMetres
	// wide, repainting it through `paint` when `revision` differs from the
	// last one painted. False when any piece is missing; the reason is in the
	// log once.
	bool Show(vr::OpenVRBackend& backend, void* gameDevice, const vr::openvr::HmdMatrix34& pose,
	          float widthMetres, UInt32 revision, PaintFn paint, const void* context);

	// Hides it; cheap when hidden already. Called on every frame it is not
	// shown, as the settings layer does.
	void Hide(vr::OpenVRBackend& backend);

	bool IsVisible() const { return m_visible; }

	// The last painted picture as a 32-bit BMP. False before the first paint
	// or when the file cannot be written.
	bool SaveBmp(const char* path) const;

	void Destroy();

private:
	bool EnsureTexture(void* gameDevice);
	bool Upload();

	const char* m_key;
	const char* m_name;
	UInt32 m_width;
	UInt32 m_height;
	std::vector<render::Pixel> m_pixels;

	void* m_texture = nullptr;  // IDirect3DTexture9
	void* m_surface = nullptr;  // IDirect3DSurface9, level 0
	void* m_interop = nullptr;  // ID3D9VkInteropTexture
	render::BackBufferImage m_image;
	bool m_textureTried = false;
	render::VulkanContext m_vulkan;
	bool m_vulkanChecked = false;
	bool m_vulkanUsable = false;
	render::InteropBracket m_bracket;

	vr::openvr::VROverlayHandle m_overlay = vr::openvr::kOverlayHandleInvalid;
	bool m_overlayTried = false;
	bool m_visible = false;
	bool m_painted = false;
	UInt32 m_paintedRevision = 0;
	float m_placedWidth = 0.0f;
	bool m_failureReported = false;
};

}  // namespace obvr::ui
