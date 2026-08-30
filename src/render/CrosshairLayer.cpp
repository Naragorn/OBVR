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

// The bars, and the darker edge around them. The outline is not decoration -
// a white crosshair against snow or a bright sky is invisible exactly when
// aiming matters, and a dark rim is what every game's reticle uses to stay
// readable on both. Partly transparent rather than solid black, so it reads as
// an edge instead of as a second, thicker crosshair.
constexpr UInt32 kInk = 0xFFFFFFFF;
constexpr UInt32 kOutline = 0xC0000000;

// Square, and larger than it needs to be on screen: the compositor scales the
// quad down far more gracefully than up, and a crosshair with soft edges reads
// as a smudge rather than as an aiming point.
constexpr UInt32 kTextureSize = 256;

// The shape, in texture pixels from the centre. Four bars with a hole in the
// middle, which is the vanilla shape and the useful one: a solid cross hides
// the very thing being aimed at.
constexpr SInt32 kCentre = static_cast<SInt32>(kTextureSize) / 2;
constexpr SInt32 kInkHalf = 3;
constexpr SInt32 kInkGap = 14;
constexpr SInt32 kInkReach = 46;
constexpr SInt32 kEdge = 2;  // how far the outline stands out past the ink

// Paints the four bars in one colour. Called twice - the outline first, the
// ink over it - which is what makes the rim without any per-pixel work.
bool FillCross(void* gameDevice, void* surface, d3d9::ColorFillFn colorFill, SInt32 half,
               SInt32 gap, SInt32 reach, UInt32 colour) {
	const d3d9::Rect bars[4] = {
		{kCentre - half, kCentre - reach, kCentre + half, kCentre - gap},  // up
		{kCentre - half, kCentre + gap, kCentre + half, kCentre + reach},  // down
		{kCentre - reach, kCentre - half, kCentre - gap, kCentre + half},  // left
		{kCentre + gap, kCentre - half, kCentre + reach, kCentre + half},  // right
	};

	for (const d3d9::Rect& bar : bars) {
		if (d3d11::Failed(colorFill(gameDevice, surface, &bar, colour))) {
			return false;
		}
	}
	return true;
}

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

bool CrosshairLayer::DrawCrosshair(void* gameDevice) {
	if (m_drawn) {
		return true;
	}

	auto colorFill = d3d9::Method<d3d9::ColorFillFn>(gameDevice, d3d9::kDeviceColorFill);
	if (colorFill == nullptr) {
		return false;
	}

	// Clear to nothing first: everything the cross does not cover has to show
	// the world, and a render target starts as whatever memory it was handed.
	if (d3d11::Failed(colorFill(gameDevice, m_surface, nullptr, kTransparent))) {
		return false;
	}

	// Outline, then ink over it. The outline reaches one step further in every
	// direction, which is what leaves a rim rather than a fringe on two sides.
	if (!FillCross(gameDevice, m_surface, colorFill, kInkHalf + kEdge, kInkGap - kEdge,
	               kInkReach + kEdge, kOutline)) {
		return false;
	}
	if (!FillCross(gameDevice, m_surface, colorFill, kInkHalf, kInkGap, kInkReach, kInk)) {
		return false;
	}

	m_drawn = true;
	OBVR_LOG("Crosshair: %ux%u texture drawn - four bars around a %u pixel gap", kTextureSize,
	         kTextureSize, static_cast<UInt32>(kInkGap * 2));
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
	if (!visible) {
		if (m_overlayVisible) {
			backend.HideOverlay(m_overlay);
			m_overlayVisible = false;
		}
		return;
	}

	if (!EnsureTexture(gameDevice) || !DrawCrosshair(gameDevice) || !EnsureOverlay(backend)) {
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
	m_drawn = false;

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
