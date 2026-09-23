#include "render/VignetteLayer.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "render/D3D11Types.h"
#include "render/GameFrame.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {
namespace {

// Texture size: square, large enough that the compositor scales down cleanly.
constexpr UInt32 kTextureSize = 512;

}  // namespace

void VignetteLayer::GenerateVignettePixels(UInt32 width, UInt32 height, UInt32* pixels) {
	const float halfW = static_cast<float>(width) * 0.5f;
	const float halfH = static_cast<float>(height) * 0.5f;

	// How far from centre the vignette starts becoming visible (as a fraction of
	// half-width). Below this radius is fully clear, above it darkens towards edges.
	constexpr float kStartRadius = 0.45f;
	// Maximum darkness at the corners (alpha channel, 0-255). Not fully opaque so
	// the world behind still shows through - a vignette, not a blackout.
	constexpr UInt32 kMaxAlpha = 180u;

	for (UInt32 y = 0; y < height; ++y) {
		for (UInt32 x = 0; x < width; ++x) {
			const float dx = static_cast<float>(x) - halfW;
			const float dy = static_cast<float>(y) - halfH;

			// Normalised distance from centre, where the corners are roughly at 1.0.
			const float dist = math::Sqrt(dx * dx + dy * dy);
			const float maxDist = halfW > halfH ? halfW : halfH;
			const float norm = maxDist > 0.0f ? dist / maxDist : 0.0f;

			if (norm <= kStartRadius) {
				pixels[y * width + x] = 0x00000000u;  // fully transparent at centre
			} else {
				// Smooth ramp from clear to dark using a quadratic curve.
				const float t = (norm - kStartRadius) / (1.0f - kStartRadius);
				const float alpha = t * t;
				const UInt32 a = static_cast<UInt32>(alpha * static_cast<float>(kMaxAlpha) + 0.5f);
				pixels[y * width + x] = (a << 24u) | 0x00000000u;  // black with alpha
			}
		}
	}
}

bool VignetteLayer::EnsureTexture(void* gameDevice) {
	if (m_texture != nullptr) {
		return true;
	}
	if (m_textureTried || gameDevice == nullptr) {
		return false;
	}
	m_textureTried = true;

	auto createTexture =
		d3d9::Method<d3d9::CreateTextureFn>(gameDevice, d3d9::kDeviceCreateTexture);
	if (createTexture == nullptr) {
		return false;
	}

	if (d3d11::Failed(createTexture(gameDevice, kTextureSize, kTextureSize, 1,
	                                d3d9::kUsageRenderTarget, d3d9::kFormatA8R8G8B8,
	                                d3d9::kPoolDefault, &m_texture, nullptr)) ||
	    m_texture == nullptr) {
		m_texture = nullptr;
		return false;
	}

	auto getSurfaceLevel =
		d3d9::Method<d3d9::GetSurfaceLevelFn>(m_texture, d3d9::kTextureGetSurfaceLevel);
	if (getSurfaceLevel == nullptr ||
	    d3d11::Failed(getSurfaceLevel(m_texture, 0, &m_surface)) || m_surface == nullptr) {
		m_surface = nullptr;
		return false;
	}

	auto* unknown = static_cast<d3d11::Unknown*>(m_texture);
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr ||
	    d3d11::Failed(unknown->vtbl->QueryInterface(unknown, &kIID_D3D9VkInteropTexture,
	                                                &m_interop)) ||
	    m_interop == nullptr) {
		m_interop = nullptr;
		return false;
	}

	if (!ReadImageInfo(m_interop, m_image)) {
		return false;
	}

	// Fill the texture with the radial gradient. Lock, write, unlock.
	auto lockRect = d3d9::Method<d3d9::LockRectFn>(m_surface, d3d9::kSurfaceLockRect);
	auto unlockRect = d3d9::Method<d3d9::UnlockRectFn>(m_surface, d3d9::kSurfaceUnlockRect);

	if (lockRect == nullptr || unlockRect == nullptr) {
		return false;
	}

	d3d9::LockedRect locked{};
	if (d3d11::Failed(lockRect(m_surface, &locked, nullptr, 0))) {
		return false;
	}

	UInt32* pixels = static_cast<UInt32*>(locked.bits);
	if (pixels != nullptr) {
		GenerateVignettePixels(kTextureSize, kTextureSize, pixels);
	}

	unlockRect(m_surface);

	return true;
}

