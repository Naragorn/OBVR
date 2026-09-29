#include "render/HandHudLayer.h"

#include <cstdio>

#include "core/Log.h"
#include "render/D3D11Types.h"
#include "render/GameFrame.h"
#include "render/LayoutProbe.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {
namespace {

constexpr UInt32 kTransparent = 0x00000000;

}  // namespace

bool HandHudLayer::EnsureTexture(void* gameDevice) {
	if (m_texture != nullptr) {
		return true;
	}
	if (m_textureTried || gameDevice == nullptr) {
		return false;
	}
	m_textureTried = true;

	auto createTexture = d3d9::Method<d3d9::CreateTextureFn>(gameDevice, d3d9::kDeviceCreateTexture);
	if (createTexture == nullptr) {
		return false;
	}
	// A render target for the same two reasons as the HUD layer and the
	// crosshair: StretchRect and ColorFill need one, and only a texture
	// carries the sampled bit the compositor requires.
	if (d3d11::Failed(createTexture(gameDevice, vr::kHandHudAtlasWidth, vr::kHandHudAtlasHeight, 1,
	                                d3d9::kUsageRenderTarget, d3d9::kFormatA8R8G8B8, d3d9::kPoolDefault,
	                                &m_texture, nullptr)) ||
	    m_texture == nullptr) {
		m_texture = nullptr;
		OBVR_LOG("Hand HUD: no %ux%u A8R8G8B8 target, so the HUD stays in the main panel",
		         vr::kHandHudAtlasWidth, vr::kHandHudAtlasHeight);
		return false;
	}
	auto getSurfaceLevel = d3d9::Method<d3d9::GetSurfaceLevelFn>(m_texture, d3d9::kTextureGetSurfaceLevel);
	if (getSurfaceLevel == nullptr || d3d11::Failed(getSurfaceLevel(m_texture, 0, &m_surface)) ||
	    m_surface == nullptr) {
		m_surface = nullptr;
		return false;
	}
	auto* unknown = static_cast<d3d11::Unknown*>(m_texture);
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr ||
	    d3d11::Failed(unknown->vtbl->QueryInterface(unknown, &kIID_D3D9VkInteropTexture, &m_interop)) ||
	    m_interop == nullptr) {
		m_interop = nullptr;
		return false;
	}
	return ReadImageInfo(m_interop, m_image);
}

UInt32 HandHudLayer::Lift(void* gameDevice, void* hudSurface, UInt32 hudWidth, UInt32 hudHeight,
                          const vr::CaptureRect* rects, const bool* lift, const bool* show) {
	for (bool& lifted : m_lifted) {
		lifted = false;
	}
	if (hudSurface == nullptr || rects == nullptr || lift == nullptr || show == nullptr) {
		return 0;
	}
	bool anyShown = false;
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		anyShown = anyShown || (lift[e] && show[e] && rects[e].valid);
	}
	auto stretchRect = d3d9::Method<d3d9::StretchRectFn>(gameDevice, d3d9::kDeviceStretchRect);
	auto colorFill = d3d9::Method<d3d9::ColorFillFn>(gameDevice, d3d9::kDeviceColorFill);
	if (stretchRect == nullptr || colorFill == nullptr) {
		return 0;
	}
	const bool haveAtlas = anyShown && EnsureTexture(gameDevice);
	// Cleared whole each frame: a slot whose element went (no active effect
	// any more) must not keep its last picture.
	if (haveAtlas && d3d11::Failed(colorFill(gameDevice, m_surface, nullptr, kTransparent))) {
		return 0;
	}

	UInt32 copied = 0;
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		const vr::CaptureRect& r = rects[e];
		if (!lift[e] || !r.valid || r.left < 0 || r.top < 0 ||
		    static_cast<UInt32>(r.right) > hudWidth || static_cast<UInt32>(r.bottom) > hudHeight) {
			continue;
		}
		const d3d9::Rect source{r.left, r.top, r.right, r.bottom};
		if (show[e] && haveAtlas) {
			const vr::CaptureRect slot = vr::AtlasSlotRect(e, r.right - r.left, r.bottom - r.top);
			const d3d9::Rect destination{slot.left, slot.top, slot.right, slot.bottom};
			if (slot.valid && !d3d11::Failed(stretchRect(gameDevice, hudSurface, &source, m_surface,
			                                             &destination, d3d9::kTexFilterLinear))) {
				m_slot[e] = slot;
				m_lifted[e] = true;
				++copied;
			}
		}
		// Erased where it came from once it is on its quad, and for Off: the
		// main panel must not show it a second time, and Off means gone. A copy
		// that failed leaves it where the game drew it.
		if (!show[e] || m_lifted[e]) {
			colorFill(gameDevice, hudSurface, &source, kTransparent);
		}
	}

	if (!m_liftReported && copied > 0) {
		m_liftReported = true;
		char line[256];
		int at = 0;
		for (UInt32 e = 0; e < vr::kHudElementCount && at < static_cast<int>(sizeof(line)) - 48; ++e) {
			if (m_lifted[e]) {
				at += std::snprintf(line + at, sizeof(line) - at, " %s %d,%d..%d,%d;", vr::kHudElementKeys[e],
				                    rects[e].left, rects[e].top, rects[e].right, rects[e].bottom);
			}
		}
		OBVR_LOG("Hand HUD: lifted out of the %ux%u interface -%s", hudWidth, hudHeight, line);
	}
	return copied;
}

