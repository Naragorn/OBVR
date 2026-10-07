#pragma once

#include "core/Types.h"
#include "render/D3D9Types.h"
#include "render/GameDevice.h"
#include "render/InteropBracket.h"
#include "vr/HandHud.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::render {

// The HUD on the hands (docs/hud-on-hands-spec.md): parts of the game's own
// HUD lifted out of the captured interface, each onto a quad of its own - on
// the back of a hand, or for the compass in the sky.
//
// The same operation the crosshair does (CrosshairLayer::TakeFromHud), for
// several rectangles: copied into one atlas texture, each into its own slot,
// and erased where they came from so the main panel does not show them a
// second time. Every quad shows the atlas through its own texture bounds.
class HandHudLayer {
public:
	~HandHudLayer() { Destroy(); }

	HandHudLayer() = default;
	HandHudLayer(const HandHudLayer&) = delete;
	HandHudLayer& operator=(const HandHudLayer&) = delete;

	// Lifts every element with a valid rectangle and `lift` set out of the
	// captured interface: copied into its atlas slot when `show` is set,
	// erased from the capture either way. Returns how many were copied.
	UInt32 Lift(void* gameDevice, void* hudSurface, UInt32 hudWidth, UInt32 hudHeight,
	            const vr::CaptureRect* rects, const bool* lift, const bool* show);

	// Where each element's quad goes this frame.
	struct Placement {
		bool shown = false;
		bool onDevice = false;  // relative to a controller; else absolute in the room
		UInt32 device = 0;
		vr::openvr::HmdMatrix34 pose{};
		float widthMetres = 0.0f;
		float alpha = 1.0f;
	};

	// Once per frame after Lift: shows the lifted elements where `placements`
	// say, hides the rest. `visible` false hides them all (a menu, no world).
	void Submit(vr::OpenVRBackend& backend, void* gameDevice, bool visible,
	            const Placement* placements);

	// The element's lifted picture in atlas pixels this frame (for its width).
	bool Lifted(UInt32 element) const { return element < vr::kHudElementCount && m_lifted[element]; }

	// The atlas as a picture, for the harness (DumpSurfaceBmp). False when
	// nothing was ever lifted.
	bool DumpAtlas(void* gameDevice, const char* path) const;

	void Destroy();

private:
	bool EnsureTexture(void* gameDevice);
	bool EnsureOverlay(vr::OpenVRBackend& backend, UInt32 element);

	void* m_texture = nullptr;  // IDirect3DTexture9
	void* m_surface = nullptr;  // IDirect3DSurface9, level 0
	void* m_interop = nullptr;  // ID3D9VkInteropTexture
	BackBufferImage m_image;
	bool m_textureTried = false;

	bool m_lifted[vr::kHudElementCount] = {};
	vr::CaptureRect m_slot[vr::kHudElementCount] = {};

	vr::openvr::VROverlayHandle m_overlay[vr::kHudElementCount] = {};
	bool m_overlayTried[vr::kHudElementCount] = {};
	bool m_overlayVisible[vr::kHudElementCount] = {};
	// Whether the overlay has been handed the atlas once: SteamVR makes its
	// shared texture for an overlay on the first SetOverlayTexture, ~100 ms
	// on the frame's thread, so every overlay gets the atlas on the first
	// submit while hidden, rather than the first time a hand is looked at
	// (the tester, 2026-10-07: "beim ersten mal sehen ein laderuckler").
	bool m_warmed[vr::kHudElementCount] = {};

	VulkanContext m_vulkan;
	bool m_vulkanChecked = false;
	bool m_vulkanUsable = false;

	bool m_liftReported = false;
	bool m_liveReported = false;
	bool m_failureReported = false;

	InteropBracket m_bracket;
};

}  // namespace obvr::render
