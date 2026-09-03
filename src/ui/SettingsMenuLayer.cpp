#include "ui/SettingsMenuLayer.h"

#include "core/Log.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"
#include "render/GameFrame.h"
#include "ui/MenuCanvas.h"

namespace obvr::ui {

// The render layer's own namespaces, spelled once here rather than at every
// use. This file lives in obvr::ui but talks Direct3D throughout, and
// render::d3d9:: on every line would bury what the lines actually do.
namespace d3d9 = render::d3d9;
namespace d3d11 = render::d3d11;
namespace dxvk = render::dxvk;

namespace {

// The texture the menu is drawn into.
//
// Wide rather than square: a settings menu is rows of text, so the shape that
// wastes the least is the shape of the text. Big enough that the five-pixel
// font can be drawn at a scale of three and still leave room for twenty rows,
// which is what makes it readable at arm's length without the letters becoming
// blocks.
constexpr UInt32 kTextureWidth = 1024;
constexpr UInt32 kTextureHeight = 768;

// How many screen pixels one font pixel becomes. See Canvas::DrawText for why
// this has to be a whole number.
constexpr UInt32 kMenuScale = 3;

// The palette, with red and blue already exchanged.
//
// The canvas writes Pixel as it is given, and Direct3D 9's A8R8G8B8 is blue
// first in memory while Pixel is red first. Swapping the handful of colours in
// the theme once is the cheap way round that; swapping every pixel of the
// finished picture would be a second pass over the whole texture on every
// repaint. See SwapRedAndBlue.
MenuTheme MakeTheme() {
	MenuTheme theme;
	theme.background = SwapRedAndBlue(theme.background);
	theme.frame = SwapRedAndBlue(theme.frame);
	theme.title = SwapRedAndBlue(theme.title);
	theme.category = SwapRedAndBlue(theme.category);
	theme.text = SwapRedAndBlue(theme.text);
	theme.value = SwapRedAndBlue(theme.value);
	theme.highlight = SwapRedAndBlue(theme.highlight);
	theme.highlightText = SwapRedAndBlue(theme.highlightText);
	theme.help = SwapRedAndBlue(theme.help);
	theme.warning = SwapRedAndBlue(theme.warning);
	return theme;
}

}  // namespace

UInt32 SettingsMenuLayer::VisibleRows() const {
	return VisibleRowsFor(kTextureHeight, kMenuScale);
}

bool SettingsMenuLayer::EnsureTexture(void* gameDevice) {
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

	// DYNAMIC rather than the crosshair's RENDERTARGET, and that is the whole
	// difference between the two layers. A render target in the default pool
	// cannot be locked, and this texture's contents come from the processor
	// rather than from a copy - there is no surface anywhere holding a picture
	// of OBVR's own menu to copy from.
	if (d3d11::Failed(createTexture(gameDevice, kTextureWidth, kTextureHeight, 1,
	                                d3d9::kUsageDynamic, d3d9::kFormatA8R8G8B8,
	                                d3d9::kPoolDefault, &m_texture, nullptr)) ||
	    m_texture == nullptr) {
		m_texture = nullptr;
		OBVR_LOG("Menu: no %ux%u dynamic A8R8G8B8 texture, so the settings menu cannot be "
		         "drawn",
		         kTextureWidth, kTextureHeight);
		return false;
	}

	auto getSurfaceLevel =
		d3d9::Method<d3d9::GetSurfaceLevelFn>(m_texture, d3d9::kTextureGetSurfaceLevel);
	if (getSurfaceLevel == nullptr ||
	    d3d11::Failed(getSurfaceLevel(m_texture, 0, &m_surface)) || m_surface == nullptr) {
		m_surface = nullptr;
		OBVR_LOG("Menu: the texture has no surface to lock, so the settings menu cannot be "
		         "drawn");
		return false;
	}

	auto* unknown = static_cast<d3d11::Unknown*>(m_texture);
	if (unknown->vtbl == nullptr || unknown->vtbl->QueryInterface == nullptr ||
	    d3d11::Failed(unknown->vtbl->QueryInterface(unknown, &render::kIID_D3D9VkInteropTexture,
	                                                &m_interop)) ||
	    m_interop == nullptr) {
		m_interop = nullptr;
		OBVR_LOG("Menu: the texture is not a DXVK interop texture, so it cannot reach the "
		         "compositor");
		return false;
	}

	return true;
}

bool SettingsMenuLayer::EnsureOverlay(vr::OpenVRBackend& backend) {
	if (m_overlay != vr::openvr::kOverlayHandleInvalid) {
		return true;
	}
	if (m_overlayTried) {
		return false;
	}
	m_overlayTried = true;

	return backend.CreateOverlay(m_overlayKey, m_overlayName, m_overlay);
}

bool SettingsMenuLayer::Repaint(const MenuItem* items, const char* const* categories, UInt32 count,
                                MenuState state) {
	if (m_surface == nullptr) {
		return false;
	}

	auto lockRect = d3d9::Method<d3d9::LockRectFn>(m_surface, d3d9::kSurfaceLockRect);
	auto unlockRect = d3d9::Method<d3d9::UnlockRectFn>(m_surface, d3d9::kSurfaceUnlockRect);
	if (lockRect == nullptr || unlockRect == nullptr) {
		return false;
	}

	d3d9::LockedRect locked{};
	if (d3d11::Failed(lockRect(m_surface, &locked, nullptr, 0)) || locked.bits == nullptr) {
		if (!m_lockFailureReported) {
			m_lockFailureReported = true;
			OBVR_LOG("Menu: the texture would not lock, so the settings menu stays empty. A "
			         "dynamic texture in the default pool is the documented way to do this, "
			         "so this is a driver answer rather than a wrong request.");
		}
		return false;
	}

	// The pitch is the driver's, not ours. It is in bytes and is usually wider
	// than the picture; handing the canvas a stride in pixels is what keeps the
	// rows from shearing. A pitch that is not a whole number of pixels would
	// mean a format this code did not ask for, so it is refused rather than
	// rounded.
	const UInt32 pitch = static_cast<UInt32>(locked.pitch < 0 ? -locked.pitch : locked.pitch);
	if (pitch % sizeof(render::Pixel) != 0) {
		unlockRect(m_surface);
		if (!m_lockFailureReported) {
			m_lockFailureReported = true;
			OBVR_LOG("Menu: the locked pitch of %u is not a whole number of pixels, which is "
			         "not a format this asked for - the menu stays empty rather than drawing "
			         "a sheared picture",
			         pitch);
		}
		return false;
	}

	Canvas canvas(static_cast<render::Pixel*>(locked.bits), kTextureWidth, kTextureHeight,
	              pitch / static_cast<UInt32>(sizeof(render::Pixel)));

	static const MenuTheme theme = MakeTheme();
	PaintMenu(canvas, items, categories, count, state, kMenuScale, theme, m_title);

	unlockRect(m_surface);
	return true;
}

void SettingsMenuLayer::Place(vr::OpenVRBackend& backend, float distanceMetres, float widthMetres,
                              bool inWorld) {
	// The width can be set whenever it changes; the position is a separate
	// question, because it is taken once and then left alone.
	if (!m_placed || m_placedWidth != widthMetres) {
		backend.SetOverlayWidthInMetres(m_overlay, widthMetres);
		m_placedWidth = widthMetres;
	}

	// Carried on the head, because the setting says so. Reasserted every frame
	// rather than once: an hmd-relative transform is a standing instruction and
	// costs a call, but it is the only way a change of this setting from the
	// menu itself takes effect without closing and reopening.
	if (!inWorld) {
		vr::openvr::HmdMatrix34 hmdToOverlay{};
		hmdToOverlay.m[0][0] = 1.0f;
		hmdToOverlay.m[1][1] = 1.0f;
		hmdToOverlay.m[2][2] = 1.0f;
		hmdToOverlay.m[2][3] = -distanceMetres;
		backend.SetOverlayTransformHmdRelative(m_overlay, hmdToOverlay);

		// Not counted as placed, so switching back to the room takes a fresh
		// anchor rather than reusing one from before the menu was ever opened.
		m_placed = false;
		return;
	}

	// Anchored in the room, once, where the head was when the menu opened -
	// rather than carried on the face.
	//
	// A panel fixed to the head cannot be looked at. Every attempt to read the
	// row below the one in the middle moves the row below it, the eyes have
	// nothing to converge on that holds still, and the whole thing reads as a
	// smear that follows you. Left standing in the room it becomes an object:
	// lean in to read it, turn away and it stays where you put it, look back
	// and it is still there.
	//
	// Heading only, for the reason LevelPose gives - a quad carrying whatever
	// pitch and roll the head happened to have when the key was pressed hangs
	// crooked for as long as it stands.
	if (m_placed && m_placedDistance == distanceMetres) {
		return;
	}

	vr::openvr::HmdMatrix34 pose{};
	if (backend.GetRenderPoseMatrix(pose)) {
		vr::LevelPose(pose);
		backend.SetOverlayTransformAbsolute(m_overlay,
		                                    vr::OverlayPoseAhead(pose, distanceMetres));
		m_placed = true;
		m_placedDistance = distanceMetres;
		return;
	}

	// No pose to anchor to. That happens during the intro films, where
	// WaitGetPoses has not run yet - and a settings menu that cannot appear
	// there is a settings menu that cannot fix whatever made the films
	// unwatchable. So it falls back to the face, which is worse to read but is
	// present, and the next open takes a real anchor.
	vr::openvr::HmdMatrix34 hmdToOverlay{};
	hmdToOverlay.m[0][0] = 1.0f;
	hmdToOverlay.m[1][1] = 1.0f;
	hmdToOverlay.m[2][2] = 1.0f;
	hmdToOverlay.m[2][3] = -distanceMetres;
	backend.SetOverlayTransformHmdRelative(m_overlay, hmdToOverlay);
}

void SettingsMenuLayer::Submit(vr::OpenVRBackend& backend, void* gameDevice, bool visible,
                               const MenuItem* items, const char* const* categories, UInt32 count,
                               MenuState state, UInt32 revision, float distanceMetres,
                               float widthMetres, bool inWorld) {
	if (!visible) {
		if (m_overlayVisible) {
			backend.HideOverlay(m_overlay);
			m_overlayVisible = false;
		}

		// The anchor is dropped on the way out, so the next open takes a fresh
		// one where the head is then. Keeping it would leave the menu standing
		// wherever it was last opened - which, after walking away, means
		// pressing the key and seeing nothing at all.
		m_placed = false;
		return;
	}

	if (!EnsureTexture(gameDevice) || !EnsureOverlay(backend)) {
		return;
	}

	if (!m_vulkanChecked) {
		m_vulkanChecked = true;
		m_vulkanUsable = render::GetVulkanContext(gameDevice, m_vulkan);
	}
	if (!m_vulkanUsable) {
		return;
	}

	// Only when the picture would differ. Between keypresses a settings menu is
	// a still image, and locking and repainting three quarters of a million
	// pixels to produce the identical one is a cost paid for nothing.
	if (!m_painted || m_paintedRevision != revision) {
		if (!Repaint(items, categories, count, state)) {
			return;
		}
		m_painted = true;
		m_paintedRevision = revision;
	}

	Place(backend, distanceMetres, widthMetres, inWorld);

	if (!render::ReadImageInfo(m_interop, m_image)) {
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
	render::DescribeForOpenVR(m_image, m_vulkan, data);
	const int error = backend.SetOverlayTexture(m_overlay, &data);

	m_bracket.Release();

	if (error != vr::openvr::kOverlayErrorNone) {
		if (!m_failureReported) {
			m_failureReported = true;
			OBVR_LOG("Menu: SetOverlayTexture failed (%d), so the settings menu stays hidden",
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
		OBVR_LOG("Menu: the settings menu is live - %ux%u at scale %u, %u rows, %.2f m ahead "
		         "and %.2f m wide",
		         kTextureWidth, kTextureHeight, kMenuScale, VisibleRows(),
		         static_cast<double>(distanceMetres), static_cast<double>(widthMetres));
	}
}

void SettingsMenuLayer::Destroy() {
	m_bracket.Release();

	if (m_interop != nullptr) {
		auto* unknown = static_cast<d3d11::Unknown*>(m_interop);
		if (unknown->vtbl != nullptr && unknown->vtbl->Release != nullptr) {
			unknown->vtbl->Release(unknown);
		}
		m_interop = nullptr;
	}

	if (m_surface != nullptr) {
		auto* unknown = static_cast<d3d11::Unknown*>(m_surface);
		if (unknown->vtbl != nullptr && unknown->vtbl->Release != nullptr) {
			unknown->vtbl->Release(unknown);
		}
		m_surface = nullptr;
	}

	if (m_texture != nullptr) {
		auto* unknown = static_cast<d3d11::Unknown*>(m_texture);
		if (unknown->vtbl != nullptr && unknown->vtbl->Release != nullptr) {
			unknown->vtbl->Release(unknown);
		}
		m_texture = nullptr;
	}

	// The overlay handle is left to the runtime, for the reason HudLayer's own
	// Destroy records.
	m_overlay = vr::openvr::kOverlayHandleInvalid;
	m_overlayVisible = false;
	m_painted = false;
}

}  // namespace obvr::ui
