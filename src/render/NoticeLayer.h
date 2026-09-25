#pragma once

#include "core/Types.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::render {

// One line of text hanging above the middle of the view: the update notice.
//
// Its own overlay with raw pixels, painted once on the CPU with the menu font,
// the way the laser dot is. Head-locked, so it is seen wherever one looks in
// the main menu, and above everything else OBVR draws.
class NoticeLayer {
public:
	~NoticeLayer() { Destroy(); }

	NoticeLayer() = default;
	NoticeLayer(const NoticeLayer&) = delete;
	NoticeLayer& operator=(const NoticeLayer&) = delete;

	// Once per frame. The text is painted when it first shows and whenever it
	// changes; an empty or null text hides the notice.
	void Submit(vr::OpenVRBackend& backend, bool visible, const char* text);

	void Destroy();

	static constexpr UInt32 kTextureWidth = 480;
	static constexpr UInt32 kTextureHeight = 40;

private:
	bool EnsureOverlay(vr::OpenVRBackend& backend);

	vr::openvr::VROverlayHandle m_overlay = vr::openvr::kOverlayHandleInvalid;
	bool m_overlayTried = false;
	bool m_overlayVisible = false;
	char m_painted[96] = {};
};

}  // namespace obvr::render
