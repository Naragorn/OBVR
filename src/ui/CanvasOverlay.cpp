#include "ui/CanvasOverlay.h"

#include <cstdio>
#include <cstring>

#include "core/Log.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"
#include "render/GameFrame.h"

namespace obvr::ui {

namespace d3d9 = render::d3d9;
namespace d3d11 = render::d3d11;
namespace dxvk = render::dxvk;

namespace {

void ReleaseCom(void*& object) {
	if (object != nullptr) {
		auto* unknown = static_cast<d3d11::Unknown*>(object);
		if (unknown->vtbl != nullptr && unknown->vtbl->Release != nullptr) {
			unknown->vtbl->Release(unknown);
		}
		object = nullptr;
	}
}

void PutU32(UInt8* at, UInt32 value) {
	at[0] = static_cast<UInt8>(value);
	at[1] = static_cast<UInt8>(value >> 8);
	at[2] = static_cast<UInt8>(value >> 16);
	at[3] = static_cast<UInt8>(value >> 24);
}

}  // namespace

// The same texture the settings menu draws on (SettingsMenuLayer::
// EnsureTexture has the reasons): dynamic, default pool, A8R8G8B8, one level,
// and a DXVK interop texture so the overlay can be handed its Vulkan image.
bool CanvasOverlay::EnsureTexture(void* gameDevice) {
	if (m_texture != nullptr) {
		return true;
	}
	if (m_textureTried || gameDevice == nullptr) {
		return false;
	}
	m_textureTried = true;
	auto createTexture = d3d9::Method<d3d9::CreateTextureFn>(gameDevice, d3d9::kDeviceCreateTexture);
	if (createTexture == nullptr ||
	    d3d11::Failed(createTexture(gameDevice, m_width, m_height, 1, d3d9::kUsageDynamic,
	                                d3d9::kFormatA8R8G8B8, d3d9::kPoolDefault, &m_texture,
	                                nullptr)) ||
	    m_texture == nullptr) {
		m_texture = nullptr;
		OBVR_LOG("%s: no %ux%u dynamic texture, so it cannot be drawn", m_name, m_width, m_height);
		return false;
	}
	auto getSurfaceLevel =
		d3d9::Method<d3d9::GetSurfaceLevelFn>(m_texture, d3d9::kTextureGetSurfaceLevel);
	if (getSurfaceLevel == nullptr || d3d11::Failed(getSurfaceLevel(m_texture, 0, &m_surface)) ||
	    m_surface == nullptr) {
		m_surface = nullptr;
		OBVR_LOG("%s: the texture has no surface to lock", m_name);
		return false;
	}
	auto* unknown = static_cast<d3d11::Unknown*>(m_texture);
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr ||
	    d3d11::Failed(unknown->vtbl->QueryInterface(unknown, &render::kIID_D3D9VkInteropTexture,
	                                                &m_interop)) ||
	    m_interop == nullptr) {
		m_interop = nullptr;
		OBVR_LOG("%s: the texture is not a DXVK interop texture", m_name);
		return false;
	}
	return true;
}

// The painted picture into the texture, row by row: the locked pitch is the
// driver's (see Canvas).
bool CanvasOverlay::Upload() {
	auto lockRect = d3d9::Method<d3d9::LockRectFn>(m_surface, d3d9::kSurfaceLockRect);
	auto unlockRect = d3d9::Method<d3d9::UnlockRectFn>(m_surface, d3d9::kSurfaceUnlockRect);
	if (lockRect == nullptr || unlockRect == nullptr) {
		return false;
	}
	d3d9::LockedRect locked{};
	if (d3d11::Failed(lockRect(m_surface, &locked, nullptr, 0)) || locked.bits == nullptr) {
		return false;
	}
	const UInt32 rowBytes = m_width * static_cast<UInt32>(sizeof(render::Pixel));
	for (UInt32 y = 0; y < m_height; ++y) {
		std::memcpy(static_cast<UInt8*>(locked.bits) + static_cast<SInt32>(y) * locked.pitch,
		            &m_pixels[y * m_width], rowBytes);
	}
	unlockRect(m_surface);
	return true;
}

