#pragma once

#include "render/GameDevice.h"
#include "render/InteropBracket.h"
#include "ui/MenuModel.h"
#include "ui/MenuPainter.h"
#include "vr/OpenVRBackend.h"

namespace obvr::ui {

// OBVR's own settings menu, as a quad hanging in front of the wearer.
//
// Built on the same three pieces the crosshair overlay is: a Direct3D texture,
// an OpenVR overlay, and the interop bracket that hands one to the other. What
// is different is where the pixels come from. The crosshair copies them out of
// the game's own 2D layer; this draws them, which means the texture has to be
// lockable and therefore cannot be a render target.
//
// So it is created DYNAMIC in the default pool rather than as a render target.
// That is the documented way to have a texture whose contents are rewritten
// from the processor, and it costs the ColorFill the crosshair uses - which is
// no loss here, since a menu fills its own background anyway.
//
// It is redrawn only when something changes. A settings menu is static between
// keypresses, and locking and repainting a quarter of a million pixels every
// frame to produce the identical picture would be a cost paid for nothing.

class SettingsMenuLayer {
public:
	~SettingsMenuLayer() { Destroy(); }

	SettingsMenuLayer() = default;
	SettingsMenuLayer(const SettingsMenuLayer&) = delete;
	SettingsMenuLayer& operator=(const SettingsMenuLayer&) = delete;

	// Once per frame, after the eyes have been submitted.
	//
	// visible false hides the overlay. Like the crosshair, it is called on
	// every frame rather than only while the menu is open: an overlay that has
	// been shown once stays shown until something hides it, and this is that
	// something.
	//
	// The rows are drawn from `items` and `categories`, which the caller builds
	// from the settings table each frame - cheap, and it keeps the picture
	// honest when a value changes from somewhere other than this menu.
	//
	// A repaint happens when `revision` differs from the last one drawn. The
	// caller advances it on anything that changes the picture; passing a
	// constant would show the first frame's menu for ever.
	//
	// inWorld stands the panel in the room where the head was when it opened;
	// false carries it on the head instead. See Config::settingsMenuInWorld for
	// why the room is the default and why the other is kept.
	void Submit(vr::OpenVRBackend& backend, void* gameDevice, bool visible, const MenuItem* items,
	            const char* const* categories, UInt32 count, MenuState state, UInt32 revision,
	            float distanceMetres, float widthMetres, bool inWorld);

	// How many rows the menu can show, which the caller needs so its scrolling
	// matches what is drawn. Answered by the layer because the layer owns the
	// texture size.
	UInt32 VisibleRows() const;

	void Destroy();

private:
	bool EnsureTexture(void* gameDevice);
	bool EnsureOverlay(vr::OpenVRBackend& backend);
	bool Repaint(const MenuItem* items, const char* const* categories, UInt32 count,
	             MenuState state);
	void Place(vr::OpenVRBackend& backend, float distanceMetres, float widthMetres,
	           bool inWorld);

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
	bool m_overlayVisible = false;

	// The revision last painted, and whether anything has been painted at all.
	// Kept so a static menu is not repainted every frame for an identical
	// picture.
	UInt32 m_paintedRevision = 0;
	bool m_painted = false;

	bool m_placed = false;
	float m_placedDistance = 0.0f;
	float m_placedWidth = 0.0f;

	bool m_failureReported = false;
	bool m_liveReported = false;
	bool m_lockFailureReported = false;
};

}  // namespace obvr::ui
