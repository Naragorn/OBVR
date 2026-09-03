#include "render/HudLayer.h"

#include <cmath>

#include "core/Log.h"
#include "render/D3D11Types.h"
#include "render/EyeGeometry.h"
#include "render/GameFrame.h"
#include "render/ResolutionHook.h"
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

void* HudLayer::BeginCapture(void* gameDevice, UInt32 frameNumber) {
	if (!EnsureTexture(gameDevice)) {
		m_captured = false;
		return nullptr;
	}

	// Transparent, then drawn on - once per frame. The pass runs more than
	// once in a frame, and every call after the first has to accumulate onto
	// what the earlier calls drew, not onto a fresh clear: clearing per call
	// is how twenty-two counted draws became a texture of nothing.
	//
	// Without any clear at all, last frame's HUD would show through wherever
	// this frame draws nothing - which is most of it.
	if (!m_everCleared || m_lastClearFrame != frameNumber) {
		auto colorFill =
			d3d9::Method<d3d9::ColorFillFn>(gameDevice, d3d9::kDeviceColorFill);
		if (colorFill == nullptr ||
		    d3d11::Failed(colorFill(gameDevice, m_surface, nullptr, kTransparentBlack))) {
			m_captured = false;
			return nullptr;
		}
		m_everCleared = true;
		m_lastClearFrame = frameNumber;
		m_captured = false;
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
	// mask without the 0x8 bit is a HUD whose alpha never left zero, and a
	// z test enabled against a depth buffer nobody cleared would reject
	// every draw without an error anywhere.
	if (!m_statesReported) {
		m_statesReported = true;
		UInt32 zEnable = 0;
		UInt32 alphaBlend = 0;
		UInt32 alphaTest = 0;
		UInt32 scissor = 0;
		getState(gameDevice, 7, &zEnable);      // D3DRS_ZENABLE
		getState(gameDevice, 27, &alphaBlend);  // D3DRS_ALPHABLENDENABLE
		getState(gameDevice, 15, &alphaTest);   // D3DRS_ALPHATESTENABLE
		getState(gameDevice, 174, &scissor);    // D3DRS_SCISSORTESTENABLE
		OBVR_LOG("Hud: states before the pass: separate=%u srcA=%u dstA=%u op=%u "
		         "writeMask=0x%X z=%u blend=%u alphaTest=%u scissor=%u",
		         m_savedStates[0], m_savedStates[1], m_savedStates[2], m_savedStates[3],
		         m_savedStates[4], zEnable, alphaBlend, alphaTest, scissor);
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

	// The head-relative placement, which is also what a world-anchored
	// overlay falls back to until an anchor pose can be read: straight ahead
	// of the head at the configured distance, identity rotation, negative Z
	// because that is forward in the headset's own frame.
	vr::openvr::HmdMatrix34 hmdToOverlay{};
	hmdToOverlay.m[0][0] = 1.0f;
	hmdToOverlay.m[1][1] = 1.0f;
	hmdToOverlay.m[2][2] = 1.0f;
	hmdToOverlay.m[2][3] = -distanceMetres;

	backend.SetOverlayTransformHmdRelative(m_overlay, hmdToOverlay);
	backend.SetOverlayWidthInMetres(m_overlay, widthMetres);

	// The slice of the layer texture the 2D actually draws in. An earlier
	// crop to the believed size was wrong and swallowed the HUD, because the
	// UI then drew across the full buffer; since the screen-size copy is
	// raised (UiScreenSize.h), the UI draws exactly into the believed
	// rectangle - the raise is what MAKES the believed size true - and the
	// texture below and right of it holds nothing. The bounds also set the
	// quad's aspect, which is what makes menus hang as a cinema screen
	// rather than a square. When belief and texture agree, no bounds are
	// set and the overlay shows everything, exactly as before.
	UInt32 believedWidth = 0;
	UInt32 believedHeight = 0;
	if (GameBelievedSize(believedWidth, believedHeight)) {
		const TextureBounds content =
			ContentBounds(believedWidth, believedHeight, m_width, m_height);
		if (content.uMax < 1.0f || content.vMax < 1.0f) {
			const vr::openvr::VRTextureBounds bounds{content.uMin, content.vMin,
			                                         content.uMax, content.vMax};
			backend.SetOverlayTextureBounds(m_overlay, bounds);
			OBVR_LOG("Hud: the overlay shows u=0..%.3f v=0..%.3f of the layer - the "
			         "%ux%u the 2D lives in, out of %ux%u",
			         static_cast<double>(content.uMax), static_cast<double>(content.vMax),
			         believedWidth, believedHeight, m_width, m_height);
		}
	}
	return true;
}

// An anchor further from the head than this is an anchor taken where the
// wearer no longer is. Two metres is past any lean and short of any walk:
// nothing in seated or standing play moves the head that far from where the
// HUD was placed, so crossing it means the anchor was wrong to begin with -
// the pose the runtime handed out before the headset was on anyone's head.
// That is not a hypothetical: on the Dream Air the HUD and every menu were
// "missing" for a whole session because the first readable pose anchored
// them somewhere the wearer never looked.
constexpr float kAnchorReachMetres = 2.0f;

// Places a world-anchored overlay, taking the anchor the first time it can
// and retaking it whenever the head turns out to be beyond reach of it.
//
// The anchor is a pose held rather than read each frame, and that is the
// whole difference between the two anchorings: handing over the current head
// pose every frame is exactly what head-relative already does, through the
// compositor and better. Held and levelled, the quad stays in the room and
// the head moves against it.
//
// Until a pose can be read - tracking not up yet - the overlay keeps the
// head-relative transform it was created with. A HUD riding the head is a
// far better wrong answer than one nailed to wherever the runtime guessed
// the origin was.
void HudLayer::SetWristPlacement(UInt32 deviceIndex,
                                 const vr::openvr::HmdMatrix34& deviceToOverlay,
                                 float widthMetres) {
	if (m_wristSet && m_wristWidth != widthMetres) {
		m_wristApplied = false;  // a new width to apply
	}
	m_wristSet = true;
	m_wristDevice = deviceIndex;
	m_wristTransform = deviceToOverlay;
	m_wristWidth = widthMetres;
}

void HudLayer::ShownPixels(float& width, float& height) const {
	UInt32 believedWidth = 0;
	UInt32 believedHeight = 0;
	if (GameBelievedSize(believedWidth, believedHeight) && believedWidth <= m_width &&
	    believedHeight <= m_height) {
		width = static_cast<float>(believedWidth);
		height = static_cast<float>(believedHeight);
		return;
	}
	width = static_cast<float>(m_width);
	height = static_cast<float>(m_height);
}

void HudLayer::PlaceInRoom(vr::OpenVRBackend& backend, float distanceMetres) {
	vr::openvr::HmdMatrix34 current{};
	if (!backend.GetRenderPoseMatrix(current)) {
		return;
	}

	if (m_anchorValid) {
		const float distSq = vr::PoseDistanceSq(current, m_anchorPose);
		if (distSq <= kAnchorReachMetres * kAnchorReachMetres) {
			return;
		}
		// Out of reach: the anchor stands where the wearer is not. Dropped
		// and retaken below, where the head actually is - the self-heal for
		// an anchor taken off a pose that predates the headset being worn.
		m_anchorValid = false;
		if (m_reanchorsReported < 4) {
			++m_reanchorsReported;
			OBVR_LOG("Hud: the room anchor sat %.1f metres from the head - retaken "
			         "where the head is now",
			         std::sqrt(static_cast<double>(distSq)));
		}
	}

	m_anchorPose = current;
	m_anchorValid = true;

	// Heading only, for the reasons LevelPose gives: a quad carrying the
	// pitch and roll the head happened to have hangs crooked for as long
	// as the anchor stands.
	vr::LevelPose(m_anchorPose);

	const vr::openvr::HmdMatrix34 placed =
		vr::OverlayPoseAhead(m_anchorPose, distanceMetres);
	backend.SetOverlayTransformAbsolute(m_overlay, placed);

	if (!m_anchorReported) {
		m_anchorReported = true;
		OBVR_LOG("Hud: the layer is anchored in the room at (%.2f, %.2f, %.2f) - "
		         "the recenter key moves it",
		         static_cast<double>(placed.m[0][3]),
		         static_cast<double>(placed.m[1][3]),
		         static_cast<double>(placed.m[2][3]));
	}
}

void HudLayer::Submit(vr::OpenVRBackend& backend, void* gameDevice, bool captured,
                      float distanceMetres, float widthMetres, bool anchorWorld,
                      bool probeSquare) {
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

	if (m_wristSet) {
		// On the wrist: the transform every frame, because the device index
		// can change when a controller is swapped, and the width once per
		// change of wrist. The room anchor is marked stale so leaving the
		// wrist puts the head-relative transform back.
		backend.SetOverlayTransformDeviceRelative(m_overlay, m_wristDevice, m_wristTransform);
		if (!m_wristApplied) {
			m_wristApplied = true;
			backend.SetOverlayWidthInMetres(m_overlay, m_wristWidth);
		}
		m_anchorValid = true;
	} else if (m_wristApplied) {
		m_wristApplied = false;
		backend.SetOverlayWidthInMetres(m_overlay, widthMetres);
	}

	if (m_wristSet) {
		// placed above
	} else if (anchorWorld) {
		PlaceInRoom(backend, distanceMetres);
	} else if (m_anchorValid) {
		// Switched back to the head while the game runs. The overlay is
		// still carrying an absolute transform, so it has to be given the
		// head-relative one again or it stays hanging in the room.
		vr::openvr::HmdMatrix34 hmdToOverlay{};
		hmdToOverlay.m[0][0] = 1.0f;
		hmdToOverlay.m[1][1] = 1.0f;
		hmdToOverlay.m[2][2] = 1.0f;
		hmdToOverlay.m[2][3] = -distanceMetres;
		backend.SetOverlayTransformHmdRelative(m_overlay, hmdToOverlay);
		m_anchorValid = false;
	}

	if (!m_vulkanChecked) {
		m_vulkanChecked = true;
		m_vulkanUsable = GetVulkanContext(gameDevice, m_vulkan);
	}
	if (!m_vulkanUsable) {
		return;
	}

	// The readback before the square, so the square's own opaque pixels do
	// not pollute the numbers being measured.
	if (probeSquare) {
		++m_captureCount;
		if (m_captureCount == 100) {
			DumpContentOnce(gameDevice);
		}
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

void HudLayer::DumpContentOnce(void* gameDevice) {
	if (m_contentDumped || m_surface == nullptr || m_width == 0 || m_height == 0) {
		return;
	}
	m_contentDumped = true;

	// A render target cannot be read by the CPU; GetRenderTargetData copies
	// it into a system memory surface that can. The copy stalls the GPU, which
	// is why this runs exactly once, and only while the probe is on.
	auto createPlain = d3d9::Method<d3d9::CreateOffscreenPlainSurfaceFn>(
		gameDevice, d3d9::kDeviceCreateOffscreenPlainSurface);
	auto getData = d3d9::Method<d3d9::GetRenderTargetDataFn>(
		gameDevice, d3d9::kDeviceGetRenderTargetData);
	if (createPlain == nullptr || getData == nullptr) {
		return;
	}

	void* staging = nullptr;
	if (d3d11::Failed(createPlain(gameDevice, m_width, m_height, d3d9::kFormatA8R8G8B8,
	                              d3d9::kPoolSystemMem, &staging, nullptr)) ||
	    staging == nullptr) {
		OBVR_LOG("Hud: no staging surface for the content dump");
		return;
	}

	if (d3d11::Failed(getData(gameDevice, m_surface, staging))) {
		OBVR_LOG("Hud: GetRenderTargetData refused the content dump");
		d3d11::Release(staging);
		return;
	}

	auto lockRect = d3d9::Method<d3d9::LockRectFn>(staging, d3d9::kSurfaceLockRect);
	auto unlockRect = d3d9::Method<d3d9::UnlockRectFn>(staging, d3d9::kSurfaceUnlockRect);
	d3d9::LockedRect locked{};
	if (lockRect == nullptr || unlockRect == nullptr ||
	    d3d11::Failed(lockRect(staging, &locked, nullptr, d3d9::kLockReadOnly)) ||
	    locked.bits == nullptr) {
		OBVR_LOG("Hud: the staging surface could not be locked");
		d3d11::Release(staging);
		return;
	}

	// The four counts that decide the diagnosis. colouredNoAlpha is the
	// smoking gun for "the content is there and its alpha is not": pixels
	// with visible colour sitting at alpha zero.
	UInt32 alphaZero = 0;
	UInt32 alphaFull = 0;
	UInt32 alphaMid = 0;
	UInt32 colouredNoAlpha = 0;
	UInt32 maxAlpha = 0;

	for (UInt32 y = 0; y < m_height; ++y) {
		const auto* row = reinterpret_cast<const UInt32*>(
			static_cast<const UInt8*>(locked.bits) +
			static_cast<SInt32>(y) * locked.pitch);
		for (UInt32 x = 0; x < m_width; ++x) {
			const UInt32 pixel = row[x];
			const UInt32 alpha = pixel >> 24;
			if (alpha == 0) {
				++alphaZero;
				// "Visible" means any channel above the noise floor.
				if ((pixel & 0x00F0F0F0u) != 0) {
					++colouredNoAlpha;
				}
			} else if (alpha == 255) {
				++alphaFull;
			} else {
				++alphaMid;
			}
			if (alpha > maxAlpha) {
				maxAlpha = alpha;
			}
		}
	}

	unlockRect(staging);
	d3d11::Release(staging);

	const UInt32 total = m_width * m_height;
	OBVR_LOG("Hud: content dump of %ux%u - alpha 0: %u, alpha 255: %u, between: %u, "
	         "max alpha %u",
	         m_width, m_height, alphaZero, alphaFull, alphaMid, maxAlpha);
	OBVR_LOG("Hud: %u of %u pixels carry colour at alpha zero%s", colouredNoAlpha, total,
	         colouredNoAlpha > total / 100
	             ? " - the layer is there and its alpha is not"
	             : (alphaFull + alphaMid == 0
	                    ? " - and nothing carries alpha, so the layer may not be here at all"
	                    : ""));
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
	m_captureCount = 0;
	m_contentDumped = false;

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
