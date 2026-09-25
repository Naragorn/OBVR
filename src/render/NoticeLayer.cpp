#include "render/NoticeLayer.h"

#include "core/Log.h"
#include "render/TestPattern.h"
#include "ui/MenuCanvas.h"
#include "ui/MenuFont.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {
namespace {

// Above the laser (10, 11) and the crosshair (5): a notice under a menu quad
// could not be read.
constexpr UInt32 kNoticeSortOrder = 20;
constexpr float kNoticeDistanceMetres = 1.5f;
constexpr float kNoticeWidthMetres = 0.9f;
// Above the middle of the view, clear of the main menu's buttons.
constexpr float kNoticeHeightMetres = 0.35f;
constexpr SInt32 kTextScale = 2;

bool SameText(const char* a, const char* b) {
	while (*a != '\0' && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

}  // namespace

bool NoticeLayer::EnsureOverlay(vr::OpenVRBackend& backend) {
	if (m_overlay != vr::openvr::kOverlayHandleInvalid) {
		return true;
	}
	if (m_overlayTried) {
		return false;
	}
	m_overlayTried = true;
	if (!backend.CreateOverlay("obvr.notice", "OBVR notice", m_overlay)) {
		OBVR_LOG("Notice: the overlay could not be created - the update notice is not shown");
		return false;
	}
	backend.SetOverlaySortOrder(m_overlay, kNoticeSortOrder);
	backend.SetOverlayWidthInMetres(m_overlay, kNoticeWidthMetres);
	vr::openvr::HmdMatrix34 pose{};
	pose.m[0][0] = 1.0f;
	pose.m[1][1] = 1.0f;
	pose.m[2][2] = 1.0f;
	pose.m[1][3] = kNoticeHeightMetres;
	pose.m[2][3] = -kNoticeDistanceMetres;
	backend.SetOverlayTransformHmdRelative(m_overlay, pose);
	return true;
}

void NoticeLayer::Submit(vr::OpenVRBackend& backend, bool visible, const char* text) {
	if (!visible || text == nullptr || text[0] == '\0') {
		if (m_overlayVisible) {
			backend.HideOverlay(m_overlay);
			m_overlayVisible = false;
			OBVR_LOG("Notice: hidden");
		}
		return;
	}
	if (!EnsureOverlay(backend)) {
		return;
	}
	if (!SameText(m_painted, text)) {
		static Pixel pixels[kTextureWidth * kTextureHeight];
		ui::Canvas canvas(pixels, kTextureWidth, kTextureHeight);
		// The menus' parchment dark, a gold rim, the text in the laser's gold.
		canvas.Fill(Pixel{40, 28, 16, 225});
		canvas.DrawFrame(0, 0, kTextureWidth, kTextureHeight, 2, Pixel{200, 160, 90, 255});
		const SInt32 width = static_cast<SInt32>(ui::TextWidth(text)) * kTextScale;
		const SInt32 height = static_cast<SInt32>(ui::kGlyphHeight) * kTextScale;
		canvas.DrawText((static_cast<SInt32>(kTextureWidth) - width) / 2,
		                (static_cast<SInt32>(kTextureHeight) - height) / 2, text, kTextScale,
		                Pixel{255, 225, 150, 255});
		const int error = backend.SetOverlayRaw(m_overlay, pixels, kTextureWidth, kTextureHeight);
		if (error != vr::openvr::kOverlayErrorNone) {
			OBVR_LOG("Notice: SetOverlayRaw failed (%d) - the update notice is not shown", error);
			return;
		}
		UInt32 i = 0;
		for (; text[i] != '\0' && i + 1 < sizeof(m_painted); ++i) {
			m_painted[i] = text[i];
		}
		m_painted[i] = '\0';
	}
	if (!m_overlayVisible) {
		backend.ShowOverlay(m_overlay);
		m_overlayVisible = true;
		OBVR_LOG("Notice: shown - \"%s\"", text);
	}
}

void NoticeLayer::Destroy() {
	m_overlay = vr::openvr::kOverlayHandleInvalid;
	m_overlayVisible = false;
	m_painted[0] = '\0';
}

}  // namespace obvr::render
