#include "render/HudLayer.h"

#include "core/Log.h"
#include "render/D3D11Types.h"
#include "render/GameFrame.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {
namespace {

// Transparent black: nothing at all, until the interface draws something.
// The eye margin uses opaque black; here the transparency is the point, since
// everything the interface did not draw should show the world behind it.
constexpr UInt32 kTransparentBlack = 0x00000000;

// The five render states BeginCapture changes, in one place so the save and
// the restore cannot disagree about which they are.
constexpr UInt32 kAlphaStates[5] = {
	d3d9::kRenderStateSeparateAlphaBlendEnable,
	d3d9::kRenderStateSrcBlendAlpha,
	d3d9::kRenderStateDestBlendAlpha,
	d3d9::kRenderStateBlendOpAlpha,
	d3d9::kRenderStateColorWriteEnable,
};

// What they are set to: alpha accumulates as coverage, one layer over the
// next. Without the separate function the alpha side inherits the colour
// side's SRCALPHA/INVSRCALPHA and the target's alpha comes out as alpha
// squared - close, monotone, and slightly too transparent everywhere.
//
// The write mask is the one that turned out to matter: a game whose back
// buffer has no alpha channel may run with alpha writes off, and then
// nothing lands in the channel the overlay renders by. Set here for the
// state as the pass begins; the SetRenderState hook keeps the bit on
// whatever the pass sets mid-way.
constexpr UInt32 kAlphaValues[5] = {
	1,                        // separate alpha blending on
	d3d9::kBlendOne,          // source contributes its full alpha
	d3d9::kBlendInvSrcAlpha,  // what remains of the destination
	d3d9::kBlendOpAdd,
	d3d9::kColorWriteAll,
};

// The back buffer's size, which is the layout space the interface draws in.
bool DescribeBackBuffer(void* gameDevice, d3d9::SurfaceDesc& out) {
	auto getBackBuffer =
		d3d9::Method<d3d9::GetBackBufferFn>(gameDevice, d3d9::kDeviceGetBackBuffer);
	if (getBackBuffer == nullptr) {
		return false;
	}

	void* surface = nullptr;
	if (d3d11::Failed(getBackBuffer(gameDevice, 0, 0, d3d9::kBackBufferTypeMono, &surface)) ||
	    surface == nullptr) {
		return false;
	}

	auto getDesc = d3d9::Method<d3d9::GetDescFn>(surface, d3d9::kSurfaceGetDesc);
	const bool ok = getDesc != nullptr && !d3d11::Failed(getDesc(surface, &out));

	d3d11::Release(surface);
	return ok;
}

}  // namespace