bool VignetteLayer::EnsureOverlay(vr::OpenVRBackend& backend) {
	if (m_overlay != vr::openvr::kOverlayHandleInvalid) {
		return true;
	}
	if (m_overlayTried) {
		return false;
	}
	m_overlayTried = true;

	return backend.CreateOverlay("obvr.vignette", "OBVR Snap Turn Vignette", m_overlay);
}

void VignetteLayer::Trigger() {
	// Start fading in from wherever we currently are. If already visible, this just
	// keeps it going - a rapid double-snap does not reset the effect to zero.
	m_fadingIn = true;
	if (m_alpha < 0.5f) {
		m_alpha = 0.1f;  // small head start so it appears immediately
	}
}

void VignetteLayer::Update(vr::OpenVRBackend& backend, void* gameDevice, bool visible,
                           float deltaSeconds) {
	if (deltaSeconds <= 0.0f || deltaSeconds > 0.1f) {
		return;
	}

	// Not allowed: kill any in-progress fade and hide the overlay.
	if (!visible) {
		if (m_alpha > 0.0f) {
			m_alpha = 0.0f;
			m_fadingIn = false;
		}
		if (m_overlayVisible) {
			backend.HideOverlay(m_overlay);
			m_overlayVisible = false;
		}
		return;
	}

	// Advance the fade state.
	const float oldAlpha = m_alpha;
	if (m_fadingIn) {
		m_alpha += kFadeInRate * deltaSeconds;
		if (m_alpha >= 1.0f) {
			m_alpha = 1.0f;
			m_fadingIn = false;
		}
	} else {
		m_alpha -= kFadeOutRate * deltaSeconds;
		if (m_alpha <= 0.0f) {
			m_alpha = 0.0f;
		}
	}

	// Nothing visible: hide the overlay and return early.
	if (m_alpha < 0.01f) {
		if (m_overlayVisible) {
			backend.HideOverlay(m_overlay);
			m_overlayVisible = false;
		}
		return;
	}

	if (!EnsureTexture(gameDevice) || !EnsureOverlay(backend)) {
		return;
	}

	if (!m_vulkanChecked) {
		m_vulkanChecked = true;
		m_vulkanUsable = GetVulkanContext(gameDevice, m_vulkan);
	}
	if (!m_vulkanUsable) {
		return;
	}

	// Place the overlay flat ahead of the head, large enough to cover both eyes.
	// Same placement pattern as the HUD layer - straight ahead at a fixed distance.
	vr::openvr::HmdMatrix34 toOverlay{};
	toOverlay.m[0][0] = 1.0f;
	toOverlay.m[1][1] = 1.0f;
	toOverlay.m[2][2] = 1.0f;
	toOverlay.m[2][3] = -1.5f;  // 1.5m ahead, close enough to fill the view

	backend.SetOverlayTransformHmdRelative(m_overlay, toOverlay);
	backend.SetOverlayWidthInMetres(m_overlay, 4.0f);  // wide enough to cover both eyes

	if (!ReadImageInfo(m_interop, m_image)) {
		return;
	}
	if (!m_bracket.Begin(gameDevice)) {
		return;
	}
	if (!m_bracket.ToTransferSrc(m_interop, m_image.layout)) {
		m_bracket.Release();
		return;
	}

	dxvk::VRVulkanTextureData data{};
	DescribeForOpenVR(m_image, m_vulkan, data);
	const int error = backend.SetOverlayTexture(m_overlay, &data);

	m_bracket.Release();

	if (error != vr::openvr::kOverlayErrorNone) {
		return;
	}

	// Update alpha - this is what actually controls visibility.
	backend.SetOverlayAlpha(m_overlay, m_alpha);

	if (!m_overlayVisible) {
		backend.ShowOverlay(m_overlay);
		m_overlayVisible = true;
	}
}

void VignetteLayer::Destroy() {
	m_bracket.Release();

	d3d11::Release(m_interop);
	d3d11::Release(m_surface);
	d3d11::Release(m_texture);
	m_interop = nullptr;
	m_surface = nullptr;
	m_texture = nullptr;
	m_image = BackBufferImage{};
	m_textureTried = false;

	// Left to the runtime, same reason as CrosshairLayer and HudLayer.
	m_overlay = vr::openvr::kOverlayHandleInvalid;
	m_overlayTried = false;
	m_overlayVisible = false;

	m_alpha = 0.0f;
	m_fadingIn = false;
	m_vulkanChecked = false;
	m_vulkanUsable = false;
}

}  // namespace obvr::render
