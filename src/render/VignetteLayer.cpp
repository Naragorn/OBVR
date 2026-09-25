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

void VignetteLayer::GenerateVignettePixels(UInt32 width, UInt32 height, UInt8* rows,
                                           UInt32 pitch) {
	const float halfW = static_cast<float>(width) * 0.5f;
	const float halfH = static_cast<float>(height) * 0.5f;

	// How far from centre the vignette starts becoming visible (as a fraction of
	// half-width). Below this radius is fully clear, above it darkens towards edges.
	constexpr float kStartRadius = 0.45f;
	// Maximum darkness at the corners (alpha channel, 0-255). Not fully opaque so
	// the world behind still shows through - a vignette, not a blackout.
	constexpr UInt32 kMaxAlpha = 180u;

	for (UInt32 y = 0; y < height; ++y) {
		// Row by row at the surface's own pitch, which may be wider than the
		// picture.
		UInt32* const pixels = reinterpret_cast<UInt32*>(rows + y * pitch);
		for (UInt32 x = 0; x < width; ++x) {
			const float dx = static_cast<float>(x) - halfW;
			const float dy = static_cast<float>(y) - halfH;

			// Normalised distance from centre, where the corners are roughly at 1.0.
			const float dist = math::Sqrt(dx * dx + dy * dy);
			const float maxDist = halfW > halfH ? halfW : halfH;
			const float norm = maxDist > 0.0f ? dist / maxDist : 0.0f;

			if (norm <= kStartRadius) {
				pixels[x] = 0x00000000u;  // fully transparent at centre
			} else {
				// Smooth ramp from clear to dark using a quadratic curve.
				// Held at full beyond the inscribed circle: the corners reach
				// 1.41, and an unclamped square would overflow the alpha byte.
				float t = (norm - kStartRadius) / (1.0f - kStartRadius);
				t = t > 1.0f ? 1.0f : t;
				const float alpha = t * t;
				const UInt32 a = static_cast<UInt32>(alpha * static_cast<float>(kMaxAlpha) + 0.5f);
				pixels[x] = (a << 24u) | 0x00000000u;  // black with alpha
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
		ReportFailure("the device has no CreateTexture");
		return false;
	}

	if (d3d11::Failed(createTexture(gameDevice, kTextureSize, kTextureSize, 1,
	                                d3d9::kUsageRenderTarget, d3d9::kFormatA8R8G8B8,
	                                d3d9::kPoolDefault, &m_texture, nullptr)) ||
	    m_texture == nullptr) {
		m_texture = nullptr;
		ReportFailure("CreateTexture refused the render target");
		return false;
	}

	// Every failure from here on takes the texture away again: the check at
	// the top treats a texture that exists as one that is ready.
	auto getSurfaceLevel =
		d3d9::Method<d3d9::GetSurfaceLevelFn>(m_texture, d3d9::kTextureGetSurfaceLevel);
	if (getSurfaceLevel == nullptr ||
	    d3d11::Failed(getSurfaceLevel(m_texture, 0, &m_surface)) || m_surface == nullptr) {
		m_surface = nullptr;
		ReportFailure("the texture has no surface");
		DestroyTexture();
		return false;
	}

	auto* unknown = static_cast<d3d11::Unknown*>(m_texture);
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr ||
	    d3d11::Failed(unknown->vtbl->QueryInterface(unknown, &kIID_D3D9VkInteropTexture,
	                                                &m_interop)) ||
	    m_interop == nullptr) {
		m_interop = nullptr;
		ReportFailure("DXVK gave no interop texture");
		DestroyTexture();
		return false;
	}

	if (!ReadImageInfo(m_interop, m_image)) {
		ReportFailure("the interop texture could not be described");
		DestroyTexture();
		return false;
	}

	// Fill it with the radial gradient. Not by locking it: a render target in
	// the default pool cannot be locked in Direct3D 9, so the picture is drawn
	// into a system-memory surface and uploaded, the way the crosshair cache
	// does it. Until that has worked the layer counts as having no texture at
	// all - an unfilled one would put an arbitrary picture across the view.
	if (!FillTexture(gameDevice)) {
		ReportFailure("the gradient could not be uploaded");
		DestroyTexture();
		return false;
	}
	return true;
}

bool VignetteLayer::FillTexture(void* gameDevice) {
	auto createPlain = d3d9::Method<d3d9::CreateOffscreenPlainSurfaceFn>(
		gameDevice, d3d9::kDeviceCreateOffscreenPlainSurface);
	auto updateSurface =
		d3d9::Method<d3d9::UpdateSurfaceFn>(gameDevice, d3d9::kDeviceUpdateSurface);
	if (createPlain == nullptr || updateSurface == nullptr) {
		return false;
	}

	void* staging = nullptr;
	if (d3d11::Failed(createPlain(gameDevice, kTextureSize, kTextureSize, d3d9::kFormatA8R8G8B8,
	                              d3d9::kPoolSystemMem, &staging, nullptr)) ||
	    staging == nullptr) {
		return false;
	}

	auto lockRect = d3d9::Method<d3d9::LockRectFn>(staging, d3d9::kSurfaceLockRect);
	auto unlockRect = d3d9::Method<d3d9::UnlockRectFn>(staging, d3d9::kSurfaceUnlockRect);
	d3d9::LockedRect locked{};
	if (lockRect == nullptr || unlockRect == nullptr ||
	    d3d11::Failed(lockRect(staging, &locked, nullptr, 0)) || locked.bits == nullptr ||
	    locked.pitch < static_cast<SInt32>(kTextureSize * 4)) {
		d3d11::Release(staging);
		return false;
	}
	GenerateVignettePixels(kTextureSize, kTextureSize, static_cast<UInt8*>(locked.bits),
	                       static_cast<UInt32>(locked.pitch));
	const bool unlocked = !d3d11::Failed(unlockRect(staging));
	const bool uploaded =
		unlocked && !d3d11::Failed(updateSurface(gameDevice, staging, nullptr, m_surface, nullptr));
	d3d11::Release(staging);
	return uploaded;
}

void VignetteLayer::DestroyTexture() {
	d3d11::Release(m_interop);
	d3d11::Release(m_surface);
	d3d11::Release(m_texture);
	m_interop = nullptr;
	m_surface = nullptr;
	m_texture = nullptr;
	m_image = BackBufferImage{};
}

bool VignetteLayer::EnsureOverlay(vr::OpenVRBackend& backend) {
	if (m_overlay != vr::openvr::kOverlayHandleInvalid) {
		return true;
	}
	if (m_overlayTried) {
		return false;
	}
	m_overlayTried = true;

	if (!backend.CreateOverlay("obvr.vignette", "OBVR Snap Turn Vignette", m_overlay)) {
		ReportFailure("SteamVR refused the overlay");
		return false;
	}
	return true;
}

void VignetteLayer::ReportFailure(const char* why) {
	if (m_failureReported) {
		return;
	}
	m_failureReported = true;
	OBVR_LOG("Vignette: %s - the snap turn vignette stays off", why);
}

void VignetteLayer::Trigger() {
	// Up at once, held, then faded: the first headset run with the vignette
	// on saw nothing of a pulse that was gone again in about 0.12 s. A snap
	// that comes while it is still up starts the hold again.
	m_fadingIn = true;
	m_holdSeconds = kHoldSeconds;
	if (m_alpha < 0.5f) {
		m_alpha = 0.5f;
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
			m_holdSeconds = 0.0f;
		}
		if (m_overlayVisible) {
			backend.HideOverlay(m_overlay);
			m_overlayVisible = false;
		}
		return;
	}

	// Advance the fade state.
	if (m_fadingIn) {
		m_alpha += kFadeInRate * deltaSeconds;
		if (m_alpha >= 1.0f) {
			m_alpha = 1.0f;
			m_fadingIn = false;
		}
	} else if (m_holdSeconds > 0.0f) {
		m_holdSeconds -= deltaSeconds;
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
		ReportFailure("DXVK's Vulkan context could not be read");
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
	// 8 m at 1.5 m spans 2 * atan(4 / 1.5) = 139 degrees, past the field of
	// view of the headsets OBVR is used with (the Index's is quoted around
	// 108 horizontally), so the dark rim never shows its own edge.
	backend.SetOverlayWidthInMetres(m_overlay, 8.0f);

	if (!ReadImageInfo(m_interop, m_image)) {
		return;
	}
	if (!m_bracket.Begin(gameDevice)) {
		ReportFailure("DXVK's queue could not be taken");
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
		ReportFailure("SteamVR refused the overlay texture");
		return;
	}

	// Update alpha - this is what actually controls visibility.
	backend.SetOverlayAlpha(m_overlay, m_alpha);

	if (!m_overlayVisible) {
		backend.ShowOverlay(m_overlay);
		m_overlayVisible = true;
		if (!m_shownReported) {
			m_shownReported = true;
			OBVR_LOG("Vignette: shown for a snap turn");
		}
	}
}

void VignetteLayer::Destroy() {
	m_bracket.Release();

	DestroyTexture();
	m_textureTried = false;

	// Left to the runtime, same reason as CrosshairLayer and HudLayer.
	m_overlay = vr::openvr::kOverlayHandleInvalid;
	m_overlayTried = false;
	m_overlayVisible = false;

	m_alpha = 0.0f;
	m_fadingIn = false;
	m_holdSeconds = 0.0f;
	m_vulkanChecked = false;
	m_vulkanUsable = false;
}

}  // namespace obvr::render