bool CanvasOverlay::Show(vr::OpenVRBackend& backend, void* gameDevice,
                         const vr::openvr::HmdMatrix34& pose, float widthMetres, UInt32 revision,
                         PaintFn paint, const void* context) {
	if (!EnsureTexture(gameDevice)) {
		return false;
	}
	if (m_overlay == vr::openvr::kOverlayHandleInvalid) {
		if (m_overlayTried) {
			return false;
		}
		m_overlayTried = true;
		if (!backend.CreateOverlay(m_key, m_name, m_overlay)) {
			return false;
		}
	}
	if (!m_vulkanChecked) {
		m_vulkanChecked = true;
		m_vulkanUsable = render::GetVulkanContext(gameDevice, m_vulkan);
	}
	if (!m_vulkanUsable) {
		return false;
	}
	if (!m_painted || m_paintedRevision != revision) {
		m_pixels.assign(static_cast<size_t>(m_width) * m_height, render::Pixel{0, 0, 0, 0});
		Canvas canvas(m_pixels.data(), m_width, m_height);
		paint(canvas, context);
		if (!Upload()) {
			return false;
		}
		m_painted = true;
		m_paintedRevision = revision;
	}
	if (m_placedWidth != widthMetres) {
		backend.SetOverlayWidthInMetres(m_overlay, widthMetres);
		m_placedWidth = widthMetres;
	}
	backend.SetOverlayTransformAbsolute(m_overlay, pose);

	if (!render::ReadImageInfo(m_interop, m_image) ||
	    !m_bracket.Begin(gameDevice, render::BracketOwner::Canvas)) {
		return false;
	}
	if (!m_bracket.ToTransferSrc(m_interop, m_image.layout)) {
		m_bracket.Release();
		return false;
	}
	dxvk::VRVulkanTextureData data{};
	render::DescribeForOpenVR(m_image, m_vulkan, data);
	const int error = backend.SetOverlayTexture(m_overlay, &data);
	m_bracket.Release();
	if (error != vr::openvr::kOverlayErrorNone) {
		if (!m_failureReported) {
			m_failureReported = true;
			OBVR_LOG("%s: SetOverlayTexture failed (%d), so it stays hidden", m_name, error);
		}
		return false;
	}
	if (!m_visible) {
		backend.ShowOverlay(m_overlay);
		m_visible = true;
	}
	return true;
}

void CanvasOverlay::Hide(vr::OpenVRBackend& backend) {
	if (m_visible) {
		backend.HideOverlay(m_overlay);
		m_visible = false;
	}
}

bool CanvasOverlay::SaveBmp(const char* path) const {
	if (!m_painted || m_pixels.empty()) {
		return false;
	}
	FILE* file = nullptr;
	if (fopen_s(&file, path, "wb") != 0 || file == nullptr) {
		return false;
	}
	const UInt32 imageBytes = m_width * m_height * 4;
	UInt8 header[54] = {};
	header[0] = 'B';
	header[1] = 'M';
	PutU32(header + 2, 54 + imageBytes);
	PutU32(header + 10, 54);
	PutU32(header + 14, 40);
	PutU32(header + 18, m_width);
	PutU32(header + 22, m_height);
	header[26] = 1;
	header[28] = 32;
	PutU32(header + 34, imageBytes);
	bool ok = std::fwrite(header, 1, sizeof(header), file) == sizeof(header);
	// Bottom-up rows; the bytes as the texture keeps them are BMP's own
	// order (blue, green, red, alpha).
	for (UInt32 row = 0; ok && row < m_height; ++row) {
		const render::Pixel* line = &m_pixels[(m_height - 1 - row) * m_width];
		ok = std::fwrite(line, sizeof(render::Pixel), m_width, file) == m_width;
	}
	std::fclose(file);
	return ok;
}

void CanvasOverlay::Destroy() {
	m_bracket.Release();
	ReleaseCom(m_interop);
	ReleaseCom(m_surface);
	ReleaseCom(m_texture);
	m_overlay = vr::openvr::kOverlayHandleInvalid;
	m_visible = false;
	m_painted = false;
}

}  // namespace obvr::ui
