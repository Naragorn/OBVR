#include "render/CrosshairLayer.h"

#include "core/Log.h"
#include "render/D3D11Types.h"
#include "render/GameFrame.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {
namespace {

// Everything the crosshair is not: the quad is mostly nothing, and the world
// shows through it.
constexpr UInt32 kTransparent = 0x00000000;

// Square, and larger than the lifted square so nothing is thrown away on the
// way in: the compositor scales the quad down far more gracefully than up.
constexpr UInt32 kTextureSize = 256;

}  // namespace

bool CrosshairLayer::EnsureTexture(void* gameDevice) {
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

	// A render target, and CreateTexture rather than CreateRenderTarget, for
	// the same two reasons the HUD layer has: ColorFill needs a target, and
	// only a texture carries the sampled bit the compositor requires.
	if (d3d11::Failed(createTexture(gameDevice, kTextureSize, kTextureSize, 1,
	                                d3d9::kUsageRenderTarget, d3d9::kFormatA8R8G8B8,
	                                d3d9::kPoolDefault, &m_texture, nullptr)) ||
	    m_texture == nullptr) {
		m_texture = nullptr;
		OBVR_LOG("Crosshair: no %ux%u A8R8G8B8 target, so the crosshair stays in the flat "
		         "HUD where it was",
		         kTextureSize, kTextureSize);
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

	return ReadImageInfo(m_interop, m_image);
}

bool CrosshairLayer::TakeFromHud(void* gameDevice, void* hudSurface, UInt32 hudWidth,
                                 UInt32 hudHeight, UInt32 believedWidth, UInt32 believedHeight,
                                 UInt32 sizePixels) {
	if (hudSurface == nullptr || sizePixels == 0 || !EnsureTexture(gameDevice)) {
		return false;
	}

	// The centre of the rectangle the game believes it drew in, not the centre
	// of the texture. The layer texture is the back buffer's size while the UI
	// lays out inside the believed size in its corner (UiScreenSize), so the
	// middle of the texture is not the middle of the picture anyone is looking
	// at - and taking the wrong square would lift a piece of empty layer and
	// leave the crosshair where it was.
	const UInt32 width = believedWidth > 0 ? believedWidth : hudWidth;
	const UInt32 height = believedHeight > 0 ? believedHeight : hudHeight;
	const SInt32 half = static_cast<SInt32>(sizePixels) / 2;
	const SInt32 cx = static_cast<SInt32>(width) / 2;
	const SInt32 cy = static_cast<SInt32>(height) / 2;

	const d3d9::Rect source{cx - half, cy - half, cx + half, cy + half};
	if (source.left < 0 || source.top < 0 || static_cast<UInt32>(source.right) > hudWidth ||
	    static_cast<UInt32>(source.bottom) > hudHeight) {
		return false;
	}

	auto stretchRect = d3d9::Method<d3d9::StretchRectFn>(gameDevice, d3d9::kDeviceStretchRect);
	auto colorFill = d3d9::Method<d3d9::ColorFillFn>(gameDevice, d3d9::kDeviceColorFill);
	if (stretchRect == nullptr || colorFill == nullptr) {
		return false;
	}

	// Cleared first: the square is stretched across this whole texture, but a
	// failure half way would otherwise leave the previous frame showing through
	// whatever did arrive.
	if (d3d11::Failed(colorFill(gameDevice, m_surface, nullptr, kTransparent)) ||
	    d3d11::Failed(stretchRect(gameDevice, hudSurface, &source, m_surface, nullptr,
	                              d3d9::kTexFilterLinear))) {
		return false;
	}

	// And erased where it came from. Without this the flat copy stays in the
	// HUD quad at the HUD's own distance, and there are two crosshairs again -
	// which is the entire thing this layer exists to stop.
	if (d3d11::Failed(colorFill(gameDevice, hudSurface, &source, kTransparent))) {
		return false;
	}

	m_takenFromHud = true;

	if (!m_takeReported) {
		m_takeReported = true;
		OBVR_LOG("Crosshair: taking the game's own from the 2D layer - %d,%d..%d,%d of the "
		         "%ux%u it believes it drew in",
		         source.left, source.top, source.right, source.bottom, width, height);
	}
	return true;
}

bool CrosshairLayer::EnsureKeptTexture(void* gameDevice) {
	if (m_kept != nullptr) {
		return true;
	}
	if (m_keptTried || gameDevice == nullptr) {
		return false;
	}
	m_keptTried = true;

	auto createTexture =
		d3d9::Method<d3d9::CreateTextureFn>(gameDevice, d3d9::kDeviceCreateTexture);
	if (createTexture == nullptr) {
		return false;
	}

	// A render target like the one it copies from and to, because StretchRect
	// wants both ends to be one. No interop here: this texture never reaches
	// the compositor, it only holds a picture until third person asks for it.
	if (d3d11::Failed(createTexture(gameDevice, kTextureSize, kTextureSize, 1,
	                                d3d9::kUsageRenderTarget, d3d9::kFormatA8R8G8B8,
	                                d3d9::kPoolDefault, &m_kept, nullptr)) ||
	    m_kept == nullptr) {
		m_kept = nullptr;
		OBVR_LOG("Crosshair: no second %ux%u target, so third person cannot borrow the "
		         "game's crosshair",
		         kTextureSize, kTextureSize);
		return false;
	}

	auto getSurfaceLevel =
		d3d9::Method<d3d9::GetSurfaceLevelFn>(m_kept, d3d9::kTextureGetSurfaceLevel);
	if (getSurfaceLevel == nullptr || d3d11::Failed(getSurfaceLevel(m_kept, 0, &m_keptSurface)) ||
	    m_keptSurface == nullptr) {
		m_keptSurface = nullptr;
		return false;
	}
	return true;
}

bool CrosshairLayer::RememberCrosshair(void* gameDevice) {
	if (m_surface == nullptr || !EnsureKeptTexture(gameDevice)) {
		return false;
	}

	auto stretchRect = d3d9::Method<d3d9::StretchRectFn>(gameDevice, d3d9::kDeviceStretchRect);
	if (stretchRect == nullptr) {
		return false;
	}

	if (d3d11::Failed(stretchRect(gameDevice, m_surface, nullptr, m_keptSurface, nullptr,
	                              d3d9::kTexFilterNone))) {
		return false;
	}

	m_haveKept = true;
	if (!m_keptReported) {
		m_keptReported = true;
		OBVR_LOG("Crosshair: keeping the game's own crosshair for third person to borrow");
	}
	return true;
}

bool CrosshairLayer::UseRememberedCrosshair(void* gameDevice) {
	if (!m_haveKept || m_keptSurface == nullptr || !EnsureTexture(gameDevice)) {
		return false;
	}

	auto stretchRect = d3d9::Method<d3d9::StretchRectFn>(gameDevice, d3d9::kDeviceStretchRect);
	if (stretchRect == nullptr) {
		return false;
	}

	// Straight over whatever the lift left there. In third person the lifted
	// square is empty - Oblivion draws nothing in the middle of the layer -
	// so there is nothing being thrown away.
	if (d3d11::Failed(stretchRect(gameDevice, m_keptSurface, nullptr, m_surface, nullptr,
	                              d3d9::kTexFilterNone))) {
		return false;
	}

	m_takenFromHud = true;
	return true;
}

bool CrosshairLayer::EnsureOverlay(vr::OpenVRBackend& backend) {
	if (m_overlay != vr::openvr::kOverlayHandleInvalid) {
		return true;
	}
	if (m_overlayTried) {
		return false;
	}
	m_overlayTried = true;

	return backend.CreateOverlay("obvr.crosshair", "Oblivion Crosshair", m_overlay);
}

void CrosshairLayer::Place(vr::OpenVRBackend& backend, float distanceMetres,
                           float widthMetres) {
	// Nothing moved, so nothing is said. Worth the comparison because this
	// runs every frame and both calls cross into the compositor.
	if (m_placed && m_placedDistance == distanceMetres && m_placedWidth == widthMetres) {
		return;
	}
	m_placed = true;
	m_placedDistance = distanceMetres;
	m_placedWidth = widthMetres;

	// Straight ahead of the head, negative Z being forward in the headset's
	// own frame - the same placement the HUD layer uses, and for the same
	// reason: the crosshair belongs where the wearer is looking, not where
	// they were looking.
	vr::openvr::HmdMatrix34 hmdToOverlay{};
	hmdToOverlay.m[0][0] = 1.0f;
	hmdToOverlay.m[1][1] = 1.0f;
	hmdToOverlay.m[2][2] = 1.0f;
	hmdToOverlay.m[2][3] = -distanceMetres;
	backend.SetOverlayTransformHmdRelative(m_overlay, hmdToOverlay);

	// The width already grew with the distance, back where it was decided.
	backend.SetOverlayWidthInMetres(m_overlay, widthMetres);
}

void CrosshairLayer::Submit(vr::OpenVRBackend& backend, void* gameDevice, bool visible,
                            float distanceMetres, float widthMetres) {
	// Consumed either way: the next frame lifts it again or it is not shown.
	const bool taken = m_takenFromHud;
	m_takenFromHud = false;

	// Nothing lifted means nothing to show. There is no cross of OBVR's own
	// to fall back to, deliberately: a hand-drawn one that is nearly the
	// game's is worse than none, because it looks like the game got it wrong
	// rather than like the mod is off.
	if (!visible || !taken) {
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

	Place(backend, distanceMetres, widthMetres);

	// The same bracket the eyes and the HUD go through. The texture never
	// changes after the first frame, so handing it over once would in
	// principle be enough and would save a flush per frame - but whether the
	// compositor keeps holding a Vulkan image it was shown once is not
	// something this code knows, and the erring side of that question is the
	// one where the crosshair is still there.
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
		if (!m_failureReported) {
			m_failureReported = true;
			OBVR_LOG("Crosshair: SetOverlayTexture failed (%d), so the crosshair stays "
			         "hidden",
			         error);
		}
		return;
	}

	if (!m_overlayVisible) {
		backend.ShowOverlay(m_overlay);
		m_overlayVisible = true;
	}

	if (!m_liveReported) {
		m_liveReported = true;
		OBVR_LOG("Crosshair: live - %.2f m ahead, %.3f m wide there",
		         static_cast<double>(distanceMetres), static_cast<double>(widthMetres));
	}
}

void CrosshairLayer::Destroy() {
	m_bracket.Release();

	d3d11::Release(m_interop);
	d3d11::Release(m_surface);
	d3d11::Release(m_texture);
	m_interop = nullptr;
	m_surface = nullptr;
	m_texture = nullptr;
	m_image = BackBufferImage{};
	m_textureTried = false;
	m_takenFromHud = false;
	m_takeReported = false;

	d3d11::Release(m_keptSurface);
	d3d11::Release(m_kept);
	m_keptSurface = nullptr;
	m_kept = nullptr;
	m_keptTried = false;
	m_haveKept = false;
	m_keptReported = false;

	// Left to the runtime, for the reason HudLayer::Destroy records: calling
	// DestroyOverlay from a shutdown next to DllMain would reach into a
	// library that may be tearing down, and SteamVR reclaims a process's
	// overlays when it exits anyway.
	m_overlay = vr::openvr::kOverlayHandleInvalid;
	m_overlayTried = false;
	m_overlayVisible = false;
	m_placed = false;
	m_vulkanChecked = false;
	m_vulkanUsable = false;
}

}  // namespace obvr::render