bool HudLayer::EnsureTexture(void* gameDevice) {
	if (m_texture != nullptr) {
		return true;
	}
	if (m_textureTried || gameDevice == nullptr) {
		return false;
	}
	m_textureTried = true;

	d3d9::SurfaceDesc backBuffer{};
	if (!DescribeBackBuffer(gameDevice, backBuffer)) {
		return false;
	}

	auto createTexture =
		d3d9::Method<d3d9::CreateTextureFn>(gameDevice, d3d9::kDeviceCreateTexture);
	if (createTexture == nullptr) {
		return false;
	}

	// The back buffer's size, but not its format: the interface needs the
	// alpha channel the screen never had. CreateTexture rather than
	// CreateRenderTarget for the reason recorded at kDeviceCreateTexture -
	// only the former carries the sampled bit the compositor requires.
	if (d3d11::Failed(createTexture(gameDevice, backBuffer.width, backBuffer.height, 1,
	                                d3d9::kUsageRenderTarget, d3d9::kFormatA8R8G8B8,
	                                d3d9::kPoolDefault, &m_texture, nullptr)) ||
	    m_texture == nullptr) {
		m_texture = nullptr;
		OBVR_LOG("Hud: no A8R8G8B8 target at %ux%u, so the 2D layer stays in the frame",
		         backBuffer.width, backBuffer.height);
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

	m_width = backBuffer.width;
	m_height = backBuffer.height;
	OBVR_LOG("Hud: %ux%u A8R8G8B8 texture ready, usage=%08X - the 2D layer has somewhere "
	         "of its own to go",
	         m_width, m_height, m_image.usage);
	return true;
}

void* HudLayer::BeginCapture(void* gameDevice) {
	m_captured = false;

	if (!EnsureTexture(gameDevice)) {
		return nullptr;
	}

	// Transparent, then drawn on. Without the clear, last frame's HUD shows
	// through wherever this frame draws nothing - which is most of it.
	auto colorFill = d3d9::Method<d3d9::ColorFillFn>(gameDevice, d3d9::kDeviceColorFill);
	if (colorFill == nullptr ||
	    d3d11::Failed(colorFill(gameDevice, m_surface, nullptr, kTransparentBlack))) {
		return nullptr;
	}

	// Save, then set. The restore in EndCapture runs whether or not the pass
	// drew anything, and it restores what was read here rather than assumed
	// defaults - the game's own state is the game's business.
	auto getState =
		d3d9::Method<d3d9::GetRenderStateFn>(gameDevice, d3d9::kDeviceGetRenderState);
	auto setState =
		d3d9::Method<d3d9::SetRenderStateFn>(gameDevice, d3d9::kDeviceSetRenderState);
	if (getState == nullptr || setState == nullptr) {
		return nullptr;
	}

	for (int i = 0; i < 5; ++i) {
		getState(gameDevice, kAlphaStates[i], &m_savedStates[i]);
	}
	for (int i = 0; i < 5; ++i) {
		setState(gameDevice, kAlphaStates[i], kAlphaValues[i]);
	}
	m_statesSaved = true;

	// What the game was running with, once, because it is evidence: a write
	// mask without the 0x8 bit is a HUD whose alpha never left zero, and this
	// line is what says so without another run.
	if (!m_statesReported) {
		m_statesReported = true;
		OBVR_LOG("Hud: states before the pass: separate=%u srcA=%u dstA=%u op=%u "
		         "writeMask=0x%X",
		         m_savedStates[0], m_savedStates[1], m_savedStates[2], m_savedStates[3],
		         m_savedStates[4]);
	}

	m_captured = true;
	return m_surface;
}

void HudLayer::EndCapture() {
	if (!m_statesSaved) {
		return;
	}
	m_statesSaved = false;

	void* gameDevice = GetGameDevice();
	auto setState =
		d3d9::Method<d3d9::SetRenderStateFn>(gameDevice, d3d9::kDeviceSetRenderState);
	if (setState == nullptr) {
		return;
	}
	for (int i = 0; i < 5; ++i) {
		setState(gameDevice, kAlphaStates[i], m_savedStates[i]);
	}
}

bool HudLayer::EnsureOverlay(vr::OpenVRBackend& backend, float distanceMetres,
                             float widthMetres) {
	if (m_overlay != vr::openvr::kOverlayHandleInvalid) {
		return true;
	}
	if (m_overlayTried) {
		return false;
	}
	m_overlayTried = true;

	if (!backend.CreateOverlay("obvr.hud", "Oblivion HUD", m_overlay)) {
		return false;
	}

	// Straight ahead of the head, at the configured distance. Identity
	// rotation, and the translation is negative Z because that is forward in
	// the headset's own frame.
	vr::openvr::HmdMatrix34 hmdToOverlay{};
	hmdToOverlay.m[0][0] = 1.0f;
	hmdToOverlay.m[1][1] = 1.0f;
	hmdToOverlay.m[2][2] = 1.0f;
	hmdToOverlay.m[2][3] = -distanceMetres;

	backend.SetOverlayTransformHmdRelative(m_overlay, hmdToOverlay);
	backend.SetOverlayWidthInMetres(m_overlay, widthMetres);
	return true;
}

void HudLayer::Submit(vr::OpenVRBackend& backend, void* gameDevice, bool captured,
                      float distanceMetres, float widthMetres, bool probeSquare) {
	// Consumed either way; the next frame's capture decides afresh.
	const bool haveCapture = captured && m_captured;
	m_captured = false;

	if (!haveCapture) {
		if (m_overlayVisible && m_overlay != vr::openvr::kOverlayHandleInvalid) {
			backend.HideOverlay(m_overlay);
			m_overlayVisible = false;
		}
		return;
	}

	if (!EnsureOverlay(backend, distanceMetres, widthMetres)) {
		return;
	}

	if (!m_vulkanChecked) {
		m_vulkanChecked = true;
		m_vulkanUsable = GetVulkanContext(gameDevice, m_vulkan);
	}
	if (!m_vulkanUsable) {
		return;
	}

	// The probe: an opaque red square, 256 pixels, dead centre, painted after
	// everything the game drew. If it shows in the headset, the overlay path
	// is fine and the layer's own alpha is what never arrived; if it does
	// not, the display path itself is at fault and the alpha was never the
	// question.
	if (probeSquare && m_width > 512 && m_height > 512) {
		auto colorFill = d3d9::Method<d3d9::ColorFillFn>(gameDevice, d3d9::kDeviceColorFill);
		if (colorFill != nullptr) {
			const SInt32 cx = static_cast<SInt32>(m_width) / 2;
			const SInt32 cy = static_cast<SInt32>(m_height) / 2;
			const d3d9::Rect square{cx - 128, cy - 128, cx + 128, cy + 128};
			colorFill(gameDevice, m_surface, &square, 0xFFFF0000u);
		}
	}

	// The same bracket the eyes go through, because the requirement is the
	// same shape: the compositor reads the image, so outstanding commands
	// are flushed, the queue is held, and the layout is TRANSFER_SRC while
	// it looks. That the overlay path wants exactly what Submit wants is an
	// assumption stated here rather than smuggled: both take the same
	// Texture_t, and the next in-game log is what confirms it.
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
			OBVR_LOG("Hud: SetOverlayTexture failed (%d), so the HUD stays hidden", error);
		}
		return;
	}

	if (!m_overlayVisible) {
		backend.ShowOverlay(m_overlay);
		m_overlayVisible = true;
	}

	if (!m_liveReported) {
		m_liveReported = true;
		OBVR_LOG("Hud: live - the 2D layer hangs %.2f m ahead, %.2f m wide",
		         static_cast<double>(distanceMetres), static_cast<double>(widthMetres));
	}
}

void HudLayer::Destroy() {
	m_bracket.Release();

	d3d11::Release(m_interop);
	d3d11::Release(m_surface);
	d3d11::Release(m_texture);
	m_interop = nullptr;
	m_surface = nullptr;
	m_texture = nullptr;
	m_image = BackBufferImage{};
	m_width = 0;
	m_height = 0;
	m_textureTried = false;
	m_statesSaved = false;
	m_statesReported = false;
	m_captured = false;

	// The overlay handle is deliberately left to the runtime: DestroyOverlay
	// from DllMain-adjacent shutdown would call into a library that may be
	// tearing down, and SteamVR reclaims a process's overlays on exit anyway.
	m_overlay = vr::openvr::kOverlayHandleInvalid;
	m_overlayTried = false;
	m_overlayVisible = false;
	m_vulkanChecked = false;
	m_vulkanUsable = false;
}

}  // namespace obvr::render