bool HandHudLayer::EnsureOverlay(vr::OpenVRBackend& backend, UInt32 element) {
	if (m_overlay[element] != vr::openvr::kOverlayHandleInvalid) {
		return true;
	}
	if (m_overlayTried[element]) {
		return false;
	}
	m_overlayTried[element] = true;
	char key[48];
	char name[64];
	std::snprintf(key, sizeof(key), "obvr.handhud.%s", vr::kHudElementKeys[element]);
	std::snprintf(name, sizeof(name), "Oblivion HUD %s", vr::kHudElementKeys[element]);
	if (!backend.CreateOverlay(key, name, m_overlay[element])) {
		return false;
	}
	backend.SetOverlaySortOrder(m_overlay[element], 4);
	return true;
}

void HandHudLayer::Submit(vr::OpenVRBackend& backend, void* gameDevice, bool visible,
                          const Placement* placements) {
	bool anyToShow = false;
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		const bool show = visible && placements != nullptr && placements[e].shown && m_lifted[e] &&
		                  placements[e].alpha > 0.0f;
		if (!show) {
			if (m_overlayVisible[e]) {
				backend.HideOverlay(m_overlay[e]);
				m_overlayVisible[e] = false;
			}
			continue;
		}
		anyToShow = true;
	}
	if (!anyToShow || m_texture == nullptr) {
		return;
	}

	if (!m_vulkanChecked) {
		m_vulkanChecked = true;
		m_vulkanUsable = GetVulkanContext(gameDevice, m_vulkan);
	}
	if (!m_vulkanUsable || !ReadImageInfo(m_interop, m_image)) {
		return;
	}
	// One bracket for every quad: they all show the same image.
	if (!m_bracket.Begin(gameDevice)) {
		return;
	}
	if (!m_bracket.ToTransferSrc(m_interop, m_image.layout)) {
		m_bracket.Release();
		return;
	}
	dxvk::VRVulkanTextureData data{};
	DescribeForOpenVR(m_image, m_vulkan, data);

	UInt32 shown = 0;
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		const Placement& p = placements[e];
		if (!(visible && p.shown && m_lifted[e] && p.alpha > 0.0f) || !EnsureOverlay(backend, e)) {
			continue;
		}
		const vr::CaptureRect& slot = m_slot[e];
		const vr::openvr::VRTextureBounds bounds{
			static_cast<float>(slot.left) / static_cast<float>(vr::kHandHudAtlasWidth),
			static_cast<float>(slot.top) / static_cast<float>(vr::kHandHudAtlasHeight),
			static_cast<float>(slot.right) / static_cast<float>(vr::kHandHudAtlasWidth),
			static_cast<float>(slot.bottom) / static_cast<float>(vr::kHandHudAtlasHeight)};
		backend.SetOverlayTextureBounds(m_overlay[e], bounds);
		if (p.onDevice) {
			backend.SetOverlayTransformDeviceRelative(m_overlay[e], p.device, p.pose);
		} else {
			backend.SetOverlayTransformAbsolute(m_overlay[e], p.pose);
		}
		backend.SetOverlayWidthInMetres(m_overlay[e], p.widthMetres);
		backend.SetOverlayAlpha(m_overlay[e], p.alpha > 1.0f ? 1.0f : p.alpha);
		const int error = backend.SetOverlayTexture(m_overlay[e], &data);
		if (error != vr::openvr::kOverlayErrorNone) {
			if (!m_failureReported) {
				m_failureReported = true;
				OBVR_LOG("Hand HUD: SetOverlayTexture failed (%d) for %s", error, vr::kHudElementKeys[e]);
			}
			continue;
		}
		if (!m_overlayVisible[e]) {
			backend.ShowOverlay(m_overlay[e]);
			m_overlayVisible[e] = true;
		}
		++shown;
	}
	m_bracket.Release();

	if (!m_liveReported && shown > 0) {
		m_liveReported = true;
		OBVR_LOG("Hand HUD: live - %u quad(s) on the hands and in the sky", shown);
	}
}

bool HandHudLayer::DumpAtlas(void* gameDevice, const char* path) const {
	return m_surface != nullptr && DumpSurfaceBmp(gameDevice, m_surface, vr::kHandHudAtlasWidth,
	                                              vr::kHandHudAtlasHeight, d3d9::kFormatA8R8G8B8, path);
}

void HandHudLayer::Destroy() {
	m_bracket.Release();
	d3d11::Release(m_interop);
	d3d11::Release(m_surface);
	d3d11::Release(m_texture);
	m_interop = nullptr;
	m_surface = nullptr;
	m_texture = nullptr;
	m_image = BackBufferImage{};
	m_textureTried = false;
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		m_lifted[e] = false;
		// Left to the runtime, as HudLayer::Destroy explains.
		m_overlay[e] = vr::openvr::kOverlayHandleInvalid;
		m_overlayTried[e] = false;
		m_overlayVisible[e] = false;
	}
	m_vulkanChecked = false;
	m_vulkanUsable = false;
}

}  // namespace obvr::render
